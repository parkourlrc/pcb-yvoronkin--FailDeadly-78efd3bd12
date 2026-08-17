#include "crc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "message_buffer.h"
#include "mpu_wrappers.h"

#include "macros.h"
#include "fd_crc.h"

#include "stm32_assert.h"

// most messages will be small, additional 512 bytes make space for queue 
#define BUFFER_SIZE ((sizeof(crc_request_t) + sizeof(size_t)) + 512)

struct fd_crc_data
{
    StaticMessageBuffer_t StaticMessageBuffer;
    StaticSemaphore_t StaticMutexSemaphore;
    StaticSemaphore_t StaticSemaphore;

    MessageBufferHandle_t msg_buf;
    SemaphoreHandle_t send_mutex;
    SemaphoreHandle_t isEmpty;

    uint8_t buffer[BUFFER_SIZE];
};

static
struct fd_crc_data fd_crc_data
__attribute__((aligned(ROUND_UP_POW2(sizeof(struct fd_crc_data)))));


int32_t fd_crc32_compute(crc_request_t * data, uint16_t payload_len, uint32_t * crc32_result)
{
    int32_t ret; 
    size_t bytes_available;
    TickType_t start_tick_count, curr_tick_count,
               elapsed_tick_count, remaining_tick_count;

    data->reply_to = xTaskGetCurrentTaskHandle();
    start_tick_count = xTaskGetTickCount();

    if (xSemaphoreTake(fd_crc_data.send_mutex, CRC32_COMPUTE_TIME_WAIT) == pdTRUE)
    {
        curr_tick_count = xTaskGetTickCount();

        if (curr_tick_count >= start_tick_count)
        {
            elapsed_tick_count = curr_tick_count - start_tick_count;
        }
        else
        {
            elapsed_tick_count = (portMAX_DELAY - start_tick_count) + curr_tick_count + 1;
        }
        remaining_tick_count = CRC32_COMPUTE_TIME_WAIT - elapsed_tick_count;

        if ((int32_t)uxSemaphoreGetCount(fd_crc_data.isEmpty) == 1)
        {
            xSemaphoreTake(fd_crc_data.isEmpty, 0);
        }

        bytes_available = xMessageBufferSpacesAvailable(fd_crc_data.msg_buf);
        if ((bytes_available < payload_len) && (remaining_tick_count > 0))
        {
            if (xSemaphoreTake(fd_crc_data.isEmpty, remaining_tick_count) == pdPASS)
            {
                bytes_available = BUFFER_SIZE;
            }
        }

        if (bytes_available >= payload_len)
        {
            ret = xMessageBufferSend(fd_crc_data.msg_buf, 
                                     (void *)data, 
                                     payload_len + sizeof(TaskHandle_t),
                                     0);
        }
        else ret = -2;
        
        xSemaphoreGive(fd_crc_data.send_mutex);

        if (ret != 0) return ret;
        
        // wait untill crc value is ready 
        *crc32_result = ulTaskNotifyTakeIndexed(1, pdTRUE, CRC32_RESULT_TIME_WAIT);
        ret = 0;
    }
    else ret = -1;

    return ret;
}

static
uint32_t fd_crc32_hw_bytes(const void * address, size_t size)
{
    const volatile uint8_t *p = (const volatile uint8_t *)address;

    CRC->CR = CRC_CR_RESET;

    while (size >= 4u)
    {
        /* Peripheral processing, MSB-first */
        const uint32_t word = 
                ((uint32_t)p[0] << 24u) |
                ((uint32_t)p[1] << 16u) |
                ((uint32_t)p[2] <<  8u) |
                ((uint32_t)p[3] <<  0u);

        CRC->DR = word;

        p    += 4u;
        size -= 4u;
    }

    uint32_t crc = CRC->DR;

    // less than 4 byte, software processing
    while (size != 0)
    {
        crc ^= (uint32_t)(*p++) << 24u;

        for (unsigned bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 0x80000000u) != 0u)
            {
                crc = (crc << 1u) ^ 0x04C11DB7u;
            }
            else
            {
                crc <<= 1u;
            }
        }

        --size;
    }

    return crc;
}

void FDCRCTask(void * arg)
{
    int32_t ret;
    uint32_t crc_value;
    size_t message_len;
    uint8_t tempBuffer[sizeof(crc_request_t)] = {0};
    crc_request_t * crc_request = (crc_request_t *)&tempBuffer;

    (void)arg;

    while(1)
    {
        message_len = xMessageBufferReceive(fd_crc_data.msg_buf,
                                            tempBuffer,
                                            sizeof(tempBuffer),
                                            portMAX_DELAY);

        if (xMessageBufferIsEmpty(fd_crc_data.msg_buf) == pdTRUE)
        {
            ret = (int32_t)uxSemaphoreGetCount(fd_crc_data.isEmpty);
            if (ret == 0)
            {
                ret = xSemaphoreGive(fd_crc_data.isEmpty);
            }
        }
        
        if (message_len == 0u) continue;

        // Compute CRC32
        crc_value = fd_crc32_hw_bytes(
                        crc_request->data, 
                        message_len - sizeof(TaskHandle_t)
        );
        
        xTaskNotifyIndexed(
            crc_request->reply_to, 
            1, 
            crc_value, 
            eSetValueWithOverwrite); 
    }
}

void FDCRCGrantAccess(TaskHandle_t xTask)
{
    assert_param(xTask != NULL);
    assert_param(fd_crc_data.msg_buf != NULL);
    assert_param(fd_crc_data.send_mutex != NULL);
    assert_param(fd_crc_data.isEmpty != NULL);

    vGrantAccessToMessageBuffer(xTask, fd_crc_data.msg_buf);
    vGrantAccessToSemaphore(xTask, fd_crc_data.send_mutex);
    vGrantAccessToSemaphore(xTask, fd_crc_data.isEmpty);
}

void FDCRCInit(void)
{
    fd_crc_data.msg_buf = 
        xMessageBufferCreateStatic( BUFFER_SIZE, 
                                    fd_crc_data.buffer,
                                    &fd_crc_data.StaticMessageBuffer);
    
    assert_param(fd_crc_data.msg_buf != NULL);

    fd_crc_data.send_mutex = 
        xSemaphoreCreateMutexStatic(&fd_crc_data.StaticMutexSemaphore);

    assert_param(fd_crc_data.send_mutex != (SemaphoreHandle_t)NULL);

    fd_crc_data.isEmpty = 
        xSemaphoreCreateBinaryStatic(&fd_crc_data.StaticSemaphore);

    assert_param(fd_crc_data.isEmpty != (SemaphoreHandle_t)NULL);
}

#define CRC_STACK_DEPTH 128 + (VCP_DRV_BUFF_LEN / 4)
#define CRC_STACK_ALIGN ROUND_UP_POW2((CRC_STACK_DEPTH * 4))

#if (CRC_STACK_ALIGN > 2048)
#warn "VCP TX Stack take a lot of MPU region"
#endif

static 
PRIVILEGED_DATA StaticTask_t xFDCRCTaskBuffer;

static 
portSTACK_TYPE xCRCStack[ CRC_STACK_DEPTH ] 
__attribute__((aligned(CRC_STACK_ALIGN)));


void FDCRCTaskInit(void)
{
    BaseType_t res;

    const
    TaskParameters_t task_params = {
        FDCRCTask,
        "CRC",
        CRC_STACK_DEPTH,
        NULL,
        6 | portPRIVILEGE_BIT,
        xCRCStack,
        {
            { 0 },
        },
        &xFDCRCTaskBuffer
    };

    FDCRCInit();
    res = xTaskCreateRestrictedStatic(&task_params, NULL);
    assert_param(res == pdPASS);
}
