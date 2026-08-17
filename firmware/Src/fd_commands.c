#include <string.h>
#include <stdint.h>

#include "crc.h"

#include "fd_parser.h"
#include "fd_commands.h"
#include "fd_log.h"

#include "endian.h"
#include "stm32_assert.h" // should be last to prevent assert macro redefinition 

const
char crc_compute_error[] = "CRC computation returned an error.";

static
void encode_answer(struct comm_parser_state * state, comm_resp_header * header, 
                   uint16_t payload_len, void *data)
{
    crc_request_t * crc_request = state->response;
    uint16_t packet_len;
    uint32_t crc_val = 0;
    int32_t res;

    assert_param((payload_len != 0) && (data != NULL));
    assert_param(payload_len <= FD_COMM_DATA_OUT_MAX_LEN); 

    memcpy(&crc_request->data[RESP_HEADER_LEN], data, payload_len);
   
    packet_len = RESP_HEADER_LEN + payload_len;

    // CRC      
    res = fd_crc32_compute(crc_request, packet_len, &crc_val);
    if (res != 0) 
    {
        FDSendLog((unsigned char *)crc_compute_error, 
                  strlen(crc_compute_error), 0, 0);
        return;
    }
    memcpy(&crc_request->data[RESP_HEADER_LEN + payload_len], &crc_val, CRC32_LEN);

    // SEND
    packet_len = RESP_HEADER_LEN + payload_len + CRC32_LEN;

    state->response_len = packet_len;
}

static
void comm_invalid(struct comm_parser_state *state, void *data)
{
    comm_resp_header resp = {
        .magic      = COMM_MAGIC,
        .req_opcode = state->req_header.opcode,
        .resp_code  = COMM_UNKNOWN,
        .data_len   = 0,
    };
    
    encode_answer(state, &resp, 0, NULL);
}

static
void comm_sys_reset(struct comm_parser_state *state, void *data)
{

}

static
void comm_sys_status(struct comm_parser_state *state, void *data)
{

}

static
void comm_sys_ping(struct comm_parser_state *state, void *data)
{
    comm_resp_header resp = {
        .magic      = COMM_MAGIC,
        .req_opcode = state->req_header.opcode,
        .resp_code  = COMM_SUCCESS,
        .data_len   = 0,
    };
    
    encode_answer(state, &resp, 0, NULL);
}

static
void comm_crypt_gen(struct comm_parser_state *state, void *data)
{

}

static
void comm_crypt_import(struct comm_parser_state *state, void *data)
{

}

static
void comm_crypt_sign(struct comm_parser_state *state, void *data)
{

}

static
void comm_crypt_verify(struct comm_parser_state *state, void *data)
{

}

static
void comm_crypt_encrypt(struct comm_parser_state *state, void *data)
{

}

static
void comm_crypt_decrypt(struct comm_parser_state *state, void *data)
{

}

static
void comm_mem_store(struct comm_parser_state *state, void *data)
{

}

static
void comm_mem_retrieve(struct comm_parser_state *state, void *data)
{

}

const
struct command_table_entry command_table[] = {
    /*COMM_INVALID */   {comm_invalid},          

    /*SYS_RESET    */   {comm_sys_reset},        
    /*SYS_STATUS   */   {comm_sys_status},       
    /*SYS_PING     */   {comm_sys_ping},         

    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          

    /*CRYPT_GEN    */   {comm_crypt_gen},        
    /*CRYPT_IMPORT */   {comm_crypt_import},     
    /*CRYPT_SIGN   */   {comm_crypt_sign},       
    /*CRYPT_VERIFY */   {comm_crypt_verify},     
    /*CRYPT_ENCRYPT*/   {comm_crypt_encrypt},    
    /*CRYPT_DECRYPT*/   {comm_crypt_decrypt},    

    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          
    /*COMM_INVALID */   {comm_invalid},          

    /*MEM_STORE    */   {comm_mem_store},        
    /*MEM_RETRIEVE */   {comm_mem_retrieve},     
};



