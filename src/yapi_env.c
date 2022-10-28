#include "yapi_inc.h"
#include "stdlib.h"
#include "inttypes.h"

#ifdef _WIN32
#define YACLI_LIB_NAME "yascli.dll"
#else
#define YACLI_LIB_NAME "libyascli.so"
#endif

YapiResult yapiAllocEnv(YapiEnv** inst)
{
    YapiErrorMsg error;

    void* handle;
    yapiInitError(&error);
    if (yapiOpenDynamicLib(YACLI_LIB_NAME, &handle, &error) == YAPI_ERROR) {
        return YAPI_ERROR;
    }

    YapiEnv* env = malloc(sizeof(YapiEnv));
    if (env == NULL) {
        yapiSetError(&error, YAPI_ERR_ALLOC_MEM, "cannot allocate %" PRId64 " bytes for %s", sizeof(YapiEnv), "env");
        return YAPI_ERROR;
    }
    if (yapiCliAllocHandle(YAPI_HANDLE_ENV, NULL, &env->envHandler) == YAPI_ERROR) {
        return YAPI_ERROR;
    }

    *inst = env;
    return YAPI_SUCCESS;
}

YapiResult yapiReleaseEnv(YapiEnv* inst)
{
    return yapiCliFreeHandle(YAPI_HANDLE_ENV, inst->envHandler);
}