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

#ifndef BAROMETER_OBJECT_H
#define BAROMETER_OBJECT_H

#include <anjay/anjay.h>

#if (SENSORS_IKS01A2 == 1)

int barometer_object_install(anjay_t *anjay);
void barometer_object_uninstall(anjay_t *anjay);
void barometer_object_update(anjay_t *anjay);

#else // Dummy functions if sensor not present

static inline int barometer_object_install(anjay_t *anjay) {
    return 0;
}

static inline void barometer_object_uninstall(anjay_t *anjay) {}

static inline void barometer_object_update(anjay_t *anjay) {}

#endif // SENSORS_IKS01A2
#endif // BAROMETER_OBJECT_H
