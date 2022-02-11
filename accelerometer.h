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

#ifndef ACCELEROMETER_OBJECT_H
#define ACCELEROMETER_OBJECT_H

#include <anjay/anjay.h>

#if (SENSORS_IKS01A2 == 1)

int accelerometer_object_install(anjay_t *anjay);

void accelerometer_object_uninstall(anjay_t *anjay);

void accelerometer_object_update(anjay_t *anjay);

#else // Define dummy functions, compiler optimizes out

static inline int accelerometer_object_install(anjay_t *anjay) {
    return 0;
}

static inline void accelerometer_object_uninstall(anjay_t *anjay) {}

static inline void accelerometer_object_update(anjay_t *anjay) {}

#endif // SENSORS_IKS01A2

#endif // ACCELEROMETER_OBJECT_H
