#ifndef FD_LOG_H
#define FD_LOG_H

#include "FreeRTOS.h"
#include "task.h"

#define LOG_BUFFER_LEN  160
#define LOG_SEND_MAX_WAIT (pdMS_TO_TICKS(100))
#define LOG_RTOS_TASK_INTERVAL (pdMS_TO_TICKS(60000U)) // ~ 1 min

void FDLogGrantAccess(TaskHandle_t xTask);
void FDLogTaskInit(void);
int32_t FDSendLog(const unsigned char * buf, size_t len, int omit_rtos, int noblock);
void FDLogFreeRTOSStateInit(void);

#endif
