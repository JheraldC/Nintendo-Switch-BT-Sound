#ifndef _TYPES_H_
#define _TYPES_H_
#include <stdint.h>
#include <stddef.h>
typedef int8_t s8;typedef int16_t s16,SHORT;typedef int32_t s32,INT,LONG;typedef int64_t s64;
typedef uint8_t u8,BYTE;typedef uint16_t u16,WORD,WCHAR;typedef uint32_t u32,UINT,DWORD;typedef uint64_t u64,QWORD;
typedef int bool;
#define true 1
#define false 0
void print_error(void);
void gfx_printf(const char *,...);
#endif
typedef volatile uint8_t vu8; typedef volatile uint16_t vu16; typedef volatile uint32_t vu32;
