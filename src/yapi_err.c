#include "yacapi.h"
#include "yapi_inc.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <dlfcn.h>
#endif
#ifdef __linux
#include <unistd.h>
#include <sys/syscall.h>
#endif

#ifdef _MSC_VER
#define __thread __declspec(thread)  // Thread Local Storage
#endif

__thread YapiErrorBuffer gErrorBuf;

void yapiSetError(YapiErrorMsg* error, yapiErrorNum errorNum, const char* format, ...)
{
    if(error == NULL) {
        return;
    }
    va_list args;
    error->buf->code = errorNum;

    va_start(args, format);
    error->buf->messageLen = (uint32_t)vsnprintf(error->buf->message,
                            sizeof(error->buf->message),
                            format, args);
    va_end(args);
}

void yapiInitError(YapiErrorMsg *error)
{
    error->buf = &gErrorBuf;
}

void yapiGetCliError(YapiErrorMsg* error)
{
    printf("yapiGetCliError\n");
    char *msg, *stat;
    if (yapiCliGetLastError(&error->buf->code, &msg, &stat, &error->buf->pos) != YAPI_SUCCESS) {
        printf("yapiGetCliError GetLastError Fail\n");
        error->buf->code = -1;
        error->buf->pos.column = -1;
        error->buf->pos.line = -1;
        msg = "get error failed";
        stat = "00000";
    }
    printf("yapiGetCliError GetLastError %s\n", msg);
    strcpy_s(error->buf->message, T2S_BUFFER_SIZE, msg);
    strcpy_s(error->buf->sqlState, YAPI_MAX_SQLSTAT_LEN, stat);
}

void yapiGetErrorInfo(YapiErrorMsg *error, YapiErrorInfo *info)
{
    info->errCode = error->buf->code;
    info->message = error->buf->message;
    info->pos = &error->buf->pos;
    info->sqlState = error->buf->sqlState;
}

#ifdef _WIN32
static YapiResult yapiGetWindowsError(DWORD errNum, YapiErrorMsg* error)
{
    TCHAR *errBuf = NULL;
    DWORD status = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, errNum, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            (LPTSTR) &errBuf, 0, NULL);
    if (errBuf == NULL) {
        return YAPI_SUCCESS;
    }

    size_t len = _tcslen(errBuf);
    if (len >= T2S_BUFFER_SIZE) {
        len = T2S_BUFFER_SIZE - 1;
    }
    memcpy(error->buf->message, errBuf, len);
    LocalFree(errBuf);
    return YAPI_ERROR;
}
#endif