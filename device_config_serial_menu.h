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

#ifndef DEVICE_CONFIG_SERIAL_MENU_H
#define DEVICE_CONFIG_SERIAL_MENU_H

#include "serial_menu.h"
#include <CellularNetwork.h>
#ifdef COMPONENT_QSPIF
#include <QSPIFBlockDevice.h>
#include <SlicingBlockDevice.h>
#include <TDBStore.h>
#endif // COMPONENT_QSPIF
#include <anjay/anjay_config.h>
#include <anjay/dm.h>
#include <anjay/security.h>
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_time.h>

enum class Lwm2mServerConfigStatus { ENABLED, DISABLED };

struct Lwm2mServerConfig {
    Lwm2mServerConfigStatus status;
    anjay_security_mode_t security_mode;
    std::string server_uri;
    std::string psk_identity;
    std::string psk_key;

    Lwm2mServerConfig(Lwm2mServerConfigStatus status,
                      anjay_security_mode_t security_mode,
                      std::string &&server_uri,
                      std::string &&psk_identity,
                      std::string &&psk_key)
            : status(status),
              security_mode(security_mode),
              server_uri(std::move(server_uri)),
              psk_identity(std::move(psk_identity)),
              psk_key(std::move(psk_key)) {}

    explicit operator bool() {
        return status == Lwm2mServerConfigStatus::ENABLED;
    }

    anjay_security_instance_t as_security_instance(anjay_ssid_t ssid) const;
};

struct ModemConfig {
    std::string apn;
    std::string username;
    std::string password;
    std::string sim_pin_code;
    mbed::CellularNetwork::RadioAccessTechnology rat;

    ModemConfig();
};

struct Lwm2mConfig {
    Lwm2mServerConfig bs_server_config;
    Lwm2mServerConfig rg_server_config;
    avs_log_level_t log_level;
    ModemConfig modem_config;

    Lwm2mConfig(Lwm2mServerConfig &&bs_server_config,
                Lwm2mServerConfig &&rg_server_config,
                avs_log_level_t log_level
                )
            : bs_server_config(std::move(bs_server_config)),
              rg_server_config(std::move(rg_server_config)),
              log_level(log_level)
              {}

    Lwm2mConfig();
};

bool should_show_menu(avs_time_duration_t max_wait_time);

void show_menu_and_maybe_update_config(Lwm2mConfig &config);

#ifdef COMPONENT_QSPIF
class Lwm2mConfigPersistence {
    QSPIFBlockDevice qspi_;

    struct QspiUtils {
        mbed::SlicingBlockDevice sliced_qspi;
        mbed::TDBStore store;

        QspiUtils(BlockDevice *bd, bd_addr_t start, bd_addr_t stop);
        ~QspiUtils();
    };
    std::unique_ptr<QspiUtils> utils_;

public:
    enum class Direction { STORE, RESTORE };
    Lwm2mConfigPersistence();
    ~Lwm2mConfigPersistence();
    int persistence(Direction direction, Lwm2mConfig &config);
};
#endif // COMPONENT_QSPIF

#endif // DEVICE_CONFIG_SERIAL_MENU_H
