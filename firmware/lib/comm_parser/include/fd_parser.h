#ifndef FD_PARSER_H
#define FD_PARSER_H

#include <stdint.h>

#define MSG_BUFF_MAX_LEN 1024

#ifndef UNIT_TESTS
    #include "FreeRTOS.h"
    #include "fd_crc.h"
#else
    typedef uint32_t TickType_t;

    typedef
    struct {
        uint32_t reply_to;
        uint8_t data[MSG_BUFF_MAX_LEN];
    } crc_request_t;
#endif

#define CRC32_LEN 4
#define REQ_HEADER_LEN 5


#define FD_COMM_DATA_IN_MAX_LEN  \
            (MSG_BUFF_MAX_LEN - CRC32_LEN - REQ_HEADER_LEN)

#define RESP_HEADER_LEN 6
#define FD_COMM_DATA_OUT_MAX_LEN  \
            (MSG_BUFF_MAX_LEN - CRC32_LEN - RESP_HEADER_LEN)

#define COMM_MAGIC 0xFDDF
#define COMM_MAGIC_SIZE 2

#define PARSER_NOT_READY -1
#define PARSER_STAGE_RESET 0
#define PARSER_NEXT_STAGE 1

typedef
struct comm_req_header
{
    uint16_t magic;
    uint8_t opcode;
    uint16_t data_len;
} comm_req_header;

typedef
struct comm_resp_header
{
    uint16_t magic;
    uint8_t req_opcode;
    uint8_t resp_code;
    uint16_t data_len;
} comm_resp_header;

typedef
enum comm_parser_stage
{
    COMM_PARSER_RESET = 0x0,
    COMM_PARSER_SEARCH_MAGIC,
    COMM_PARSER_TRIM_TO_MAGIC_VAL,
    COMM_PARSER_SEARCH_HEADER,
    COMM_PARSER_PROCESS_HEADER,
    COMM_PARSER_WAIT_FOR_DATA,
    COMM_PARSER_SEARCH_CRC,
    COMM_PARSER_VERIFY_CRC,
    COMM_PARSER_PROCES_MSG,
} comm_parser_stage;


struct comm_parser_state
{
    crc_request_t * crc_req; 
    crc_request_t * response;
    size_t response_len;
    uint8_t * buf;
    size_t max_len;
    comm_parser_stage stage;
    TickType_t msg_first_received_ticks;    
    TickType_t msg_curr_ticks;
    TickType_t msg_timeout_ticks;
    comm_req_header req_header;
    size_t magic_pos;
    size_t crc_pos;
    size_t msg_cur_end_pos;
};

struct command_table_entry
{
    void (*handler)(struct comm_parser_state * state, void * data);
};

void comm_parser_process(struct comm_parser_state *state, size_t bytes_added);

/*
   +----------------------------------------+--------------------------------+---------+
   |                                        |                                |         |
   V                                        |                                |         |
 RESET -> SEARCH -> TRIM_TO -> SEARCH -> PROCESS -> WAIT_FOR -> SEARCH -> VERIFY -> PROCESS
           MAGIC     MAGIC     HEADER     HEADER      DATA       CRC        CRC     MESSAGE
   ^         |         |         |          |           |         |          |           
   |         |         |         |          |           |         |          |           
 TIMEOUT <---+---------+---------+----------+-----------+---------+----------+

*/



#endif
