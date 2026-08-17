#ifndef FD_PARSER_PRIVATE_H
#define FD_PARSER_PRIVATE_H

#include "fd_parser.h"

int32_t parser_memmove(uint8_t * dst, const uint8_t * src, uint16_t len);
int32_t parser_trim_to(struct comm_parser_state * state, uint16_t pos);
int32_t parser_reset(struct comm_parser_state * state);
int32_t parser_find_magic(struct comm_parser_state * state);
int32_t parser_trim_to_magic_val(struct comm_parser_state * state);
int32_t parser_find_header(struct comm_parser_state * state);
int32_t parser_process_header(struct comm_parser_state * state);
int32_t parser_find_all_data(struct comm_parser_state * state);
int32_t parser_find_crc(struct comm_parser_state * state);
int32_t parser_verify_crc(struct comm_parser_state * state);
int32_t parser_processing(struct comm_parser_state * state);

#endif
