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

#ifndef __DIC_CONFIG_H
#define __DIC_CONFIG_H

/*certificates*/
#define USE_AWS 1
#define USE_MOSQUITTO 0

/* Task Configuration*/
#define DIC_TASK_NAME "DIC"
#define DIC_TASK_STACK_SIZE (3*1024)  //2k

/* Network Configuration*/
#define DIC_SERVER_HOST	"a2m21kovu9tcsh-ats.iot.ap-southeast-1.amazonaws.com"
#define DIC_SERVER_PORT	8883
#define DIC_SERVER_CA_CERT  ca_certificate
#define DIC_DEVICE_CERT     device_certificate
#define DIC_DEVICE_KEY      device_key

#define DIC_KEEP_ALIVE 0

#if USE_AWS

#define DIC_CLIENT_ID "DIC_2"
#define DIC_CLIENT_USER NULL
#define DIC_CLIENT_PASS NULL

char ca_certificate[] = "-----BEGIN CERTIFICATE-----\n"
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
"rqXRfboQnoZsG4q5WTP468SQvvG5\n"
"-----END CERTIFICATE-----";

char device_certificate[] = "-----BEGIN CERTIFICATE-----\n"
"MIIB/DCCAaMCFFvLxRPNwTeVQB5YtKLbEdwdcvMaMAoGCCqGSM49BAMCMIGAMQsw\n"
"CQYDVQQGEwJJTjESMBAGA1UECAwJVGVsYW5nYW5hMQwwCgYDVQQHDANIeWQxDzAN\n"
"BgNVBAoMBlNpbGFiczEPMA0GA1UECwwGbWF0dGVyMQ8wDQYDVQQDDAZjaGFuZHUx\n"
"HDAaBgkqhkiG9w0BCQEWDXNoQHNpbGFicy5jb20wHhcNMjMwMzI0MTIwMzI5WhcN\n"
"MjQwMzE4MTIwMzI5WjCBgDELMAkGA1UEBhMCSU4xEjAQBgNVBAgMCVRlbGFuZ2Fu\n"
"YTEMMAoGA1UEBwwDSHlkMQ8wDQYDVQQKDAZTaWxhYnMxDzANBgNVBAsMBm1hdHRl\n"
"cjEPMA0GA1UEAwwGY2hhbmR1MRwwGgYJKoZIhvcNAQkBFg1zaEBzaWxhYnMuY29t\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEWIaIixoLW8k3JFROV0Uy+ja2Awhv\n"
"TnYXuuogAqbMsCsNB2kix3zIU6d8uYM/0QjVGBo3NT5S32BlvKa1aHaD+TAKBggq\n"
"hkjOPQQDAgNHADBEAiAMm4+cD3en9qYFHnRpl0YmVlj2bWpJ+VSDRlkUru2JdgIg\n"
"CLtDL0HQ9NZt+AwKGYwnBZwJLOMuBWczU4uCcXsFGUM=\n"
"-----END CERTIFICATE-----";

char device_key[] = "------BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIFuiGwgW2Xj8QC+SO9LXNf9v6aSb+1vqAstp2h+dUArzoAoGCCqGSM49\n"
"AwEHoUQDQgAEWIaIixoLW8k3JFROV0Uy+ja2AwhvTnYXuuogAqbMsCsNB2kix3zI\n"
"U6d8uYM/0QjVGBo3NT5S32BlvKa1aHaD+Q==\n"
"-----END EC PRIVATE KEY-----";

#elif USE_MOSQUITTO

#define DIC_SERVER_HOST	"192.168.1.3"
#define DIC_CLIENT_ID "test_client"
#define DIC_CLIENT_USER "explorer1"
#define DIC_CLIENT_PASS "p@ssw0rd"

char ca_certificate[] = "-----BEGIN CERTIFICATE-----\n"
"MIICSDCCAe+gAwIBAgIUNOMkmU35VOUVIGWCsO4c3FoWzZ8wCgYIKoZIzj0EAwIw\n"
"ejELMAkGA1UEBhMCSU4xDDAKBgNVBAgMA1RlbDEMMAoGA1UEBwwDSHlkMQ8wDQYD\n"
"VQQKDAZTaWxhYnMxDzANBgNVBAsMBk1hdHRlcjEOMAwGA1UEAwwFdGhpcnUxHTAb\n"
"BgkqhkiG9w0BCQEWDnRoc0BzaWxhYnMuY29tMB4XDTIzMDcwNTEwMDIwOVoXDTI4\n"
"MDcwNDEwMDIwOVowejELMAkGA1UEBhMCSU4xDDAKBgNVBAgMA1RlbDEMMAoGA1UE\n"
"BwwDSHlkMQ8wDQYDVQQKDAZTaWxhYnMxDzANBgNVBAsMBk1hdHRlcjEOMAwGA1UE\n"
"AwwFdGhpcnUxHTAbBgkqhkiG9w0BCQEWDnRoc0BzaWxhYnMuY29tMFkwEwYHKoZI\n"
"zj0CAQYIKoZIzj0DAQcDQgAEIhCv7wWV5ZpFo+gX94oUwp7ByiCGjjfTYkh4TmA/\n"
"xaZRztYPC/CVQ01cGIapitDI/npC5qR7trimsXQvYQpJgKNTMFEwHQYDVR0OBBYE\n"
"FCFtOVOozHN2Vpd2jPHXnsCeT6L9MB8GA1UdIwQYMBaAFCFtOVOozHN2Vpd2jPHX\n"
"nsCeT6L9MA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDRwAwRAIgeS1WbQPS\n"
"F1jBv0/ZVz2XQYkDEImbGEczR+Mqmtjgp5ECIFDn0RYi0scoiexGvzLh8aqDnkrF\n"
"AZxhjgUh7ahZfm0s\n"
"-----END CERTIFICATE-----";

char device_certificate[] = "-----BEGIN CERTIFICATE-----\n"
"MIIB9jCCAZwCFG9bPw5s6br2s224QOibBkay/MuTMAoGCCqGSM49BAMCMHoxCzAJ\n"
"BgNVBAYTAklOMQwwCgYDVQQIDANUZWwxDDAKBgNVBAcMA0h5ZDEPMA0GA1UECgwG\n"
"U2lsYWJzMQ8wDQYDVQQLDAZNYXR0ZXIxDjAMBgNVBAMMBXRoaXJ1MR0wGwYJKoZI\n"
"hvcNAQkBFg50aHNAc2lsYWJzLmNvbTAeFw0yMzA3MDUxMDA2MzRaFw0yNDA2Mjkx\n"
"MDA2MzRaMIGAMQswCQYDVQQGEwJJTjEMMAoGA1UECAwDVGVsMQwwCgYDVQQHDANI\n"
"eWQxDzANBgNVBAoMBk1hdHRlcjEPMA0GA1UECwwGTWF0dGVyMRQwEgYDVQQDDAt0\n"
"ZXN0X2NsaWVudDEdMBsGCSqGSIb3DQEJARYOdGhzQHNpbGFicy5jb20wWTATBgcq\n"
"hkjOPQIBBggqhkjOPQMBBwNCAARDt4/qZJOJkGa1RlG+ZJ3vyxK/rpYG534JtZZa\n"
"dBVJ5RF4XhD5v6wv9bp/8bpgLcOXZgr8a9xcPqafhkeGIx3DMAoGCCqGSM49BAMC\n"
"A0gAMEUCID+AQD0IvqCUhISSmSWd9P/QxjZ4ju5qqrprZQ6dP0J+AiEA7EXq8LTQ\n"
"D8fSOL3GmGNGXzGmDZu9PN64777woTkaUIw=\n"
"-----END CERTIFICATE-----";

char device_key[] = "-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIE2NFtyMW02j7e8Mbp99tFvTeY1tj+lLc+BbxuWqwHIfoAoGCCqGSM49\n"
"AwEHoUQDQgAEQ7eP6mSTiZBmtUZRvmSd78sSv66WBud+CbWWWnQVSeUReF4Q+b+s\n"
"L/W6f/G6YC3Dl2YK/GvcXD6mn4ZHhiMdww==\n"
"-----END EC PRIVATE KEY-----";
 
#endif //for certificates

#endif // __DIC_CONFIG_H
