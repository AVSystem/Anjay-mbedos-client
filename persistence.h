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

#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <anjay/anjay.h>

#if MBED_CONF_APP_WITH_EST
#ifndef EST_FEATURE_AVAILABLE
#error "EST feature is only available in commercial versions of Anjay"
#endif // EST_FEATURE_AVAILABLE
#endif // MBED_CONF_APP_WITH_EST

int persistence_purge();
int restore_anjay_from_persistence(anjay_t *anjay);
int persist_anjay_if_required(anjay_t *anjay);
#endif // PERSISTENCE_H
