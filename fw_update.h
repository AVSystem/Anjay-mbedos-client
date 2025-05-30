/*
 * Copyright 2020-2025 AVSystem <avsystem@avsystem.com>
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

#ifndef FW_UPDATE_OBJECT_H
#define FW_UPDATE_OBJECT_H
#include <anjay/anjay.h>

#ifdef MBED_CLOUD_CLIENT_FOTA_ENABLE

int fw_update_object_install(anjay_t *anjay);

#endif // MBED_CLOUD_CLIENT_FOTA_ENABLE

#endif // FW_UPDATE_OBJECT_H
