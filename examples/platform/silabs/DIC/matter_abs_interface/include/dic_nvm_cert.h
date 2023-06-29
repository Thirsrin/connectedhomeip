
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

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <lib/core/CHIPError.h>
#include <platform/silabs/SilabsConfig.h>
#define DIC_MAX_CERT_LENGTH_BUF 1500

namespace chip {
namespace DeviceLayer {
namespace Internal {


CHIP_ERROR DiccaCerts(char * buf, size_t *bufSize);

CHIP_ERROR deviceCerts(char * buf, size_t *bufSize);

CHIP_ERROR devicekey(char * buf, size_t *bufSize);

}
}
}


#ifdef __cplusplus
}
#endif