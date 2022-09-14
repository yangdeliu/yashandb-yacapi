#include "yapi_inc.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
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

// macro to simplify code for loading each symbol
#define YAPI_LOAD_SYMBOL(symbolName, symbol) \
    if (!symbol && yapiLoadSymbol(symbolName, (YapiPointer*) &symbol, \
            error) < 0) {\
        return YAPI_ERROR; \
    }

static YapiSymbols yapiSymbols = {NULL};
static void *yapiLibHandle = NULL;

#ifdef _WIN32

static YapiResult yapiGetWindowsError(DWORD errNum, YapiErrorMsg* error)
{
    TCHAR *errBUf = NULL;
    DWORD status = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, errNum, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            (LPTSTR) &errBUf, 0, NULL);
    return YAPI_ERROR;
}

YapiResult yapiOpenDynamicLib(char* libName, YapiPointer* handler, YapiErrorMsg* error)
{
    *handler = LoadLibrary(libName);
    if(*handler != NULL) {
        return YAPI_SUCCESS;
    }

    DWORD errNum = GetLastError();
    // otherwise, attempt to get the error message
    return yapiGetWindowsError(errNum, error);
    
}

YapiResult yapiCloseDynamicLib(YapiPointer* handler, YapiErrorMsg* error)
{
    BOOL ret = FreeLibrary(*handler);
    if (ret) {
        *handler = NULL;
        return YAPI_SUCCESS;
    }
    DWORD errNum = GetLastError();
    yapiSetError(error, errNum, "");
    return YAPI_ERROR;
}

static int yapiLoadSymbol(const char *symbolName, void **symbol, YapiErrorMsg *error)
{
    *symbol = GetProcAddress(yapiLibHandle, symbolName);
    if (*symbol != NULL) {
    return YAPI_SUCCESS;
    }
    yapiSetError(error, YAPI_ERR_LOAD_SYMBOL, "symbol %s not found in yacli library", symbolName);
    return YAPI_ERROR;
}

#else

YapiResult yapiOpenDynamicLib(char* libName, void** handler)
{
    *handler = dlopen(libName, RTLD_LAZY);
    if (!*handler) {
        CodChar* errMsg = dlerror();
        COD_SET_ERROR(ERR_CMM_LOAD_LIB_FAILED, libName, errMsg);
        return COD_ERROR;
    }

    return COD_SUCCESS;
}

CodResult codDynamicLibClose(CodPointer* handler)
{
    if (*handler == NULL) {
        return COD_SUCCESS;
    }
    CodInt32 ret = dlclose(*handler);
    if (ret != COD_SUCCESS) {
        CodChar* errMsg = dlerror();
        COD_SET_ERROR(ERR_CMM_CLOSE_LIB_FAILED, errMsg);
        return COD_ERROR;
    }
    *handler = NULL;
    return COD_SUCCESS;
}

static int yapiLoadSymbol(YapiSymbols* funcs, const char *symbolName, void **symbol, YapiErrorMsg *error)
{
    *symbol = dlsym(yapiLibHandle, symbolName);
    if (!*symbol) {
        yapiSetError(error, YAPI_ERR_LOAD_SYMBOL, "symbol %s not found in yacli library", symbolName);
        return YAPI_ERROR;
    }
    return YAPI_SUCCESS;
}
#endif


YapiResult yapiCliAllocHandle(YapiHandleType type, YacHandle input, YacHandle* output)
{
    YapiErrorMsg *error = NULL;
    YapiResult ret;

    YAPI_LOAD_SYMBOL("yacAllocHandle", yapiSymbols.fnAllocHandle)
    ret = (*yapiSymbols.fnAllocHandle)(type, input, output);
    return ret;
}

YapiResult yapiCliFreeHandle(YapiHandleType type, YacHandle handle)
{
    YapiErrorMsg *error = NULL;
    YapiResult ret;

    YAPI_LOAD_SYMBOL("yacFreeHandle", yapiSymbols.fnHandleFree)
    ret = (*yapiSymbols.fnHandleFree)(type, handle);
    return ret;
}

YapiResult yapiCliGetVersion(char** version)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacGetGetVersion", yapiSymbols.fnGetVersion)
    *version = (*yapiSymbols.fnGetVersion)();
    return YAPI_SUCCESS;
}

YapiResult yapiCliGetLastError(int32_t* errCode, char** message, char** sqlState, YapiTextPos* pos)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacGetGetVersion", yapiSymbols.fnGetVersion)
    (*yapiSymbols.fnGetVersion)(errCode, message, sqlState, pos);
    return YAPI_SUCCESS;
}

YapiResult yapiCliGetEnvAttr(YacHandle hEnv, YapiEnvAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacGetEnvAttr", yapiSymbols.fnGetEnvAttr)
    return (*yapiSymbols.fnGetEnvAttr)(hEnv, attr, value, bufLength, stringLength);
}

YapiResult yapiCliConnect(YacHandle hConn, const char *url, int16_t urlLength, const char *user, int16_t userLength,
                      const char *password, int16_t passwordLength)
{
    YapiErrorMsg *error = NULL;
    YapiResult ret;

    YAPI_LOAD_SYMBOL("yacConnect", yapiSymbols.fnConnect)
    ret = (*yapiSymbols.fnConnect)(hConn, url, urlLength, user, userLength, password, passwordLength);
    return ret;
}

YapiResult yapiCliDisconnect(YacHandle hConn)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacDisconnect", yapiSymbols.fnDisconnect)
    (*yapiSymbols.fnDisconnect)(hConn);
    return YAPI_SUCCESS;
}

YapiResult yapiCliSetConnAttr(YacHandle hConn, YapiConnAttr attr, void* value, int32_t length)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacSetConnAttr", yapiSymbols.fnSetConnAttr)
    return (*yapiSymbols.fnSetConnAttr)(hConn, attr, value, length);
}

YapiResult yapiCliGetConnAttr(YacHandle hConn, YapiConnAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacGetConnAttr", yapiSymbols.fnGetConnAttr)
    return (*yapiSymbols.fnGetConnAttr)(hConn, attr, value, bufLength, stringLength);
}

YapiResult yapiCliCommit(YacHandle hConn)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacCommit", yapiSymbols.fnCommit)
    return (*yapiSymbols.fnCommit)(hConn);
}

YapiResult yapiCliRollback(YacHandle hConn)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacRollback", yapiSymbols.fnRollback)
    return (*yapiSymbols.fnRollback)(hConn);
}

YapiResult yapiCliCancel(YacHandle hConn)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacCancel", yapiSymbols.fnCancel)
    return (*yapiSymbols.fnCancel)(hConn);
}

YapiResult yapiCliDirectExecute(YacHandle hStmt, const char* sql, int32_t sqlLength)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacDirectExecute", yapiSymbols.fnDirectExecute)
    return (*yapiSymbols.fnDirectExecute)(hStmt, sql, sqlLength);
}

YapiResult yapiCliPrepare(YacHandle hStmt, const char* sql, int32_t sqlLength)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacPrepare", yapiSymbols.fnPrepare)
    return (*yapiSymbols.fnPrepare)(hStmt, sql, sqlLength);
}

YapiResult yapiCliExecute(YacHandle hStmt)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacExecute", yapiSymbols.fnExecute)
    return (*yapiSymbols.fnExecute)(hStmt);
}

YapiResult yapiCliSetStmtAttr(YacHandle hStmt, YapiStmtAttr attr, void* value, int32_t length)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacSetStmtAttr", yapiSymbols.fnSetStmtAttr)
    return (*yapiSymbols.fnSetStmtAttr)(hStmt, attr, value, length);
}

YapiResult yapiCliGetStmtAttr(YacHandle hStmt, YapiStmtAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacGetStmtAttr", yapiSymbols.fnGetStmtAttr)
    return (*yapiSymbols.fnGetStmtAttr)(hStmt, attr, value, bufLength, stringLength);
}

YapiResult yapiCliFetch(YacHandle hStmt, uint32_t* rows)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacFetch", yapiSymbols.fnFetch)
    return (*yapiSymbols.fnFetch)(hStmt, rows);
}

YapiResult yapiCliDescribeCol2(YacHandle hStmt, uint16_t id, YapiColumnDesc* desc)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacDescribeCol2", yapiSymbols.fnDescribeCol2)
    return (*yapiSymbols.fnDescribeCol2)(hStmt, id, desc);
}

YapiResult yapiCliBindColumn(YacHandle hStmt, uint16_t id, YapiType type, YapiPointer value, int32_t bufLen,
                        int32_t* indicator)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacBindColumn", yapiSymbols.fnBindColumn)
    return (*yapiSymbols.fnBindColumn)(hStmt, id, type, value, bufLen, indicator);
}

YapiResult yapiCliBindParameter(YacHandle hStmt, uint16_t id, YapiParamDirection direction, YapiType bindType,
                           YapiPointer value, uint32_t bindSize, int32_t bufLength, int32_t* indicator)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacBindParameter", yapiSymbols.fnBindParameter)
    return (*yapiSymbols.fnBindParameter)(hStmt, id, direction, bindType, value, bindSize, bufLength, indicator);
}

YapiResult yapiCliBindParameterByName(YacHandle hStmt, char* name, YapiParamDirection direction, YapiType bindType,
                                 YapiPointer value, uint32_t bindSize, int32_t bufLength, int32_t* indicator)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacBindParameterByName", yapiSymbols.fnBindParameterByName)
    return (*yapiSymbols.fnBindParameterByName)(hStmt, name, direction, bindType, value, bindSize, bufLength, indicator);
}

YapiResult yapiCliNumResultCols(YacHandle hStmt, int16_t* count)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacNumResultCols", yapiSymbols.fnNumResultCols)
    return (*yapiSymbols.fnNumResultCols)(hStmt, count);
}

YapiResult yapiCliGetDateStruct(YapiDate date, YapiDateStruct* ds)
{
    YapiErrorMsg *error = NULL;

    YAPI_LOAD_SYMBOL("yacGetDateStruct", yapiSymbols.fnGetDateStruct)
    return (*yapiSymbols.fnGetDateStruct)(date, ds);
}

