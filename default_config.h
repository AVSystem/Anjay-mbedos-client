/*
 * Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEFAULT_CONFIG_H
#define DEFAULT_CONFIG_H

#define DEFAULT_ENDPOINT_NAME MBED_CONF_APP_ENDPOINT_NAME
#define LWM2M_NOSEC_ADDR MBED_CONF_APP_RG_NOSEC_ADDR
#define LWM2M_DTLS_ADDR MBED_CONF_APP_RG_DTLS_ADDR
#define LWM2M_BS_NOSEC_ADDR MBED_CONF_APP_BS_NOSEC_ADDR
#define LWM2M_BS_DTLS_ADDR MBED_CONF_APP_BS_DTLS_ADDR
#define PSK_IDENTITY MBED_CONF_APP_PSK_IDENTITY
#define PSK_KEY MBED_CONF_APP_PSK_KEY
#define SMS_BINDING_LOCAL_PHONE_NUMBER \
    MBED_CONF_APP_SMS_BINDING_LOCAL_PHONE_NUMBER
#define SMS_BINDING_SERVER_PHONE_NUMBER \
    MBED_CONF_APP_SMS_BINDING_SERVER_PHONE_NUMBER

#if MBED_CONF_APP_WITH_DTLS
#define LWM2M_URI LWM2M_DTLS_ADDR
#define LWM2M_BS_URI LWM2M_BS_DTLS_ADDR
#else // MBED_CONF_APP_WITH_DTLS
#define LWM2M_URI LWM2M_NOSEC_ADDR
#define LWM2M_BS_URI LWM2M_BS_NOSEC_ADDR
#endif // MBED_CONF_APP_WITH_DTLS

#if !MBED_CONF_APP_WITH_RG_SERVER && !MBED_CONF_APP_WITH_BS_SERVER
#error "No LwM2M Server and no LwM2M Bootstrap Server enabled"
#endif

#endif // DEFAULT_CONFIG_H
