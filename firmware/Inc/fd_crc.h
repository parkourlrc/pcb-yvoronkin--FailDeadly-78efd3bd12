#ifndef FD_CRC_H
#define FD_CRC_H

#include <stdint.h>

#include "FreeRTOS.h"

#include "vcp_comm.h"

#define CRC32_COMPUTE_TIME_WAIT pdMS_TO_TICKS(500)
#define CRC32_RESULT_TIME_WAIT pdMS_TO_TICKS(10)

typedef
struct 
{
    TaskHandle_t reply_to;
    uint8_t data[VCP_DRV_BUFF_LEN];
} crc_request_t;

int32_t fd_crc32_compute(crc_request_t * data, uint16_t payload_len, uint32_t * crc32_result);

void FDCRCGrantAccess(TaskHandle_t xTask);
void FDCRCTaskInit(void);

#endif
