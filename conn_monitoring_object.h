/*
 * Copyright 2020 AVSystem <avsystem@avsystem.com>
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

#ifndef CONN_MONITORING_OBJECT_H
#define CONN_MONITORING_OBJECT_H

#include <CellularContext.h>
#include <CellularNetwork.h>
#include <anjay/anjay.h>

#define CONN_MONITORING_OID 4

#define CONN_MONITORING_RID_RADIO_SIGNAL_STRENGTH 2

int conn_monitoring_object_install(anjay_t *anjay,
                                   mbed::CellularContext *cell_ctx,
                                   mbed::CellularNetwork *net);

void conn_monitoring_object_uninstall(anjay_t *anjay);

#endif // CONN_MONITORING_OBJECT_H
