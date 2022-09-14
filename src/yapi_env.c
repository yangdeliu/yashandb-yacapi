#include "yapi_inc.h"

YapiResult yapiAllocEnv(YapiEnv** inst)
{
    YapiErrorMsg error;

    void* handle;
    if (yapiOpenDynamicLib("yaclic", &handle, &error) == YAPI_ERROR){
        return YAPI_ERROR;
    }

    return yapiCliAllocHandle(YAPI_HANDLE_ENV, NULL, inst);
}