#pragma once
#include "info.h"
#include "buffer.h"
#include <stdio.h>

#define INTERNAL_ERROR(format, ...) { PRINTF(INFO_STR("[INTERNAL ERROR] In %s: "), __FUNCTION__); PRINTF(INFO_STR(format), ##__VA_ARGS__); FPUTC(INFO_STR('\n'), stdout);}
#define INTERNAL(...) {INTERNAL_ERROR(__VA_ARGS__); return true;}
#define PANIC(...) {INTERNAL_ERROR(__VA_ARGS__); exit(1); }

enum info_internal_drawcall_content_stream
{
        TEXT
};

struct info_internal_drawcall
{
        enum info_internal_drawcall_content_stream content_stream;
        ANSI ansi;
        info_buffer content;
};

struct info_internal_Msg
{
        enum INFO_TYPE type;
        struct info_Origin origin;
        List drawcall_list;
        size_t indentation;
        ANSI current;
        ANSI start;
};
typedef struct info_internal_Msg* info_Msg;

extern const info_char *info_internal_tags[];
extern const ANSI info_internal_ANSI[];
extern info_formats info_internal_formats;
extern struct info_format format_current;
extern ANSI ansi_prefix;
extern const size_t tab_width;

bool info_internal_drawcall_vprintf(List drawcall_list, enum info_internal_drawcall_content_stream stream, ANSI ansi, const info_char *format, va_list args);
bool info_internal_drawcall_printf(List drawcall_list, enum info_internal_drawcall_content_stream stream, ANSI ansi, const info_char *format, ...);
