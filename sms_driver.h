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

#ifndef SMS_DRIVER_H
#define SMS_DRIVER_H

#include "mbed.h"

#if MBED_CONF_CELLULAR_USE_SMS

#ifndef SMS_FEATURE_AVAILABLE
#error "SMS feature is only available in commercial versions of Anjay"
#endif // SMS_FEATURE_AVAILABLE

#include <CellularSMS.h>
#include <anjay/core.h>

anjay_smsdrv_t *nrf_smsdrv_create(mbed::CellularSMS *sms);
bool nrf_smsdrv_has_unread(anjay_smsdrv_t *smsdrv);

#endif // MBED_CONF_CELLULAR_USE_SMS

#endif // SMS_DRIVER_H
