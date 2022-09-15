#include "yapi_inc.h"
#include "stdlib.h"
#include "string.h"

YapiResult yapiConnect(YapiEnv* env, const char* url, int16_t urlLength, const char* user, int16_t userLength,
                     const char* password, int16_t passwordLength, YapiConnect** hConn)
{
    YapiConnect* conn = malloc(sizeof(YapiConnect));
    if (conn == NULL) {
        return YAPI_ERROR;
    }
    if (yapiCliAllocHandle(YAPI_HANDLE_DBC, env->envHandler, &conn->connHandler) != YAPI_SUCCESS) {
       return YAPI_ERROR;
    }
    if (yapiCliConnect(conn->connHandler, url,urlLength, user, userLength,password,passwordLength) != YAPI_SUCCESS) {
        return YAPI_ERROR;
    }
    *hConn = conn;
    return YAPI_SUCCESS;
}

YapiResult yapiDisconnect(YapiConnect* hConn)
{
    return yapiCliDisconnect(hConn->connHandler);
}

YapiResult yapiReleseConn(YapiConnect* hConn)
{
    return yapiCliFreeHandle(YAPI_HANDLE_DBC, hConn->connHandler);
}

YapiResult yapiCancel(YapiConnect* hConn)
{
    return YAPI_ERROR;
}

YapiResult yapiCommit(YapiConnect* hConn)
{
    return yapiCliCommit(hConn->connHandler);
}

YapiResult yapiRollback(YapiConnect* hConn)
{
    return yapiCliRollback(hConn->connHandler);
}

YapiResult yapiSetConnAttr(YapiConnect* hConn, YapiConnAttr attr, void* value, int32_t length)
{
    return yapiCliSetConnAttr(hConn->connHandler, attr, value, length);
}

YapiResult yapiGetConnAttr(YapiConnect* hConn, YapiConnAttr attr, void* value, int32_t bufLength, int32_t* stringLength)
{
    return yapiCliGetConnAttr(hConn->connHandler,attr, value, bufLength,stringLength);
}

void  yapiGetLastError(YapiErrorInfo* info)
{
    char* msg, *stat;
    if(yapiCliGetLastError(&info->errCode, &msg, &stat, &info->pos) != YAPI_SUCCESS){
        info->errCode = -1;
        info->pos.column = -1;
        info->pos.line = -1;
        strcpy(info->message, "get error failed");
        strcpy(info->sqlState, "00000");
    } else {
        strcpy(info->message, msg);
        strcpy(info->sqlState, stat);
    }
}
