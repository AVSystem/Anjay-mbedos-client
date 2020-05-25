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

#include "device_config_serial_menu.h"
#include <avs_qspi.h>
#include "default_config.h"
#include <avsystem/commons/avs_url.h>
#include <cctype>
#include <mbed.h>
#include <sstream>

using namespace avs;
using namespace std;

namespace {

int parse_security_mode(const string &server_uri,
                        anjay_security_mode_t *out_security_mode) {
    avs_url_t *parsed_url = avs_url_parse(server_uri.c_str());
    if (!parsed_url) {
        return -1;
    }
    static const char prefix[] = "coaps";
    if (!avs_strncasecmp(avs_url_protocol(parsed_url), prefix,
                         sizeof(prefix))) {
        *out_security_mode = ANJAY_SECURITY_PSK;
    } else {
        *out_security_mode = ANJAY_SECURITY_NOSEC;
    }
    avs_url_free(parsed_url);
    return 0;
}

SerialConfigMenu create_server_config_menu(const char *label,
                                           Lwm2mServerConfig &server_config) {
    return SerialConfigMenu(
            label,
            { SerialConfigMenuEntry(
                      "Server URI",
                      [&]() {
                          string new_server_uri;
                          while (true) {
                              new_server_uri = SerialConfigMenu::read_string(
                                      "Server URI");
                              if (new_server_uri.empty()) {
                                  return SerialConfigMenuEntry::MenuLoopAction::
                                          CONTINUE;
                              }
                              if (parse_security_mode(
                                          new_server_uri,
                                          &server_config.security_mode)) {
                                  printf("Could not parse URI. Try again.\n");
                              } else {
                                  break;
                              }
                          }
                          server_config.server_uri = new_server_uri;
                          server_config.status =
                                  Lwm2mServerConfigStatus::ENABLED;
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return server_config.server_uri; }),
              SerialConfigMenuEntry(
                      "PSK Identity",
                      [&]() {
                          server_config.psk_identity =
                                  SerialConfigMenu::read_string("PSK Identity");
                          server_config.status =
                                  Lwm2mServerConfigStatus::ENABLED;
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return server_config.psk_identity; }),
              SerialConfigMenuEntry(
                      "PSK Key",
                      [&]() {
                          server_config.psk_key =
                                  SerialConfigMenu::read_string("PSK Key");
                          server_config.status =
                                  Lwm2mServerConfigStatus::ENABLED;
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return server_config.psk_key; }),
              SerialConfigMenuEntry(
                      "Enable / Disable",
                      [&]() {
                          if (server_config) {
                              server_config.status =
                                      Lwm2mServerConfigStatus::DISABLED;
                          } else {
                              server_config.status =
                                      Lwm2mServerConfigStatus::ENABLED;
                          }
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return server_config ? "ENABLED" : "DISABLED"; }),
              SerialConfigMenuEntry("Back", []() {
                  return SerialConfigMenuEntry::MenuLoopAction::EXIT;
              }) });
}

} // namespace

anjay_security_instance_t
Lwm2mServerConfig::as_security_instance(anjay_ssid_t ssid) const {
    anjay_security_instance_t instance;
    memset(&instance, 0, sizeof(instance));
    instance.ssid = ssid;
    instance.server_uri = server_uri.c_str();
    instance.bootstrap_server = (ssid == ANJAY_SSID_BOOTSTRAP);
    instance.security_mode = security_mode;
    instance.public_cert_or_psk_identity =
            (const uint8_t *) psk_identity.c_str();
    instance.public_cert_or_psk_identity_size = psk_identity.size();
    instance.private_cert_or_psk_key = (const uint8_t *) psk_key.c_str();
    instance.private_cert_or_psk_key_size = psk_key.size();
    return instance;
}

bool should_show_menu(avs_time_duration_t max_wait_time) {
    const avs_time_real_t deadline =
            avs_time_real_add(avs_time_real_now(), max_wait_time);
    while (avs_time_duration_less(
            avs_time_real_diff(avs_time_real_now(), deadline), max_wait_time)) {
        if (mbed_file_handle(STDIN_FILENO)->readable()) {
            const char character = SerialConfigMenu::get_char();
            if (isprint(character) || isspace(character)) {
                return true;
            }
        }
    }
    return false;
}

void show_menu_and_maybe_update_config(Lwm2mConfig &config) {
    Lwm2mConfig cached_config = config;

    SerialConfigMenu bootstrap_server_config_menu =
            create_server_config_menu("BOOTSTRAP SERVER CONFIGURATION MENU",
                                      cached_config.bs_server_config);

    SerialConfigMenu regular_server_config_menu =
            create_server_config_menu("REGULAR SERVER CONFIGURATION MENU",
                                      cached_config.rg_server_config);

    EnumSelectMenu<avs_log_level_t> log_level_select_menu(
            "LwM2M client log level", cached_config.log_level,
            { { AVS_LOG_TRACE, "trace" },
              { AVS_LOG_DEBUG, "debug" },
              { AVS_LOG_INFO, "info" },
              { AVS_LOG_WARNING, "warning" },
              { AVS_LOG_ERROR, "error" },
              { AVS_LOG_QUIET, "quiet" } });

    using RAT = CellularNetwork::RadioAccessTechnology;
    EnumSelectMenu<RAT> rat_select_menu(
            "Radio Access Technology", cached_config.modem_config.rat,
            { { RAT::RAT_GSM, "GSM" },
              { RAT::RAT_GSM_COMPACT, "GSM COMPACT" },
              { RAT::RAT_EGPRS, "EDGE (EGPRS)" },
              { RAT::RAT_UTRAN, "UTRAN" },
              { RAT::RAT_E_UTRAN, "E-UTRAN" },
              { RAT::RAT_HSDPA, "HSDPA" },
              { RAT::RAT_HSUPA, "HSUPA" },
              { RAT::RAT_HSDPA_HSUPA, "HSPA" },
              { RAT::RAT_CATM1, "LTE Cat M1" },
              { RAT::RAT_NB1, "LTE Cat NB1" } });

    SerialConfigMenu modem_config_menu(
            "Modem configuration",
            { SerialConfigMenuEntry(
                      "APN",
                      [&]() {
                          cached_config.modem_config.apn =
                                  SerialConfigMenu::read_string("APN");
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return cached_config.modem_config.apn; }),
              SerialConfigMenuEntry(
                      "Username",
                      [&]() {
                          cached_config.modem_config.username =
                                  SerialConfigMenu::read_string("Username");
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return cached_config.modem_config.username; }),
              SerialConfigMenuEntry(
                      "Password",
                      [&]() {
                          cached_config.modem_config.password =
                                  SerialConfigMenu::read_string("Password");
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return cached_config.modem_config.password; }),
              SerialConfigMenuEntry(
                      "SIM PIN Code",
                      [&]() {
                          cached_config.modem_config.sim_pin_code =
                                  SerialConfigMenu::read_string("SIM PIN Code");
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() {
                          return cached_config.modem_config.sim_pin_code;
                      }),
              SerialConfigMenuEntry(
                      "Radio Access Technology",
                      [&]() {
                          cached_config.modem_config.rat =
                                  rat_select_menu.show_and_get_value();
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() { return rat_select_menu.get_value_as_string(); }),
              SerialConfigMenuEntry("Back", [&]() {
                  return SerialConfigMenuEntry::MenuLoopAction::EXIT;
              }) });

    SerialConfigMenu main_menu(
            "DEVICE CONFIGURATION MENU",
            { SerialConfigMenuEntry(
                      "Bootstrap Server configuration",
                      [&]() {
                          bootstrap_server_config_menu
                                  .keep_showing_until_exit_command();
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() {
                          return cached_config.bs_server_config
                                         ? cached_config.bs_server_config
                                                   .server_uri
                                         : "DISABLED";
                      }),
              SerialConfigMenuEntry(
                      "Regular Server configuration",
                      [&]() {
                          regular_server_config_menu
                                  .keep_showing_until_exit_command();
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() {
                          return cached_config.rg_server_config
                                         ? cached_config.rg_server_config
                                                   .server_uri
                                         : "DISABLED";
                      }),
              SerialConfigMenuEntry(
                      "LwM2M client log level",
                      [&]() {
                          cached_config.log_level =
                                  log_level_select_menu.show_and_get_value();
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() {
                          return log_level_select_menu.get_value_as_string();
                      }),
              SerialConfigMenuEntry(
                      "Modem configuration",
                      [&]() {
                          modem_config_menu.keep_showing_until_exit_command();
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() {
                          return cached_config.modem_config.apn.empty()
                                         ? "APN not specified"
                                         : "APN = "
                                                   + cached_config.modem_config
                                                             .apn;
                      }),
              SerialConfigMenuEntry(
                      "Exit & save changes",
                      [&]() {
                          config = cached_config;
                          return SerialConfigMenuEntry::MenuLoopAction::EXIT;
                      }),
              SerialConfigMenuEntry("Exit & discard changes", []() {
                  return SerialConfigMenuEntry::MenuLoopAction::EXIT;
              }) });

    main_menu.keep_showing_until_exit_command();
}

namespace {

#if MBED_CONF_APP_WITH_DTLS
constexpr anjay_security_mode_t INITIAL_SECURITY_MODE = ANJAY_SECURITY_PSK;
#else
constexpr anjay_security_mode_t INITIAL_SECURITY_MODE = ANJAY_SECURITY_NOSEC;
#endif
#if MBED_CONF_APP_WITH_BS_SERVER
constexpr Lwm2mServerConfigStatus INITIAL_BS_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::ENABLED;
#else
constexpr Lwm2mServerConfigStatus INITIAL_BS_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::DISABLED;
#endif
#if MBED_CONF_APP_WITH_RG_SERVER
constexpr Lwm2mServerConfigStatus INITIAL_RG_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::ENABLED;
#else
constexpr Lwm2mServerConfigStatus INITIAL_RG_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::DISABLED;
#endif

} // namespace

ModemConfig::ModemConfig()
        :
#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_APN
          apn(MBED_CONF_NSAPI_DEFAULT_CELLULAR_APN),
#else  // MBED_CONF_NSAPI_DEFAULT_CELLULAR_APN
          apn(),
#endif // MBED_CONF_NSAPI_DEFAULT_CELLULAR_APN
#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_USERNAME
          username(MBED_CONF_NSAPI_DEFAULT_CELLULAR_USERNAME),
#else  // MBED_CONF_NSAPI_DEFAULT_CELLULAR_USERNAME
          username(),
#endif // MBED_CONF_NSAPI_DEFAULT_CELLULAR_USERNAME
#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_PASSWORD
          password(MBED_CONF_NSAPI_DEFAULT_CELLULAR_PASSWORD),
#else  // MBED_CONF_NSAPI_DEFAULT_CELLULAR_PASSWORD
          password(),
#endif // MBED_CONF_NSAPI_DEFAULT_CELLULAR_PASSWORD
#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_SIM_PIN
          sim_pin_code(MBED_CONF_NSAPI_DEFAULT_CELLULAR_SIM_PIN),
#else  // MBED_CONF_NSAPI_DEFAULT_CELLULAR_SIM_PIN
          sim_pin_code(),
#endif // MBED_CONF_NSAPI_DEFAULT_CELLULAR_SIM_PIN
#ifdef MBED_CONF_CELLULAR_RADIO_ACCESS_TECHNOLOGY
          rat(MBED_CONF_CELLULAR_RADIO_ACCESS_TECHNOLOGY)
#else  // MBED_CONF_CELLULAR_RADIO_ACCESS_TECHNOLOGY
          rat(CellularNetwork::RadioAccessTechnology::RAT_UNKNOWN)
#endif // MBED_CONF_CELLULAR_RADIO_ACCESS_TECHNOLOGY
{
}

Lwm2mConfig::Lwm2mConfig()
        : Lwm2mConfig(Lwm2mServerConfig(INITIAL_BS_SERVER_CONFIG_STATUS,
                                        INITIAL_SECURITY_MODE,
                                        LWM2M_BS_URI,
                                        PSK_IDENTITY,
                                        PSK_KEY),
                      Lwm2mServerConfig(INITIAL_RG_SERVER_CONFIG_STATUS,
                                        INITIAL_SECURITY_MODE,
                                        LWM2M_URI,
                                        PSK_IDENTITY,
                                        PSK_KEY),
                      AVS_LOG_DEBUG
                      ) {}

constexpr size_t CONFIG_PERSISTENCE_SIZE = 2 * QSPI_ERASE_PAGE_SIZE;

constexpr bd_addr_t CONFIG_PERSISTENCE_ADDR_BEGIN =
        QSPI_FLASH_SIZE - CONFIG_PERSISTENCE_SIZE;

constexpr bd_addr_t CONFIG_PERSISTENCE_ADDR_END =
        CONFIG_PERSISTENCE_ADDR_BEGIN + CONFIG_PERSISTENCE_SIZE;


AVS_STATIC_ASSERT(CONFIG_PERSISTENCE_ADDR_END <= QSPI_FLASH_SIZE,
                  config_persistence_size_too_big);
AVS_STATIC_ASSERT(CONFIG_PERSISTENCE_ADDR_BEGIN <= CONFIG_PERSISTENCE_ADDR_END,
                  config_persistence_size_must_be_positive);

Lwm2mConfigPersistence::Lwm2mConfigPersistence()
        : qspi_(QSPI_FLASH1_IO0,
                QSPI_FLASH1_IO1,
                QSPI_FLASH1_IO2,
                QSPI_FLASH1_IO3,
                QSPI_FLASH1_SCK,
                QSPI_FLASH1_CSN,
                QSPIF_POLARITY_MODE_0),
          sliced_qspi_(&qspi_,
                       CONFIG_PERSISTENCE_ADDR_BEGIN,
                       CONFIG_PERSISTENCE_ADDR_END),
          store_(&sliced_qspi_) {
    qspi_.init();
    sliced_qspi_.init();
    store_.init();
}

Lwm2mConfigPersistence::~Lwm2mConfigPersistence() {
    store_.deinit();
    sliced_qspi_.deinit();
    qspi_.deinit();
}

namespace config_persistence_keys {
const char *bs_server_status = "bs_server_status";
const char *bs_server_security_mode = "bs_server_security_mode";
const char *bs_server_uri = "bs_server_uri";
const char *bs_server_psk_identity = "bs_server_psk_identity";
const char *bs_server_psk_key = "bs_server_psk_key";

const char *rg_server_status = "rg_server_status";
const char *rg_server_security_mode = "rg_server_security_mode";
const char *rg_server_uri = "rg_server_uri";
const char *rg_server_psk_identity = "rg_server_psk_identity";
const char *rg_server_psk_key = "rg_server_psk_key";

const char *log_level = "log_level";

const char *apn = "apn";
const char *username = "username";
const char *password = "password";
const char *sim_pin_code = "sim_pin_code";
const char *rat = "rat";
} // namespace config_persistence_keys

template <typename T>
int load_key(TDBStore &store, const char *key, T &loaded_value) {
    return store.get(key, &loaded_value, sizeof(loaded_value));
}

template <>
int load_key(TDBStore &store, const char *key, std::string &loaded_value) {
    char buffer[128];
    size_t actual_size;
    int result;
    if ((result = store.get(key, buffer, sizeof(buffer), &actual_size))) {
        return result;
    }
    loaded_value = string(buffer, actual_size);
    return result;
}

template <typename T>
int save_key(TDBStore &store, const char *key, const T &value) {
    return store.set(key, &value, sizeof(value), 0);
}

template <>
int save_key(TDBStore &store, const char *key, const string &value) {
    return store.set(key, value.c_str(), value.length(), 0);
}

template <typename T>
int key_persistence(Lwm2mConfigPersistence::Direction direction,
                    TDBStore &store,
                    const char *key,
                    T &value) {
    switch (direction) {
    case Lwm2mConfigPersistence::Direction::STORE:
        return save_key(store, key, value);
    case Lwm2mConfigPersistence::Direction::RESTORE:
        return load_key(store, key, value);
    };
    AVS_UNREACHABLE("invalid enum value");
    return -1;
}

int Lwm2mConfigPersistence::persistence(Direction direction,
                                        Lwm2mConfig &config) {
    using namespace config_persistence_keys;
    int result;
    (void) ((result = key_persistence(direction, store_, bs_server_status,
                                      config.bs_server_config.status))
            || (result = key_persistence(direction, store_,
                                         bs_server_security_mode,
                                         config.bs_server_config.security_mode))
            || (result = key_persistence(direction, store_, bs_server_uri,
                                         config.bs_server_config.server_uri))
            || (result = key_persistence(direction, store_,
                                         bs_server_psk_identity,
                                         config.bs_server_config.psk_identity))
            || (result = key_persistence(direction, store_, bs_server_psk_key,
                                         config.bs_server_config.psk_key))
            || (result = key_persistence(direction, store_, rg_server_status,
                                         config.rg_server_config.status))
            || (result = key_persistence(direction, store_,
                                         rg_server_security_mode,
                                         config.rg_server_config.security_mode))
            || (result = key_persistence(direction, store_, rg_server_uri,
                                         config.rg_server_config.server_uri))
            || (result = key_persistence(direction, store_,
                                         rg_server_psk_identity,
                                         config.rg_server_config.psk_identity))
            || (result = key_persistence(direction, store_, rg_server_psk_key,
                                         config.rg_server_config.psk_key))
            || (result = key_persistence(direction, store_, log_level,
                                         config.log_level))
            || (result = key_persistence(direction, store_, apn,
                                         config.modem_config.apn))
            || (result = key_persistence(direction, store_, username,
                                         config.modem_config.username))
            || (result = key_persistence(direction, store_, password,
                                         config.modem_config.password))
            || (result = key_persistence(direction, store_, sim_pin_code,
                                         config.modem_config.sim_pin_code))
            || (result = key_persistence(direction, store_, rat,
                                         config.modem_config.rat)));
    return result;
}
