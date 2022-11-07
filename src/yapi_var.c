#include "yapi_inc.h"

YapiResult yapiGetDateStruct(YapiDate date, YapiDateStruct* ds)
{
    YapiErrorMsg error;
    yapiInitError(&error);
    return yapiCliGetDateStruct(date, ds, &error);
}