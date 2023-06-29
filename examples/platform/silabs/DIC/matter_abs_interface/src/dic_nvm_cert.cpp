/**
 * @file
 * @brief Matter abstraction layer for Direct Internet Connectivity.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc.
 *www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon
 *Laboratories Inc. Your use of this software is
 *governed by the terms of Silicon Labs Master
 *Software License Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.
 *This software is distributed to you in Source Code
 *format and is governed by the sections of the MSLA
 *applicable to Source Code.
 *
 ******************************************************************************/
#include "stddef.h"
#include "dic_nvm_cert.h"

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif


namespace chip {
namespace DeviceLayer {
namespace Internal {

EXTERNC CHIP_ERROR DiccaCerts(char * buf, size_t *bufSize)
{
    size_t cacerts_len = 0; // without counting null-terminator
    return SilabsConfig::ReadConfigValueStr(SilabsConfig::kConfigKey_CACerts, buf, *bufSize, cacerts_len);
}

EXTERNC CHIP_ERROR deviceCerts(char * buf, size_t *bufSize) 
{
    size_t devicecerts_len = 0; // without counting null-terminator
    return SilabsConfig::ReadConfigValueStr(SilabsConfig::kConfigKey_DeviceCerts, buf, *bufSize, devicecerts_len);
}

EXTERNC CHIP_ERROR devicekey(char * buf, size_t *bufSize) 
{
    size_t devicekey_len = 0; // without counting null-terminator
    return SilabsConfig::ReadConfigValueStr(SilabsConfig::kConfigKey_DeviceKey, buf, *bufSize, devicekey_len);
}

} // END chip
} // END DeviceLayer
} // END Internal