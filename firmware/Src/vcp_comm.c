#include "usbd_cdc_if.h"
#include "usb_device.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "semphr.h"

#include "macros.h"
#include "vcp_comm.h"
#include "fd_commands.h"
#include "fd_parser.h"
#include "fd_log.h"
#include "fd_crc.h"

#include "faildeadly_config.h"
#include "stm32_assert.h"

struct vcp_tx_data
{
    SemaphoreHandle_t   vcpStreamBufferSendMutex;
    SemaphoreHandle_t   vcpStreamBufferIsEmptySemaphore;
    StreamBufferHandle_t vcpTransmitStreamBuffer;

    uint8_t initialized;
};

struct vcp_tx_priv_data
{
    StaticSemaphore_t    StaticMutex;
    StaticSemaphore_t    StaticSemaphore;
    StaticStreamBuffer_t StaticBuffer;

    uint8_t buffer[VCP_DRV_BUFF_LEN + 1];
};

static 
PRIVILEGED_DATA StaticTask_t xVCPTXTaskBuffer;

static
struct vcp_tx_priv_data vcp_tx_priv_data;

static
struct vcp_tx_data vcp_tx_data
__attribute__((aligned(ROUND_UP_POW2(sizeof(struct vcp_tx_data)))));

const
MemoryRegion_t vcp_tx_data_region = {
    &vcp_tx_data,
    sizeof(vcp_tx_data),
    portMPU_REGION_READ_WRITE | 
    portMPU_REGION_EXECUTE_NEVER,
};

void VCPTXGrantAccess(TaskHandle_t xTask)
{
    assert_param(xTask != NULL);
    assert_param(vcp_tx_data.vcpTransmitStreamBuffer != NULL);
    assert_param(vcp_tx_data.vcpStreamBufferSendMutex != NULL);
    assert_param(vcp_tx_data.vcpStreamBufferIsEmptySemaphore != NULL);

    vGrantAccessToMessageBuffer(xTask, vcp_tx_data.vcpTransmitStreamBuffer);
    vGrantAccessToSemaphore(xTask, vcp_tx_data.vcpStreamBufferSendMutex);
    vGrantAccessToSemaphore(xTask, vcp_tx_data.vcpStreamBufferIsEmptySemaphore);
}

static 
void vcpInit(void)
{
    vcp_tx_data.vcpTransmitStreamBuffer = 
        xStreamBufferCreateStatic(VCP_DRV_BUFF_LEN, 
                                  1, 
                                  vcp_tx_priv_data.buffer, 
                                  &vcp_tx_priv_data.StaticBuffer);

    assert_param(vcp_tx_data.vcpTransmitStreamBuffer != NULL);

    vcp_tx_data.vcpStreamBufferSendMutex = 
        xSemaphoreCreateMutexStatic(&vcp_tx_priv_data.StaticMutex);

    assert_param(vcp_tx_data.vcpStreamBufferSendMutex != (SemaphoreHandle_t)NULL);

    vcp_tx_data.vcpStreamBufferIsEmptySemaphore = 
        xSemaphoreCreateBinaryStatic(&vcp_tx_priv_data.StaticSemaphore);

    assert_param(vcp_tx_data.vcpStreamBufferIsEmptySemaphore != (SemaphoreHandle_t) NULL);

    vcp_tx_data.initialized = 1;
    return;
}

extern 
USBD_HandleTypeDef hUsbDeviceFS;

void VCPTransmitTask(void * arg)
{
    int32_t ret;
    size_t numBytes;
    static uint8_t tempBuffer[VCP_DRV_BUFF_LEN];
    USBD_CDC_HandleTypeDef *hcdc = NULL;

    (void)arg;

    while (1)
    {
        hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        while (hcdc == NULL)
        {
            hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        // Wait forever for data to become available in the stream-buffer
        numBytes = xStreamBufferReceive(
                        vcp_tx_data.vcpTransmitStreamBuffer, tempBuffer,
                        VCP_DRV_BUFF_LEN, portMAX_DELAY);

        if (numBytes == 0u) continue;

        ret = (int32_t)uxSemaphoreGetCount(vcp_tx_data.vcpStreamBufferIsEmptySemaphore);
        if (ret == 0)
        {
            ret = xSemaphoreGive(vcp_tx_data.vcpStreamBufferIsEmptySemaphore);
        }

        // Transmit the data
        do 
        {
            ret = CDC_Transmit_FS(tempBuffer, numBytes);
            if (ret == USBD_BUSY) vTaskDelay(pdMS_TO_TICKS(1));
        }
        while( ret == USBD_BUSY);

        if (ret != USBD_OK)
        {
            char msg[] = "Something wrong with usb connection, can't transmit data.";
            FDSendLog((unsigned char*)msg, strlen(msg), 0, 0);
            continue; // something wrong with usb connection
        }

        // Wait for transmission completion
        while (hcdc->TxState != 0U)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

#define VCP_TX_STACK_DEPTH 128 + (VCP_DRV_BUFF_LEN / 4)
#define VCP_TX_STACK_ALIGN ROUND_UP_POW2((VCP_TX_STACK_DEPTH * 4))

#if (VCP_TX_STACK_ALIGN > 2048)
#warn "VCP TX Stack take a lot of MPU region"
#endif

static 
portSTACK_TYPE xVCPTXStack[ VCP_TX_STACK_DEPTH ] 
__attribute__((aligned(VCP_TX_STACK_ALIGN)));

void VCPTransmitTaskInit(void)
{
    BaseType_t res;

    const
    TaskParameters_t task_params = {
        VCPTransmitTask,
        "VCP TX",
        VCP_TX_STACK_DEPTH,
        NULL,
        6 | portPRIVILEGE_BIT,
        xVCPTXStack,
        {
            { 0 },
        },
        &xVCPTXTaskBuffer,
    };

    vcp_tx_data.initialized = 0;
    vcpInit();

    res = xTaskCreateRestrictedStatic(&task_params, NULL);
    assert_param(res == pdPASS);
}

int32_t vcpSend(const char * buf, uint16_t len)
{
    int32_t ret; 
    size_t bytes_available;

    TickType_t start_tick_count, cur_tick_count, 
               elapsed_tick_count, remaining_tick_count;
    
    while (vcp_tx_data.initialized == 0) vTaskDelay(1);

    start_tick_count = xTaskGetTickCount();
    
    if (xSemaphoreTake(vcp_tx_data.vcpStreamBufferSendMutex, VCP_DRV_SEND_MAX_WAIT) == pdPASS)
    {
        cur_tick_count = xTaskGetTickCount();

        // how long mutex taken
        if (cur_tick_count >= start_tick_count)
        {
            elapsed_tick_count = cur_tick_count - start_tick_count;
        }
        else
        {
            elapsed_tick_count = (portMAX_DELAY - start_tick_count) + cur_tick_count + 1;
        }
        remaining_tick_count = VCP_DRV_SEND_MAX_WAIT - elapsed_tick_count;

        // buff is empty
        if ((int32_t)uxSemaphoreGetCount(vcp_tx_data.vcpStreamBufferIsEmptySemaphore) == 1) 
        {
            // reset state
            xSemaphoreTake(vcp_tx_data.vcpStreamBufferIsEmptySemaphore, 0);
        }
        
        // get size available in buffer
        bytes_available = xStreamBufferSpacesAvailable(vcp_tx_data.vcpTransmitStreamBuffer);

        // if there is some waiting-time remaining wait until buffer will be emptied
        if ((bytes_available < len) && (remaining_tick_count > 0))
        {
            if (xSemaphoreTake(vcp_tx_data.vcpStreamBufferIsEmptySemaphore, 
                               remaining_tick_count) == pdPASS)
            {
                bytes_available = VCP_DRV_BUFF_LEN;
            }
        }

        if (bytes_available >= len)
        {
            ret = xStreamBufferSend(vcp_tx_data.vcpTransmitStreamBuffer, buf, len, 0);
        }
        // not enough space, return error
        else ret = pdFAIL;

        xSemaphoreGive(vcp_tx_data.vcpStreamBufferSendMutex);
    }
    else ret = -2;

    return ret;
}


#define VCP_RX_STREAM_BUF_LEN (VCP_DRV_BUFF_LEN * 2)

struct vcp_rx_priv_data
{
    StaticStreamBuffer_t StaticBuffer;
    uint8_t buffer[VCP_RX_STREAM_BUF_LEN];
};

struct vcp_rx_data
{
    StreamBufferHandle_t vcpReceiveStreamBuffer;
    uint8_t initialized;
};

static
struct vcp_rx_priv_data vcp_rx_priv_data;

static
struct vcp_rx_data vcp_rx_data
__attribute__((aligned(ROUND_UP_POW2(sizeof(struct vcp_rx_data)))));

const
MemoryRegion_t vcp_rx_data_region = {
    &vcp_rx_data,
    sizeof(vcp_rx_data),
    portMPU_REGION_READ_WRITE | 
    portMPU_REGION_EXECUTE_NEVER,
};

static
void VCPRXInit(void)
{
    vcp_rx_data.vcpReceiveStreamBuffer = 
        xStreamBufferCreateStatic(VCP_RX_STREAM_BUF_LEN, 
                                  1,
                                  vcp_rx_priv_data.buffer, 
                                  &vcp_rx_priv_data.StaticBuffer);

    assert_param(vcp_rx_data.vcpReceiveStreamBuffer != NULL);

    // allow FreeRTOS calls from USB ISR
    HAL_NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
    vcp_rx_data.initialized = 1;
}

static
void VCPReceiveTask(void * arg)
{
    size_t bytes_received, available;
    crc_request_t crc_request_data = {0};
    uint8_t *tempBuff = crc_request_data.data;

    struct comm_parser_state state = {
        .crc_req = &crc_request_data,
        .buf = tempBuff,
        .stage = COMM_PARSER_RESET,
        .max_len = 0,
        .msg_cur_end_pos = 0,
        .msg_timeout_ticks = pdMS_TO_TICKS(COMM_PARSER_MSG_TIMEOUT_MS),
    };
     
    (void)arg;

    _Static_assert(
        VCP_DRV_BUFF_LEN >= REQ_HEADER_LEN +
                        FD_COMM_DATA_IN_MAX_LEN +
                        CRC32_LEN,
        "VCP parser buffer cannot contain the largest frame"
    );

    while (1)
    {
        if (state.max_len > sizeof(tempBuff))
        {
            /* Internal parser-state corruption */

            // STAGE reset
            state.max_len = 0;
            state.stage = COMM_PARSER_RESET;
            state.msg_timeout_ticks = pdMS_TO_TICKS(COMM_PARSER_MSG_TIMEOUT_MS);
        }
        
        available = sizeof(tempBuff) - state.max_len;

        if (available != 0)
        {
            bytes_received = xStreamBufferReceive(
                                vcp_rx_data.vcpReceiveStreamBuffer,
                                &tempBuff[state.max_len], 
                                available,
                                pdMS_TO_TICKS(COMM_PARSER_POLL_MS)
            );
            state.msg_curr_ticks = xTaskGetTickCount();
        }
        else
        {
            /*
             * Nothing to wait for if no space left
             */
            state.stage = COMM_PARSER_RESET;
            bytes_received = 0;
        }

        // Even if bytes_received is 0, still need to check timeout
        comm_parser_process(&state, bytes_received);
    }
}

#define VCP_RX_STACK_DEPTH 128 + (VCP_DRV_BUFF_LEN / 4)
#define VCP_RX_STACK_ALIGN ROUND_UP_POW2((VCP_RX_STACK_DEPTH * 4))

#if (VCP_RX_STACK_ALIGN > 2048)
#warn "VCP RX Stack take a lot of MPU region"
#endif

extern
MemoryRegion_t log_data_region;

static 
PRIVILEGED_DATA StaticTask_t xVCPRXTaskBuffer;

static 
portSTACK_TYPE xVCPRXStack[ VCP_RX_STACK_DEPTH ] 
__attribute__((aligned(VCP_RX_STACK_ALIGN)));

static
void VCPReceiveGrantAccess(TaskHandle_t xTask)
{
    assert_param(xTask != NULL);
    assert_param(vcp_rx_data.vcpReceiveStreamBuffer != NULL);

    vGrantAccessToStreamBuffer(xTask, vcp_rx_data.vcpReceiveStreamBuffer);
}

void VCPReceiveTaskInit(void)
{
    BaseType_t res;
    TaskHandle_t task;

    const
    TaskParameters_t task_params = {
        VCPReceiveTask,
        "VCP RX",
        VCP_RX_STACK_DEPTH,
        NULL,
        6,
        xVCPRXStack,
        {
            vcp_rx_data_region,
            vcp_tx_data_region,
            log_data_region,
        },
        &xVCPRXTaskBuffer
    };

    vcp_rx_data.initialized = 0;
    VCPRXInit();

    res = xTaskCreateRestrictedStatic(&task_params, &task);
    assert_param(res == pdPASS);

    FDCRCGrantAccess(task);
    VCPReceiveGrantAccess(task);
}

int VCPReceiveRoutine(uint8_t* buf, uint32_t *len)
{
    size_t bytes_available;
    BaseType_t task_woken = pdFALSE;

    if (vcp_rx_data.initialized == 0) return 1; 
    
    // get size available in buffer
    bytes_available = xStreamBufferSpacesAvailable(vcp_rx_data.vcpReceiveStreamBuffer);

    if (bytes_available >= *len)
    {
        xStreamBufferSendFromISR(vcp_tx_data.vcpTransmitStreamBuffer, buf, 
                                 *len, &task_woken);

        // switch only after return from ISR
        portYIELD_FROM_ISR(task_woken);
    }
    // not enough space, return error
    else return 1; 

    return 0;
}
