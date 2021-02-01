/*
 * Copyright 2020-2021 AVSystem <avsystem@avsystem.com>
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

#ifndef HUMIDITY_OBJECT_H
#define HUMIDITY_OBJECT_H

#include <anjay/anjay.h>

#ifdef TARGET_DISCO_L496AG

#    define HUMIDITY_OID 3304

#    define HUMIDITY_RID_SENSOR_VALUE 5700

int humidity_object_install(anjay_t *anjay);

void humidity_object_uninstall(anjay_t *anjay);

void humidity_object_update(anjay_t *anjay);

#endif // TARGET_DISCO_L496AG

#endif // HUMIDITY_OBJECT_H
