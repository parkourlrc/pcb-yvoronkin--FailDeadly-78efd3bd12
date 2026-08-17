#include "gpio.h"

#include "FreeRTOS.h"
#include "task.h"

#include "test_task.h"
#include "vcp_comm.h"
#include "fd_log.h"

void BlinkTestTask(void *arg)
{
    const 
    char msg[] = "Smoke test\r\n";

    (void)arg;
    for (;;) {
        HAL_GPIO_TogglePin(GPIOC, debug_led_Pin);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static 
portSTACK_TYPE xTaskStack[ 128 ] 
__attribute__((aligned(128*4)));

extern const
MemoryRegion_t log_data_region;
extern const
MemoryRegion_t vcp_tx_data_region;

static 
PRIVILEGED_DATA StaticTask_t xBlinkTaskBuffer;

void BlinkTestTaskInit(void)
{
    TaskHandle_t xTask;

    const
    TaskParameters_t blink_param = {
        BlinkTestTask,
        "blink",
        128,
        NULL,
        tskIDLE_PRIORITY + 1,
        xTaskStack,
        {
        //    vcp_tx_data_region,
            log_data_region,
        },
        &xBlinkTaskBuffer,
    };

    BaseType_t res = xTaskCreateRestrictedStatic(&blink_param, &xTask);
    assert_param(res == pdPASS);

    //VCPTXGrantAccess(xTask);
}
