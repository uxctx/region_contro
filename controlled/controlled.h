#pragma once


#include "targetver.h"

#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#include "Work.h"

void default_log_call(int level, const char *file, int line, const char* fmt, va_list vl);

static void(*log_callback)(int level, const char *file, int line, const char* fmt, va_list vl) = default_log_call;

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(...) log_call(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_call(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_call(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_call(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_call(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_call(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)



void log_call(int level, const char *file, int line, const char *fmt, ...);


WCHAR* _FormatErrorMessage(DWORD dwErrCode);

void OutLogWSAError();


