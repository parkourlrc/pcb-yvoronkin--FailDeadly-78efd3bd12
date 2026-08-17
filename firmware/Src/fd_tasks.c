#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

#include "fd_tasks.h"
#include "fd_log.h"
#include "test_task.h"
#include "fd_crypto.h"
#include "vcp_comm.h"
#include "fd_crc.h"


void FDTasksInit(void)
{
    FDLogTaskInit();

    FDCRCTaskInit();

    VCPTransmitTaskInit();
    VCPReceiveTaskInit();

    BlinkTestTaskInit();

    FDCryptoTaskInit();

    FDLogFreeRTOSStateInit();

    return;
}
