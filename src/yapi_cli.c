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
#define YAPI_LOAD_SYMBOL(symbolName, symbol)                                       \
    if (!symbol && yapiLoadSymbol(symbolName, (YapiPointer*)&symbol, error) < 0) { \
        return YAPI_ERROR;                                                         \
    }

#define YAPI_CHECK_CLI_RETURN()  \
    if (ret != YAC_SUCCESS) {    \
        yapiGetCliError(error);  \
    }                            \
    return ret;

static YapiSymbols yapiSymbols = {NULL};
static void*       yapiLibHandle = NULL;

#ifdef _WIN32

static YapiResult yapiGetWindowsError(DWORD errNum, YapiErrorMsg* error)
{
    char *fallbackErrorFormat = "failed to get message for Windows Error %d";
    wchar_t *errBuf = NULL;
    DWORD length = 0;

    DWORD status =
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                       NULL, errNum, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&errBuf, 0, NULL);
    if (!status && GetLastError() == ERROR_MUI_FILE_NOT_FOUND)
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                NULL, errNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR) &errBuf, 0, NULL);

    if (errBuf == NULL) {
        return YAPI_SUCCESS;
    }

    // strip trailing period and carriage return from message, if needed
    length = (DWORD)wcslen(errBuf);
    errBuf[length] = L'\0';

    // convert to UTF-8 encoding
    if (length > 0) {
        length = WideCharToMultiByte(CP_UTF8, 0, errBuf, -1, error->buf->message, T2S_BUFFER_SIZE, NULL, NULL);
    }
    LocalFree(errBuf);
    return YAPI_SUCCESS;
}

YapiResult yapiOpenDynamicLib(char* libName, YapiPointer* handler, YapiErrorMsg* error)
{
    *handler = LoadLibraryA(libName);
    if (*handler != NULL) {
        yapiLibHandle = *handler;
        return YAPI_SUCCESS;
    }

    DWORD errNum = GetLastError();
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

static int yapiLoadSymbol(const char* symbolName, void** symbol, YapiErrorMsg* error)
{
    *symbol = GetProcAddress(yapiLibHandle, symbolName);
    if (*symbol != NULL) {
        return YAPI_SUCCESS;
    }
    printf("yapiLoadSymbol %p Fail %s\n", symbolName , yapiLibHandle);
    yapiSetError(error, YAPI_ERR_LOAD_SYMBOL, "symbol %s not found in yacli library", symbolName);
    return YAPI_ERROR;
}

#else

YapiResult yapiOpenDynamicLib(char* libName, YapiPointer* handler, YapiErrorMsg* error)
{
    *handler = dlopen(libName, RTLD_LAZY);
    if (!*handler) {
        char* errMsg = dlerror();
        yapiSetError(error, YAPI_ERR_LOAD_SYMBOL, "load yacli library error [%s]", errMsg);
        return YAPI_ERROR;
    }

    yapiLibHandle = *handler;
    return YAPI_SUCCESS;
}

YapiResult yapiCloseDynamicLib(YapiPointer* handler, YapiErrorMsg* error)
{
    int32_t ret = dlclose(*handler);
    if (ret != YAPI_SUCCESS) {
        char* errMsg = dlerror();
        yapiSetError(error, errno, errMsg);
        return YAPI_ERROR;
    }
    *handler = NULL;
    return YAPI_SUCCESS;
}

static int yapiLoadSymbol(const char* symbolName, void** symbol, YapiErrorMsg* error)
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
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacAllocHandle", yapiSymbols.fnAllocHandle)
    ret = (*yapiSymbols.fnAllocHandle)(type, input, output);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliFreeHandle(YapiHandleType type, YacHandle handle)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacFreeHandle", yapiSymbols.fnHandleFree)
    ret = (*yapiSymbols.fnHandleFree)(type, handle);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliGetVersion(char** version)
{
    YapiErrorMsg* error = NULL;

    YAPI_LOAD_SYMBOL("yacGetGetVersion", yapiSymbols.fnGetVersion)
    *version = (*yapiSymbols.fnGetVersion)();
    return YAPI_SUCCESS;
}

YapiResult yapiCliGetLastError(int32_t* errCode, char** message, char** sqlState, YapiTextPos* pos)
{
    YapiErrorMsg* error = NULL;

    YAPI_LOAD_SYMBOL("yacGetLastError", yapiSymbols.fnGetLastError)
    (*yapiSymbols.fnGetLastError)(errCode, message, sqlState, pos);
    return YAPI_SUCCESS;
}

YapiResult yapiCliGetEnvAttr(YacHandle hEnv, YapiEnvAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    YapiErrorMsg* error = NULL;

    YAPI_LOAD_SYMBOL("yacGetEnvAttr", yapiSymbols.fnGetEnvAttr)
    return (*yapiSymbols.fnGetEnvAttr)(hEnv, attr, value, bufLength, stringLength);
}

YapiResult yapiCliConnect(YacHandle hConn, const char* url, int16_t urlLength, const char* user, int16_t userLength,
                          const char* password, int16_t passwordLength)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacConnect", yapiSymbols.fnConnect)
    ret = (*yapiSymbols.fnConnect)(hConn, url, urlLength, user, userLength, password, passwordLength);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliDisconnect(YacHandle hConn)
{
    YapiErrorMsg* error = NULL;

    YAPI_LOAD_SYMBOL("yacDisconnect", yapiSymbols.fnDisconnect)
    (*yapiSymbols.fnDisconnect)(hConn);
    return YAPI_SUCCESS;
}

YapiResult yapiCliSetConnAttr(YacHandle hConn, YapiConnAttr attr, void* value, int32_t length)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacSetConnAttr", yapiSymbols.fnSetConnAttr)
    ret = (*yapiSymbols.fnSetConnAttr)(hConn, attr, value, length);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliGetConnAttr(YacHandle hConn, YapiConnAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacGetConnAttr", yapiSymbols.fnGetConnAttr)
    ret = (*yapiSymbols.fnGetConnAttr)(hConn, attr, value, bufLength, stringLength);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliCommit(YacHandle hConn)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacCommit", yapiSymbols.fnCommit)
    ret = (*yapiSymbols.fnCommit)(hConn);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliRollback(YacHandle hConn)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacRollback", yapiSymbols.fnRollback)
    ret = (*yapiSymbols.fnRollback)(hConn);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliCancel(YacHandle hConn)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacCancel", yapiSymbols.fnCancel)
    ret = (*yapiSymbols.fnCancel)(hConn);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliDirectExecute(YacHandle hStmt, const char* sql, int32_t sqlLength)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacDirectExecute", yapiSymbols.fnDirectExecute)
    ret = (*yapiSymbols.fnDirectExecute)(hStmt, sql, sqlLength);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliPrepare(YacHandle hStmt, const char* sql, int32_t sqlLength)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacPrepare", yapiSymbols.fnPrepare)
    ret = (*yapiSymbols.fnPrepare)(hStmt, sql, sqlLength);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliExecute(YacHandle hStmt)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacExecute", yapiSymbols.fnExecute)
    ret = (*yapiSymbols.fnExecute)(hStmt);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliSetStmtAttr(YacHandle hStmt, YapiStmtAttr attr, void* value, int32_t length)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacSetStmtAttr", yapiSymbols.fnSetStmtAttr)
    ret = (*yapiSymbols.fnSetStmtAttr)(hStmt, attr, value, length);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliGetStmtAttr(YacHandle hStmt, YapiStmtAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacGetStmtAttr", yapiSymbols.fnGetStmtAttr)
    ret = (*yapiSymbols.fnGetStmtAttr)(hStmt, attr, value, bufLength, stringLength);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliFetch(YacHandle hStmt, uint32_t* rows)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacFetch", yapiSymbols.fnFetch)
    ret = (*yapiSymbols.fnFetch)(hStmt, rows);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliDescribeCol2(YacHandle hStmt, uint16_t id, YapiColumnDesc* desc)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacDescribeCol2", yapiSymbols.fnDescribeCol2)
    ret = (*yapiSymbols.fnDescribeCol2)(hStmt, id, desc);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliBindColumn(YacHandle hStmt, uint16_t id, YapiType type, YapiPointer value, int32_t bufLen,
                             int32_t* indicator)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacBindColumn", yapiSymbols.fnBindColumn)
    ret = (*yapiSymbols.fnBindColumn)(hStmt, id, type, value, bufLen, indicator);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliBindParameter(YacHandle hStmt, uint16_t id, YapiParamDirection direction, YapiType bindType,
                                YapiPointer value, uint32_t bindSize, int32_t bufLength, int32_t* indicator)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacBindParameter", yapiSymbols.fnBindParameter)
    ret = (*yapiSymbols.fnBindParameter)(hStmt, id, direction, bindType, value, bindSize, bufLength, indicator);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliBindParameterByName(YacHandle hStmt, char* name, YapiParamDirection direction, YapiType bindType,
                                      YapiPointer value, uint32_t bindSize, int32_t bufLength, int32_t* indicator)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacBindParameterByName", yapiSymbols.fnBindParameterByName)
    ret = (*yapiSymbols.fnBindParameterByName)(hStmt, name, direction, bindType, value, bindSize, bufLength,
                                                indicator);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliNumResultCols(YacHandle hStmt, int16_t* count)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacNumResultCols", yapiSymbols.fnNumResultCols)
    ret = (*yapiSymbols.fnNumResultCols)(hStmt, count);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliGetDateStruct(YapiDate date, YapiDateStruct* ds)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacGetDateStruct", yapiSymbols.fnGetDateStruct)
    ret = (*yapiSymbols.fnGetDateStruct)(date, ds);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobDescAlloc(YapiConnect* hConn, YapiType type, void** desc)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobDescAlloc", yapiSymbols.fnLobDescAlloc)
    ret = (*yapiSymbols.fnLobDescAlloc)(hConn, type, desc);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobDescFree(void* desc, YapiType type)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobDescFree", yapiSymbols.fnLobDescFree)
    ret = (*yapiSymbols.fnLobDescFree)(desc, type);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobGetChunkSize(YapiConnect* hConn, YapiLobLocator* locator, uint16_t* chunkSize)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobGetChunkSize", yapiSymbols.fnLobGetChunkSize)
    ret = (*yapiSymbols.fnLobGetChunkSize)(hConn, locator, chunkSize);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobGetLength(YapiConnect* hConn, YapiLobLocator* locator, uint64_t* length)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobGetLength", yapiSymbols.fnLobGetLength)
    ret = (*yapiSymbols.fnLobGetLength)(hConn, locator, length);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobRead(YapiConnect* hConn, YapiLobLocator* loc, uint64_t* bytes, uint8_t* buf, uint64_t bufLen)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobRead", yapiSymbols.fnLobRead)
    ret = (*yapiSymbols.fnLobRead)(hConn, loc, bytes, buf, bufLen);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobWrite(YapiConnect* hConn, YapiLobLocator* loc, uint64_t* bytes, uint8_t* buf, uint64_t bufLen)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobWrite", yapiSymbols.fnLobWrite)
    ret = (*yapiSymbols.fnLobWrite)(hConn, loc, bytes, buf, bufLen);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobCreateTemporary(YapiConnect* hConn, YapiLobLocator* loc)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobCreateTemporary", yapiSymbols.fnLobCreateTemporary)
    ret = (*yapiSymbols.fnLobCreateTemporary)(hConn, loc);
    YAPI_CHECK_CLI_RETURN();
}

YapiResult yapiCliLobFreeTemporary(YapiConnect* hConn, YapiLobLocator* loc)
{
    YapiErrorMsg* error = NULL;
    YapiResult    ret;

    YAPI_LOAD_SYMBOL("yacLobFreeTemporary", yapiSymbols.fnLobFreeTemporary)
    ret = (*yapiSymbols.fnLobFreeTemporary)(hConn, loc);
    YAPI_CHECK_CLI_RETURN();
}