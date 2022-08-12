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

#if MBED_CONF_APP_WITH_EST
#include "anjay-mbed-fota/anjay_mbed_fota_conversions.h"
#endif // MBED_CONF_APP_WITH_EST
#include "BufferedSerial.h"
#include "CellularContext.h"
#include "CellularDevice.h"
#include "QUECTEL_BG96.h"
#include "avs_socket_global.h"
#include "conn_monitoring_object.h"
#include "device_config_serial_menu.h"
#include "device_object.h"
#ifdef MBED_CLOUD_CLIENT_FOTA_ENABLE
#include "fw_update.h"
#endif // MBED_CLOUD_CLIENT_FOTA_ENABLE
#include "persistence.h"
#include "sms_driver.h"
#include <EthernetInterface.h>
#include <anjay/access_control.h>
#include <anjay/anjay.h>
#include <anjay/anjay_config.h>
#include <anjay/attr_storage.h>
#include <anjay/security.h>
#include <anjay/server.h>
#include <avsystem/commons/avs_log.h>
#include <inttypes.h>
#include <mbed.h>
#include <mbed_mem_trace.h>
#include <mbed_trace.h>
#include <memory>

#include "joystick.h" // gets activated with TARGET_DISCO_L496AG

#include "accelerometer.h" // Gets activated with SENSORS_IKS01A2 == 1
#include "barometer.h"
#include "humidity.h"
#include "magnetometer.h"

#include "default_config.h"

#if defined(ANJAY_WITH_SMS) && MBED_CONF_CELLULAR_USE_SMS
#define WITH_SMS 1
#endif // ANJAY_WITH_SMS && MBED_CONF_CELLULAR_USE_SMS

namespace {

#ifdef WITH_SMS
struct ServeSmsArgs {
    anjay_t *anjay;
    anjay_smsdrv_t *smsdrv;
};

void serve_sms(avs_sched_t *sched, const void *args_) {
    const ServeSmsArgs *args = reinterpret_cast<const ServeSmsArgs *>(args_);

    for (const auto &it : avs::ListView<const anjay_socket_entry_t>(
                 anjay_get_socket_entries(args->anjay))) {
        if (it.transport == ANJAY_SOCKET_TRANSPORT_SMS) {
            if (nrf_smsdrv_has_unread(args->smsdrv)) {
                anjay_serve(args->anjay, it.socket);
            }
            break;
        }
    }

    AVS_SCHED_DELAYED(sched, nullptr,
                      avs_time_duration_from_scalar(1, AVS_TIME_S), serve_sms,
                      args, sizeof(*args));
}
#endif // WITH_SMS

// We hold these as globals because we cannot pass them two via lambda capture
// to lwm2m_serve() (see MBED_ENABLE_IF_CALLBACK_COMPATIBLE in
// mbed-os/platform/Callback.h)
CellularNetwork *NETWORK;
Lwm2mConfig SERIAL_MENU_CONFIG;

#if MBED_CONF_APP_WITH_EST
constexpr const char client_pub_cert_pem[] =
        AVS_QUOTE_MACRO((MBED_CONF_APP_EST_CLIENT_PUB_CERT));
constexpr const char client_priv_key_pem[] =
        AVS_QUOTE_MACRO((MBED_CONF_APP_EST_CLIENT_PRIV_KEY));
constexpr const char server_pub_cert_pem[] =
        AVS_QUOTE_MACRO((MBED_CONF_APP_EST_SERVER_PUB_CERT));
constexpr auto client_pub_cert =
        avs::constexpr_conversions::pem2der<sizeof(client_pub_cert_pem),
                                            client_pub_cert_pem>();
constexpr auto client_priv_key =
        avs::constexpr_conversions::pem2der<sizeof(client_priv_key_pem),
                                            client_priv_key_pem>();
constexpr auto server_pub_cert =
        avs::constexpr_conversions::pem2der<sizeof(server_pub_cert_pem),
                                            server_pub_cert_pem>();

int configure_servers_from_est(anjay_t *anjay) {

    int result;

    anjay_security_instance_t security_instance;
    memset(&security_instance, 0, sizeof(security_instance));
    security_instance.bootstrap_server = true;
    security_instance.server_uri = LWM2M_BS_DTLS_ADDR;
    security_instance.security_mode = ANJAY_SECURITY_EST;
    security_instance.public_cert =
            avs_crypto_certificate_chain_info_from_buffer(
                    (const void *) client_pub_cert.data(),
                    client_pub_cert.size());
    security_instance.private_key = avs_crypto_private_key_info_from_buffer(
            (const void *) client_priv_key.data(), client_priv_key.size(),
            nullptr);
    security_instance.server_public_key =
            (const uint8_t *) server_pub_cert.data();
    security_instance.server_public_key_size = server_pub_cert.size();

    anjay_iid_t security_instance_iid = ANJAY_ID_INVALID;

    if ((result = anjay_security_object_add_instance(anjay, &security_instance,
                                                     &security_instance_iid))) {
        return result;
    }

    return 0;
}
#else  // MBED_CONF_APP_WITH_EST
int configure_servers_from_config(anjay_t *anjay) {
    int result;

    if (SERIAL_MENU_CONFIG.bs_server_config) {
        anjay_iid_t bs_instance_iid = 0;
        const anjay_security_instance_t bs_instance =
                SERIAL_MENU_CONFIG.bs_server_config.as_security_instance(
                        ANJAY_SSID_BOOTSTRAP);
        if ((result = anjay_security_object_add_instance(anjay, &bs_instance,
                                                         &bs_instance_iid))) {
            avs_log(lwm2m, ERROR,
                    "could not add bootstrap server security instance");
            return result;
        }
    }

    if (SERIAL_MENU_CONFIG.rg_server_config) {
        anjay_iid_t rg_instance_iid =
                (SERIAL_MENU_CONFIG.bs_server_config ? 1 : 0);
        const anjay_security_instance_t rg_instance =
                SERIAL_MENU_CONFIG.rg_server_config.as_security_instance(1);
        if ((result = anjay_security_object_add_instance(anjay, &rg_instance,
                                                         &rg_instance_iid))) {
            avs_log(lwm2m, ERROR,
                    "could not add regular server security instance");
            return result;
        }
    }

    if (SERIAL_MENU_CONFIG.rg_server_config) {
        anjay_iid_t server_instance_iid = 0;
        anjay_server_instance_t server_instance;
        memset(&server_instance, 0, sizeof(server_instance));
        server_instance.ssid = 1;
        server_instance.lifetime = 50;
        server_instance.default_min_period = -1;
        server_instance.default_max_period = -1;
        server_instance.disable_timeout = -1;
        server_instance.binding = "U";
        server_instance.notification_storing = false;
        if ((result = anjay_server_object_add_instance(anjay, &server_instance,
                                                       &server_instance_iid))) {
            avs_log(lwm2m, ERROR, "could not install server object");
            return result;
        }
    }

    return 0;
}
#endif // MBED_CONF_APP_WITH_EST

void log_handler(avs_log_level_t level,
                 const char *module,
                 const char *message) {
    (void) level;
    (void) module;
    printf("%s\n", message);
}

#if !MBED_CONF_APP_WITH_EST
const char *try_get_modem_imei() {
    static char imei_buf[32] = "";

    auto *device = CellularDevice::get_default_instance();
    if (!device) {
        return nullptr;
    }
    auto *at = device->get_at_handler();
    if (!at) {
        return nullptr;
    }

    at->lock();
    at->cmd_start("AT+CGSN");
    at->cmd_stop();
    at->resp_start();
    int result = at->read_string(imei_buf, sizeof(imei_buf));
    at->resp_stop();
    at->unlock();

    if (result < 0) {
        avs_log(modem_bg96, WARNING, "unable to get IMEI: %d",
                (int) at->get_last_error());
        return nullptr;
    }

    char *end_of_imei = strpbrk(imei_buf, "\r\n");
    if (end_of_imei) {
        *end_of_imei = '\0';
    }

    return imei_buf;
}
#endif // !MBED_CONF_APP_WITH_EST

const char *get_endpoint_name() {
#if !MBED_CONF_APP_WITH_EST
    static char endpoint_name[64] = "";

    if (endpoint_name[0] != '\0') {
        return endpoint_name;
    }

    if (const char *imei = try_get_modem_imei()) {
        int result = snprintf(endpoint_name, sizeof(endpoint_name),
                              "urn:imei:%s", imei);
        assert(result > 0);
        if (result > 0) {
            return endpoint_name;
        }
    }
#endif // !MBED_CONF_APP_WITH_EST

    return DEFAULT_ENDPOINT_NAME;
}

void periodic_update(avs_sched_t *sched, const void *anjay_ptr) {
    anjay_t *anjay = *(anjay_t *const *) anjay_ptr;

    device_object_update(anjay);
    humidity_object_update(anjay);
    barometer_object_update(anjay);
    joystick_object_update(anjay);
    magnetometer_object_update(anjay);
    accelerometer_object_update(anjay);

    if (anjay_all_connections_failed(anjay)) {
        anjay_event_loop_interrupt(anjay);
    } else {
        AVS_SCHED_DELAYED(sched, nullptr,
                          avs_time_duration_from_scalar(1, AVS_TIME_S),
                          periodic_update, &anjay, sizeof(anjay));
    }

    if (SERIAL_MENU_CONFIG.persistence_enabled
        && persist_anjay_if_required(anjay)) {
        avs_log(lwm2m, ERROR, "couldn't persist Anjay's state");
    }
}

void lwm2m_serve() {
    avs_log(lwm2m, INFO, "lwm2m_task starting up");

    while (true) {
        anjay_configuration_t CONFIG;
        memset(&CONFIG, 0, sizeof(CONFIG));
        CONFIG.endpoint_name = get_endpoint_name();
        CONFIG.in_buffer_size = 1024;
        CONFIG.out_buffer_size = 1024;
        CONFIG.msg_cache_size = 2048;
        CONFIG.disable_legacy_server_initiated_bootstrap = true;
#ifdef ANJAY_WITH_LWM2M11
        anjay_lwm2m_version_config_t version_config = {
            .minimum_version = ANJAY_LWM2M_VERSION_1_0,
            .maximum_version = SERIAL_MENU_CONFIG.maximum_version
        };
        CONFIG.lwm2m_version_config = &version_config;
#endif // ANJAY_WITH_LWM2M11
#ifdef WITH_SMS
        // SMS binding
        if (auto *device = CellularDevice::get_default_instance()) {
            if (auto *sms = device->open_sms()) {
                CONFIG.sms_driver = nrf_smsdrv_create(sms);
                CONFIG.local_msisdn = SMS_BINDING_LOCAL_PHONE_NUMBER;
                CONFIG.prefer_multipart_sms = false;
            }
        }
#endif // WITH_SMS

        avs_log(lwm2m, INFO, "endpoint name: %s", CONFIG.endpoint_name);
        anjay_t *anjay = anjay_new(&CONFIG);

        if (!anjay) {
            avs_log(lwm2m, ERROR, "could not create anjay object");
            goto finish;
        }

        if (anjay_security_object_install(anjay)
            || anjay_server_object_install(anjay)
            // Access Control object is necessary if Server Object with many
            // servers is loaded
            || anjay_access_control_install(anjay)) {
            avs_log(lwm2m, ERROR, "cannot install core objects");
        }

        if ((!SERIAL_MENU_CONFIG.persistence_enabled
             || restore_anjay_from_persistence(anjay))
            &&
#if MBED_CONF_APP_WITH_EST
            configure_servers_from_est(anjay)
#else  // MBED_CONF_APP_WITH_EST
            configure_servers_from_config(anjay)
#endif // MBED_CONF_APP_WITH_EST
        ) {
            avs_log(lwm2m, ERROR, "cannot configure servers");
            goto finish;
        }

        if (device_object_install(anjay)
#ifdef MBED_CLOUD_CLIENT_FOTA_ENABLE
            || fw_update_object_install(anjay)
#endif // MBED_CLOUD_CLIENT_FOTA_ENABLE
            || joystick_object_install(anjay) || humidity_object_install(anjay)
            || barometer_object_install(anjay)
            || magnetometer_object_install(anjay)
            || accelerometer_object_install(anjay)) {
            avs_log(lwm2m, ERROR, "cannot register data model objects");
            goto finish;
        }

        if (NETWORK) {
            if (auto *ctx = CellularContext::get_default_instance()) {
                if (conn_monitoring_object_install(anjay, ctx, NETWORK)) {
                    avs_log(lwm2m, ERROR, "cannot register data model objects");
                    goto finish;
                }
            }
        }

        periodic_update(anjay_get_scheduler(anjay), &anjay);
#ifdef WITH_SMS
        ServeSmsArgs serve_sms_args;
        serve_sms_args.anjay = anjay;
        serve_sms_args.smsdrv = CONFIG.sms_driver;
        serve_sms(anjay_get_scheduler(anjay), &serve_sms_args);
#endif // WITH_SMS

        anjay_event_loop_run(anjay,
                             avs_time_duration_from_scalar(100, AVS_TIME_MS));
        avs_log(lwm2m, ERROR, "lwm2m_task finished unexpectedly");

    finish:
        if (anjay) {
            conn_monitoring_object_uninstall(anjay);
            device_object_uninstall(anjay);
            joystick_object_uninstall(anjay);
            humidity_object_uninstall(anjay);
            barometer_object_uninstall(anjay);
            magnetometer_object_uninstall(anjay);
            accelerometer_object_uninstall(anjay);
            anjay_delete(anjay);
        }

        avs_log(lwm2m, ERROR, "resetting to factory defaults after 30s");
        ThisThread::sleep_for(30s);
    }
}

const Kernel::Clock::duration_u32 STATS_SAMPLE_TIME = 15s;

float cpu_usage_percent(uint64_t idle_diff, uint64_t sample_time) {
    float usage_percent =
            100.0f
            - (static_cast<float>(idle_diff) / static_cast<float>(sample_time))
                      * 100.0f;
    return std::max(0.0f, std::min(100.0f, usage_percent));
}

void print_stats(void) {
#if !MBED_MEM_TRACING_ENABLED || !MBED_STACK_STATS_ENABLED
#warning "Thread stack statistics require MBED_STACK_STATS_ENABLED and " \
             "MBED_MEM_TRACING_ENABLED to be defined in mbed_app.json"
    avs_log(mbed_stats, INFO, "Thread stacks stats disabled");
#else
    int num_threads = osThreadGetCount();
    assert(num_threads >= 0);
    std::unique_ptr<mbed_stats_stack_t[]> stack_stats{ new (
            std::nothrow) mbed_stats_stack_t[num_threads] };
    if (!stack_stats) {
        avs_log(mbed_stats, ERROR, "out of memory");
        return;
    }
    num_threads = mbed_stats_stack_get_each(stack_stats.get(), num_threads);

    avs_log(mbed_stats, INFO, "Thread stacks:");
    for (int i = 0; i < num_threads; ++i) {
        const auto &stats = stack_stats[i];
        avs_log(mbed_stats, INFO, "- thread %#08" PRIx32 ": %5lu / %5lu B used",
                stats.thread_id, stats.max_size, stats.reserved_size);
    }
#endif

#if !MBED_MEM_TRACING_ENABLED || !MBED_HEAP_STATS_ENABLED
#warning "Thread stack statistics require MBED_HEAP_STATS_ENABLED and " \
             "MBED_MEM_TRACING_ENABLED to be defined in mbed_app.json"
    avs_log(mbed_stats, INFO, "Heap usage stats disabled");
#else
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);
    avs_log(mbed_stats, INFO, "Heap: %lu/%lu B used", heap_stats.current_size,
            heap_stats.reserved_size);
#endif

#if !MBED_CPU_STATS_ENABLED
#warning "CPU usage statistics require MBED_CPU_STATS_ENABLED to be " \
             "defined in mbed_app.json"
    avs_log(mbed_stats, INFO, "CPU usage stats disabled");
#else
    static mbed_stats_cpu_t prev_cpu_stats;
    mbed_stats_cpu_t cpu_stats;
    mbed_stats_cpu_get(&cpu_stats);

    static uint64_t samples_gathered = 0;
    ++samples_gathered;

    avs_log(mbed_stats, INFO, "CPU usage: %.4f%% current, %.4f%% average",
            cpu_usage_percent(
                    cpu_stats.idle_time - prev_cpu_stats.idle_time,
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            STATS_SAMPLE_TIME)
                            .count()),
            cpu_usage_percent(
                    cpu_stats.idle_time,
                    std::chrono::duration_cast<std::chrono::microseconds>(
                            STATS_SAMPLE_TIME)
                                    .count()
                            * samples_gathered));

    prev_cpu_stats = cpu_stats;
#endif
}

Thread thread_lwm2m(osPriorityNormal, 16384, nullptr, "lwm2m");

#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
void set_modem_configuration(CellularInterface *dest, const ModemConfig &src) {
    if (!src.apn.empty()) {
        const char *apn = src.apn.c_str();
        const char *username =
                src.username.empty() ? nullptr : src.username.c_str();
        const char *password =
                src.password.empty() ? nullptr : src.password.c_str();
        dest->set_credentials(apn, username, password);
    }
    if (!src.sim_pin_code.empty()) {
        dest->set_sim_pin(src.sim_pin_code.c_str());
    }
}
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR

class NetworkService {
    NetworkInterface *iface_;

public:
    NetworkService() : iface_() {}

    /**
     * This function is responsible for simple connection management. It's
     * supposed to:
     *      - initialize underlying network devices & connect to the network,
     *      - setup iface_ field to a valid NetworkInterface instance,
     *      - (optional) setup NETWORK global variable if the network is
     *        cellular.
     *
     * @returns 0 on success, negative value otherwise.
     */
    int init(Lwm2mConfig &config) {
        SocketAddress sa;
        nsapi_error_t err;

        NetworkInterface *netif = NetworkInterface::get_default_instance();
        if (!netif) {
            printf("ERROR - can't get default network instance!\n");
            return -1;
        }

#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
        if (CellularInterface *cellular = netif->cellularInterface()) {
            set_modem_configuration(cellular, config.modem_config);
        }
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR

        printf("Configuring network interface\n");
        for (int retry = 0;
             netif->get_connection_status() != NSAPI_STATUS_GLOBAL_UP;
             ++retry) {
            printf("connect, retry = %d\n", retry);
            nsapi_error_t err = netif->connect();
            printf("connect result = %d\n", err);
        }

#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
        if (CellularDevice *device = CellularDevice::get_default_instance()) {
            NETWORK = device->open_network();
            NETWORK->set_access_technology(config.modem_config.rat);
        }
#endif // MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR

        // Print IP address and MAC address, quite useful in troubleshooting
        err = netif->get_ip_address(&sa);
        if (err != NSAPI_ERROR_OK) {
            printf("get_ip_address() - failed, status %d\n", err);
        } else {
            printf("IP: %s\n",
                   (sa.get_ip_address() ? sa.get_ip_address() : "None"));
            printf("MAC address: %s\n",
                   (netif->get_mac_address() ? netif->get_mac_address()
                                             : "None"));
        }

        iface_ = netif;
        return 0;
    }

    NetworkInterface *get_network_interface() {
        return iface_;
    }
};

} // namespace

int main() {
    {
        Lwm2mConfigPersistence config_persistence;
        if (config_persistence.persistence(
                    Lwm2mConfigPersistence::Direction::RESTORE,
                    SERIAL_MENU_CONFIG)) {
            printf("[INFO] Error occurred during loading configuration from "
                   "flash. Possible cause: device is booted up right after "
                   "factory reset or data on flash is corrupted.\n");
        }
        printf("\nPress any key in 3 seconds to enter device configuration "
               "menu...\n");
        if (should_show_menu(avs_time_duration_from_scalar(3, AVS_TIME_S))) {
            show_menu_and_maybe_update_config(SERIAL_MENU_CONFIG);
            if (config_persistence.persistence(
                        Lwm2mConfigPersistence::Direction::STORE,
                        SERIAL_MENU_CONFIG)) {
                printf("[INFO] Error occurred during saving configuration into "
                       "flash.\n");
            }
        }
    }

    mbed_trace_init();
    avs_log_set_default_level(SERIAL_MENU_CONFIG.log_level);
    avs_log_set_handler(log_handler);

#if MBED_MEM_TRACING_ENABLED                                     \
        || (MBED_STACK_STATS_ENABLED && MBED_HEAP_STATS_ENABLED) \
        || (MBED_HEAP_STATS_ENABLED && MBED_HEAP_STATS_ENABLED)
    mbed_event_queue()->call_every(STATS_SAMPLE_TIME, print_stats);
#else
    avs_log(mbed_stats, INFO, "All stats disabled");
#endif

    // See https://github.com/ARMmbed/mbed-os/issues/7069. In general this is
    // required to initialize hardware RNG used by default.
    mbedtls_platform_setup(NULL);

    NetworkService ns{};
    if (ns.init(SERIAL_MENU_CONFIG)) {
        printf("[ERROR] The target platform you're using does not have network "
               "configuration pre-implemented. Please see the NetworkService "
               "class (defined in main.cpp) for more details.\n");

        for (;;) {
            ThisThread::sleep_for(1s);
        }
    }

    {
        AvsSocketGlobal avs(ns.get_network_interface(), 32, 1536,
                            AVS_NET_AF_INET4);

        thread_lwm2m.start(lwm2m_serve);
        DigitalOut heartbeat{ LED1 };
        int heartbeat_value = 0;
        for (;;) {
            heartbeat = (heartbeat_value ^= 1);
            ThisThread::sleep_for(1s);
        }
    }
}
