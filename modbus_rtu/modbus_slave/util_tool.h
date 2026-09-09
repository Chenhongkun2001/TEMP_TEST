/*
*/

#ifndef __UTIL_TOOL_H
#define __UTIL_TOOL_H

#include <stdint.h>
#include <ell/ell.h>

uint16_t util_tool_cal_crc16(const uint8_t *buffer, uint32_t buffer_length);

bool util_tool_is_hex_char(uint8_t kpch);
uint8_t util_tool_hex_char_to_uint8(uint8_t kpch);
uint32_t util_tool_cal_elfhash( const uint8_t *inputData,uint32_t length);

bool util_tool_is_platform_big_endian(void);


#endif


