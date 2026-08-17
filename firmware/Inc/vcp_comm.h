#ifndef VCP_COMM_H
#define VCP_COMM_H

#include "FreeRTOS.h"
#include "task.h"

#define VCP_DRV_BUFF_LEN 1024
#define VCP_DRV_SEND_MAX_WAIT 1000

//TX
void VCPTXGrantAccess(TaskHandle_t xTask);
void VCPTransmitTaskInit(void); 
int32_t vcpSend(const char * buf, uint16_t len);

//RX
void VCPReceiveTaskInit(void);
int VCPReceiveRoutine(uint8_t* Buf, uint32_t *Len);

#endif
