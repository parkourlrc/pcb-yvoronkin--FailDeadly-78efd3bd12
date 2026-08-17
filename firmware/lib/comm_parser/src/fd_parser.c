#include <string.h>
#include <stdint.h>

#include "fd_parser_private.h"
#include "fd_parser.h"

#include "faildeadly_config.h"

#ifndef UNIT_TESTS
#include "fd_log.h"
#include "stm32_assert.h"
#endif

struct stage_table_entry
{
    int32_t (*handler)(struct comm_parser_state *);
};

int32_t parser_memmove(uint8_t * dst, const uint8_t * src, uint16_t len)
{
    assert_param(dst != NULL);
    assert_param(src != NULL);

    if (dst == src || len == 0) return 0;
 
    if (dst < src)
    {
        for (uint16_t i = 0; i < len; i++)
            dst[i] = src[i];
    }
    else
    {
        for (uint16_t i = len; i != 0; i--)
            dst[i-1] = src[i - 1];
    }

    return len;
}

int32_t parser_trim_to(struct comm_parser_state * state, uint16_t pos)
{
    uint16_t remaining = 0;
    assert_param(state != NULL);
    assert_param(state->max_len >= pos);

    if (pos == 0) 
        return 0; // fast skip

    remaining = state->max_len - pos;

    parser_memmove(state->buf, &state->buf[pos], remaining);
    state->max_len = remaining;

    return (int32_t)remaining;
}

int32_t parser_reset(struct comm_parser_state * state)
{
    parser_trim_to(state, state->msg_cur_end_pos);

    memset(&state->req_header, 0, sizeof(state->req_header));

    state->stage = COMM_PARSER_RESET;
    state->msg_first_received_ticks = 0; 
    state->magic_pos = 0;
    state->msg_cur_end_pos = 0;

    return PARSER_NEXT_STAGE;
}

int32_t parser_find_magic(struct comm_parser_state * state)
{
    const uint8_t magic_lo = (uint8_t)(COMM_MAGIC & 0xffu);
    const uint8_t magic_hi = (uint8_t)((COMM_MAGIC >> 8) & 0xffu);

    if (state->max_len < COMM_MAGIC_SIZE)
        return PARSER_NOT_READY;

    for (uint16_t i = 0; i + COMM_MAGIC_SIZE <= state->max_len; i++) 
    {
        if (state->buf[i]   == magic_lo &&
            state->buf[i+1] == magic_hi)
        {
            state->magic_pos = i;
            state->msg_cur_end_pos = i + COMM_MAGIC_SIZE;
            state->msg_first_received_ticks = state->msg_curr_ticks;

            return PARSER_NEXT_STAGE; // Magic value found
        }
    }

    /*
     * Trim to the end, but left one byte as it may be 
     * the first byte of MAGIC value
     */
    state->msg_cur_end_pos = state->max_len - (COMM_MAGIC_SIZE - 1u);

    return PARSER_STAGE_RESET;
}

int32_t parser_trim_to_magic_val(struct comm_parser_state * state)
{
    parser_trim_to(state, state->magic_pos);
    state->msg_cur_end_pos -= state->magic_pos;
    state->magic_pos = 0;

    return PARSER_NEXT_STAGE;
}

int32_t parser_find_header(struct comm_parser_state * state)
{
    // Just verify that enough data is available.
    if (state->max_len < REQ_HEADER_LEN) return PARSER_NOT_READY;

    // Since magic value is a part of the header
    state->msg_cur_end_pos = REQ_HEADER_LEN;
    return PARSER_NEXT_STAGE;
}

static inline
uint16_t parser_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] |
           ((uint16_t)p[1] << 8);
}

static const
char wrong_len_msg[] = "Message data length exceeds the limit.";

int32_t parser_process_header(struct comm_parser_state * state)
{
    state->req_header.magic = parser_read_le16(&state->buf[0]);
    state->req_header.opcode = state->buf[2];
    state->req_header.data_len = parser_read_le16(&state->buf[3]);

    if (state->req_header.magic != COMM_MAGIC)
        return PARSER_STAGE_RESET;

    if (state->req_header.data_len > FD_COMM_DATA_IN_MAX_LEN)
    {
        FDSendLog((unsigned char*)wrong_len_msg, strlen(wrong_len_msg), 0, 0);

        // TODO: send notification
        return PARSER_STAGE_RESET;
    }

    return PARSER_NEXT_STAGE;
}

int32_t parser_find_all_data(struct comm_parser_state * state)
{
    // Just verify that enough data is available.
    if (state->max_len < REQ_HEADER_LEN + 
        state->req_header.data_len) 
        return PARSER_NOT_READY;

    state->msg_cur_end_pos += state->req_header.data_len;
    return PARSER_NEXT_STAGE;
}

int32_t parser_find_crc(struct comm_parser_state * state)
{
    // Verify that enouht data is available and set a crc pos
    if (state->max_len < REQ_HEADER_LEN +
        state->req_header.data_len +
        CRC32_LEN) return PARSER_NOT_READY;

    state->crc_pos = state->msg_cur_end_pos;
    state->msg_cur_end_pos += CRC32_LEN;

    return PARSER_NEXT_STAGE;
}

static inline
uint32_t parser_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static const
char invalid_crc[] = "Message CRC is invalid.";

int32_t parser_verify_crc(struct comm_parser_state * state)
{
    int32_t res = 0;
    uint32_t computed_crc = 0; 
    uint32_t received_crc = 0;

    res = fd_crc32_compute(state->crc_req, state->crc_pos, &computed_crc);

    assert_param(res != -1); // Something with semaphore

    if (res != 0) return PARSER_NOT_READY; // timeout

    received_crc = parser_read_le32(&state->buf[state->crc_pos]);
    
    if (computed_crc != received_crc)
    {
        FDSendLog((unsigned char *)invalid_crc, strlen(invalid_crc), 0, 0);

        // TODO: send notification
        return PARSER_STAGE_RESET;
    }
    
    return PARSER_NEXT_STAGE;
}

extern
struct command_table_entry command_table[];

int32_t parser_processing(struct comm_parser_state * state)
{
    if (state->req_header.opcode >= OPCODE_MAX)
    {
        (command_table[COMM_INVALID].handler)(state, state->buf);
    }
    else
    {
        (command_table[state->req_header.opcode].handler)(state, state->buf);
    }
    return PARSER_STAGE_RESET;
}

static const
struct stage_table_entry stage_table[] = {
    /* COMM_PARSER_RESET            */ {&parser_reset},
    /* COMM_PARSER_FIND_MAGIC       */ {&parser_find_magic},
    /* COMM_PARSER_TRIM_TO_MAGIC_VAL*/ {&parser_trim_to_magic_val},
    /* COMM_PARSER_FIND_HEADER      */ {&parser_find_header},
    /* COMM_PARSER_PROCESS_HEADER   */ {&parser_process_header},
    /* COMM_PARSER_WAIT_FOR_DATA    */ {&parser_find_all_data},
    /* COMM_PARSER_FIND_CRC         */ {&parser_find_crc},
    /* COMM_PARSER_VERIFY_CRC       */ {&parser_verify_crc},
    /* COMM_PARSER_PROCESSING       */ {&parser_processing},
};

void comm_parser_process(struct comm_parser_state * state, size_t bytes_added)
{
    int32_t ret = 0;
    TickType_t elapsed_ticks_count;

    // add new bytes to processing
    state->max_len += bytes_added;

    // check timeout
    if (state->stage != COMM_PARSER_RESET)
    {
        if (state->msg_curr_ticks >= state->msg_first_received_ticks)
        {
            elapsed_ticks_count = state->msg_curr_ticks - 
                                  state->msg_first_received_ticks;
        }
        else
        {
            elapsed_ticks_count = (portMAX_DELAY - state->msg_first_received_ticks) + 
                                  state->msg_curr_ticks + 1;
        }

        if (elapsed_ticks_count >= state->msg_timeout_ticks)
        {
            state->stage = COMM_PARSER_RESET;
        }
    }

    // call stage related handler
    do 
    {
        ret = (*stage_table[state->stage].handler)(state);

        if (ret == PARSER_STAGE_RESET) state->stage = COMM_PARSER_RESET;
        if (ret >= PARSER_NEXT_STAGE)  state->stage += 1; 
    } while(ret >= PARSER_STAGE_RESET);
}
