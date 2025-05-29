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

#include "device_config_serial_menu.h"
#include "default_config.h"
#include "persistence.h"
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_url.h>
#include <cctype>
#include <mbed.h>
#include <sstream>

#include <kvstore_global_api/kvstore_global_api.h>

using namespace std;

#if !MBED_CONF_APP_WITH_EST
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
#endif // !MBED_CONF_APP_WITH_EST

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

#if !MBED_CONF_APP_WITH_EST
    SerialConfigMenu bootstrap_server_config_menu =
            create_server_config_menu("BOOTSTRAP SERVER CONFIGURATION MENU",
                                      cached_config.bs_server_config);

    SerialConfigMenu regular_server_config_menu =
            create_server_config_menu("REGULAR SERVER CONFIGURATION MENU",
                                      cached_config.rg_server_config);
#endif // !MBED_CONF_APP_WITH_EST

    SerialConfigMenu persistence_config_menu(
            "Persistence configuration",
            { SerialConfigMenuEntry(
                      "Enable / Disable",
                      [&]() {
                          cached_config.persistence_enabled =
                                  !cached_config.persistence_enabled;
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      },
                      [&]() {
                          return cached_config.persistence_enabled ? "ENABLED"
                                                                   : "DISABLED";
                      }),
              SerialConfigMenuEntry(
                      "Purge persistence (applies immediately)",
                      [&]() {
                          printf(persistence_purge()
                                         ? "Could not purge persistence"
                                         : "Successfully purged persistence");
                          return SerialConfigMenuEntry::MenuLoopAction::
                                  CONTINUE;
                      }),
              SerialConfigMenuEntry("Back", [&]() {
                  return SerialConfigMenuEntry::MenuLoopAction::EXIT;
              }) });

    EnumSelectMenu<avs_log_level_t> log_level_select_menu(
            "LwM2M client log level", cached_config.log_level,
            { { AVS_LOG_TRACE, "trace" },
              { AVS_LOG_DEBUG, "debug" },
              { AVS_LOG_INFO, "info" },
              { AVS_LOG_WARNING, "warning" },
              { AVS_LOG_ERROR, "error" },
              { AVS_LOG_QUIET, "quiet" } });
#ifdef ANJAY_WITH_LWM2M11
    EnumSelectMenu<anjay_lwm2m_version_t> lwm2m_version_select_menu(
            "LwM2M version", cached_config.maximum_version,
            { { ANJAY_LWM2M_VERSION_1_0, "1.0" },
              { ANJAY_LWM2M_VERSION_1_1, "1.1" } });
#endif // ANJAY_WITH_LWM2M11

#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
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
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR

    SerialConfigMenu main_menu("DEVICE CONFIGURATION MENU", {
#if !MBED_CONF_APP_WITH_EST
        SerialConfigMenuEntry(
                "Bootstrap Server configuration",
                [&]() {
                    bootstrap_server_config_menu
                            .keep_showing_until_exit_command();
                    return SerialConfigMenuEntry::MenuLoopAction::CONTINUE;
                },
                [&]() {
                    return cached_config.bs_server_config
                                   ? cached_config.bs_server_config.server_uri
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
#endif // !MBED_CONF_APP_WITH_EST
                SerialConfigMenuEntry(
                        "Persistence configuration",
                        [&]() {
                            persistence_config_menu
                                    .keep_showing_until_exit_command();
                            return SerialConfigMenuEntry::MenuLoopAction::
                                    CONTINUE;
                        },
                        [&]() {
                            return cached_config.persistence_enabled
                                           ? "ENABLED"
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
#ifdef ANJAY_WITH_LWM2M11
                SerialConfigMenuEntry("LwM2M Version",
                                      [&]() {
                                          cached_config.maximum_version =
                                                  lwm2m_version_select_menu
                                                          .show_and_get_value();
                                          return SerialConfigMenuEntry::
                                                  MenuLoopAction::CONTINUE;
                                      },
                                      [&]() {
                                          return lwm2m_version_select_menu
                                                  .get_value_as_string();
                                      }),
#endif // ANJAY_WITH_LWM2M11
#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
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
                                                     + cached_config
                                                               .modem_config
                                                               .apn;
                        }),
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
                SerialConfigMenuEntry("Load default configuration",
                                      [&]() {
                                          cached_config = Lwm2mConfig();
                                          return SerialConfigMenuEntry::
                                                  MenuLoopAction::CONTINUE;
                                      }),
                SerialConfigMenuEntry(
                        "Exit & save changes",
                        [&]() {
                            config = cached_config;
                            return SerialConfigMenuEntry::MenuLoopAction::EXIT;
                        }),
                SerialConfigMenuEntry("Exit & discard changes", []() {
                    return SerialConfigMenuEntry::MenuLoopAction::EXIT;
                })
    });

    main_menu.keep_showing_until_exit_command();
}

namespace {

#if MBED_CONF_APP_WITH_DTLS
constexpr anjay_security_mode_t INITIAL_SECURITY_MODE = ANJAY_SECURITY_PSK;
#else  // MBED_CONF_APP_WITH_DTLS
constexpr anjay_security_mode_t INITIAL_SECURITY_MODE = ANJAY_SECURITY_NOSEC;
#endif // MBED_CONF_APP_WITH_DTLS
#if MBED_CONF_APP_WITH_BS_SERVER
constexpr Lwm2mServerConfigStatus INITIAL_BS_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::ENABLED;
#else  // MBED_CONF_APP_WITH_BS_SERVER
constexpr Lwm2mServerConfigStatus INITIAL_BS_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::DISABLED;
#endif // MBED_CONF_APP_WITH_BS_SERVER
#if MBED_CONF_APP_WITH_RG_SERVER
constexpr Lwm2mServerConfigStatus INITIAL_RG_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::ENABLED;
#else  // MBED_CONF_APP_WITH_RG_SERVER
constexpr Lwm2mServerConfigStatus INITIAL_RG_SERVER_CONFIG_STATUS =
        Lwm2mServerConfigStatus::DISABLED;
#endif // MBED_CONF_APP_WITH_RG_SERVER

} // namespace

#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
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
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR

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
                      false,
                      AVS_LOG_DEBUG
#ifdef ANJAY_WITH_LWM2M11
                      ,
                      ANJAY_LWM2M_VERSION_1_1
#endif // ANJAY_WITH_LWM2M11
          ) {
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

const char *persistence_enabled = "persistence_enabled";
const char *log_level = "log_level";
#ifdef ANJAY_WITH_LM2M11
const char *maximum_version = "maximum_version";
#endif // ANJAY_WITH_LWM2M11

const char *apn = "apn";
const char *username = "username";
const char *password = "password";
const char *sim_pin_code = "sim_pin_code";
const char *rat = "rat";
} // namespace config_persistence_keys

namespace {

#define KV_KEY_PREFIX ("/" AVS_QUOTE_MACRO(MBED_CONF_STORAGE_DEFAULT_KV) "/")

string kv_key_wrap(const char *key) {
    static const string prefix = KV_KEY_PREFIX;
    return prefix + key;
}

template <typename T>
int load_key(const char *key, T &loaded_value) {
    size_t actual_size;
    int result;
    if ((result = kv_get(kv_key_wrap(key).c_str(), &loaded_value,
                         sizeof(loaded_value), &actual_size))) {
        return result;
    }
    if (actual_size != sizeof(loaded_value)) {
        return MBED_ERROR_INVALID_DATA_DETECTED;
    }
    return 0;
}

template <>
int load_key(const char *key, string &loaded_value) {
    char buffer[128];
    size_t actual_size;
    int result;
    if ((result = kv_get(kv_key_wrap(key).c_str(), buffer, sizeof(buffer),
                         &actual_size))) {
        return result;
    }
    loaded_value = string(buffer, actual_size);
    return result;
}

template <typename T>
int save_key(const char *key, const T &value) {
    return kv_set(kv_key_wrap(key).c_str(), &value, sizeof(value), 0);
}

template <>
int save_key(const char *key, const string &value) {
    return kv_set(kv_key_wrap(key).c_str(), value.c_str(), value.length(), 0);
}

template <typename T>
int key_persistence(Lwm2mConfigPersistence::Direction direction,
                    const char *key,
                    T &value) {
    switch (direction) {
    case Lwm2mConfigPersistence::Direction::STORE:
        return save_key(key, value);
    case Lwm2mConfigPersistence::Direction::RESTORE:
        return load_key(key, value);
    };
    AVS_UNREACHABLE("invalid enum value");
    return -1;
}

} // namespace

int Lwm2mConfigPersistence::persistence(Direction direction,
                                        Lwm2mConfig &config) {
    using namespace config_persistence_keys;
    int result;
    (void) ((direction == Lwm2mConfigPersistence::Direction::STORE
             && (result = kv_reset(KV_KEY_PREFIX)))
            || (result = key_persistence(direction, bs_server_status,
                                         config.bs_server_config.status))
            || (result = key_persistence(direction, bs_server_security_mode,
                                         config.bs_server_config.security_mode))
            || (result = key_persistence(direction, bs_server_uri,
                                         config.bs_server_config.server_uri))
            || (result = key_persistence(direction, bs_server_psk_identity,
                                         config.bs_server_config.psk_identity))
            || (result = key_persistence(direction, bs_server_psk_key,
                                         config.bs_server_config.psk_key))
            || (result = key_persistence(direction, rg_server_status,
                                         config.rg_server_config.status))
            || (result = key_persistence(direction, rg_server_security_mode,
                                         config.rg_server_config.security_mode))
            || (result = key_persistence(direction, rg_server_uri,
                                         config.rg_server_config.server_uri))
            || (result = key_persistence(direction, rg_server_psk_identity,
                                         config.rg_server_config.psk_identity))
            || (result = key_persistence(direction, rg_server_psk_key,
                                         config.rg_server_config.psk_key))
            || (result = key_persistence(direction, persistence_enabled,
                                         config.persistence_enabled))
            || (result =
                        key_persistence(direction, log_level, config.log_level))
#ifdef ANJAY_WITH_LM2M11
            || (result = key_persistence(direction, maximum_version,
                                         config.maximum_version))
#endif // ANJAY_WITH_LWM2M11
#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
            || (result = key_persistence(direction, apn,
                                         config.modem_config.apn))
            || (result = key_persistence(direction, username,
                                         config.modem_config.username))
            || (result = key_persistence(direction, password,
                                         config.modem_config.password))
            || (result = key_persistence(direction, sim_pin_code,
                                         config.modem_config.sim_pin_code))
            || (result = key_persistence(direction, rat,
                                         config.modem_config.rat))
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
    );
    return result;
}
