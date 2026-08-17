#ifndef FD_COMMANDS_H
#define FD_COMMANDS_H

#include <stdint.h>

#include "faildeadly_config.h"

#define MSG_BUFF_MAX_LEN 1024

typedef
enum comm_opcode
{
    COMM_INV        = COMM_INVALID,
    SYS_RESET       = 0x01,
    SYS_STATUS      = 0x02,
    SYS_PING        = 0x03,
    CRYPT_GEN       = 0x10,
    CRYPT_IMPORT    = 0x11,
    CRYPT_SIGN      = 0x12,
    CRYPT_VERIFY    = 0x13,
    CRYPT_ENCRYPT   = 0x14,
    CRYPT_DECRYPT   = 0x15,
    MEM_STORE       = 0x20,
    MEM_RETRIEVE    = 0x21,
    MAX_OPCODE      = OPCODE_MAX
} comm_opcode;

typedef
enum comm_resp_code
{
    COMM_UNKNOWN            = 0x1,
    COMM_INVAL              = 0x2,
    COMM_SYS_ERROR          = 0x3,
    COMM_SUCCESS            = 0x4,
    COMM_SUCCESS_WITH_DATA  = 0x5,
    COMM_PENDING            = 0x6,
    COMM_CONTINUE           = 0x7,
} comm_resp_code;

typedef
enum comm_sys_status_code
{
    SYS_DISARMED    = 0x0,
    SYS_ARMED       = 0x1,
    SYS_ERROR       = 0x2,
    SYS_TAMPER      = 0x4,
    SYS_SLEEP       = 0x8,
} comm_sys_status_code;

typedef
enum comm_last_reset_reason
{
    SYS_PWR_RESET       = 0x0,
    SYS_COMM_RESET      = 0x1,
    SYS_ASSERT_ERR      = 0x2,
} comm_last_reset_reason;

struct comm_sys_status_resp
{
    comm_sys_status_code status;
    comm_last_reset_reason reason; 
    uint64_t uptime;
    uint16_t ionister_voltage;
};

#endif
