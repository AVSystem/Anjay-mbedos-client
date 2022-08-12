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

#if MBED_CONF_CELLULAR_USE_SMS

#include "sms_driver.h"
#include <array>
#include <avsystem/commons/avs_list_cxx.hpp>
#include <avsystem/commons/avs_log.h>

#include <anjay/anjay_config.h>
#include <anjay/sms.h>

#ifdef ANJAY_WITH_SMS_MULTIPART
#error SMS multipart is already handled by mbedOS
#endif // ANJAY_WITH_SMS_MULTIPART

using namespace mbed;
using namespace std;

/**
 * TODO This forward declaration is needed to use private function from
 * anjay-mbedos/src/avs_net_impl/avs_socket_impl.h. Consider moving the
 * declaration to public API.
 */
namespace avs_mbed_impl {
avs_errno_t nsapi_error_to_errno(nsapi_size_or_error_t error);
} // namespace avs_mbed_impl

namespace {

struct sms_message_t {
    std::array<char, mbed::SMS_MAX_PHONE_NUMBER_SIZE> phone_number;
    std::array<uint8_t, mbed::SMS_MAX_SIZE_WITH_CONCATENATION> content;
    size_t content_size;
};

struct nrf_smsdrv_t {
    anjay_smsdrv_t driver;

    CellularSMS *sms;
    avs::List<sms_message_t> sms_inbox;
    avs_errno_t error;

    nrf_smsdrv_t(CellularSMS *sms);

    void set_errno(avs_errno_t error) {
        this->error = error;
    }
};

nrf_smsdrv_t *get_smsdrv(anjay_smsdrv_t *smsdrv_) {
    nrf_smsdrv_t *smsdrv = reinterpret_cast<nrf_smsdrv_t *>(smsdrv_);
    assert(&smsdrv->driver == smsdrv_);
    return smsdrv;
}

int sms_send(anjay_smsdrv_t *smsdrv_,
             const char *destination,
             const void *data,
             size_t data_size,
             const anjay_smsdrv_multipart_info_t *multipart_info,
             avs_time_duration_t timeout) noexcept {
    (void) timeout;
    if (multipart_info) {
        avs_log(sms_driver, ERROR,
                "Anjay WITH_SMS_MULTIPART compile option should be set to OFF "
                "because SMS multiparts are handled internally in MbedOS");
        return -1;
    }

    nrf_smsdrv_t *smsdrv = get_smsdrv(smsdrv_);

    nsapi_size_or_error_t result = smsdrv->sms->send_sms(
            destination, reinterpret_cast<const char *>(data), data_size);
    nsapi_error_t error = result >= 0 ? NSAPI_ERROR_OK : result;

    smsdrv->set_errno(avs_mbed_impl::nsapi_error_to_errno(error));
    return smsdrv->error ? -1 : 0;
};

enum {
    SMS_SHOULD_TRY_RECV_YES = 1,
    SMS_SHOULD_TRY_RECV_NO = 0,
    SMS_SHOULD_TRY_RECV_ERROR = -1
};

constexpr nsapi_error_t SMS_ERROR_NO_SMS_WAS_FOUND = -1;

int sms_should_try_recv(anjay_smsdrv_t *smsdrv_,
                        avs_time_duration_t timeout) noexcept {
    nrf_smsdrv_t *smsdrv = get_smsdrv(smsdrv_);

    if (!smsdrv->sms_inbox.empty()) {
        return SMS_SHOULD_TRY_RECV_YES;
    }

    const avs_time_monotonic_t deadline =
            avs_time_monotonic_add(avs_time_monotonic_now(), timeout);
    do {
        auto new_sms = smsdrv->sms_inbox.allocate(smsdrv->sms_inbox.begin());
        if (new_sms == smsdrv->sms_inbox.end()) {
            smsdrv->set_errno(AVS_ENOMEM);
            return SMS_SHOULD_TRY_RECV_ERROR;
        }

        nsapi_size_or_error_t result = smsdrv->sms->get_sms(
                reinterpret_cast<char *>(new_sms->content.data()),
                new_sms->content.max_size(), new_sms->phone_number.data(),
                new_sms->phone_number.max_size(), nullptr, 0, nullptr);
        if (result >= 0) {
            new_sms->content_size = result;
        }
        nsapi_error_t error = result >= 0 ? NSAPI_ERROR_OK : result;

        if (error == NSAPI_ERROR_OK) {
            return SMS_SHOULD_TRY_RECV_YES;
        } else {
            if (error != -1) {
                avs_log(sms_driver, ERROR,
                        "Error %d while trying to receive an sms", error);
            }

            smsdrv->sms_inbox.erase(new_sms);
        }

        if (error != SMS_ERROR_NO_SMS_WAS_FOUND
            && error != SMS_ERROR_MULTIPART_ALL_PARTS_NOT_READ) {
            smsdrv->set_errno(avs_mbed_impl::nsapi_error_to_errno(error));
            return SMS_SHOULD_TRY_RECV_ERROR;
        }
    } while (avs_time_monotonic_before(avs_time_monotonic_now(), deadline));

    return SMS_SHOULD_TRY_RECV_NO;
}

int sms_recv_all(anjay_smsdrv_t *smsdrv_,
                 anjay_smsdrv_recv_all_cb_t *callback,
                 void *callback_arg) noexcept {
    nrf_smsdrv_t *smsdrv = get_smsdrv(smsdrv_);

    int first_encountered_error = 0;
    for (auto it = smsdrv->sms_inbox.begin(); it != smsdrv->sms_inbox.end();) {
        bool ignored_should_remove_param;

        int callback_result = callback(callback_arg, it->phone_number.data(),
                                       it->content.data(), it->content_size,
                                       nullptr, &ignored_should_remove_param);
        if (callback_result) {
            if (!first_encountered_error) {
                first_encountered_error = callback_result;
                smsdrv->error = AVS_EIO;
            }
            it++;
        } else {
            it = smsdrv->sms_inbox.erase(it);
        }
    }

    return first_encountered_error;
}

int sms_system_socket(anjay_smsdrv_t *smsdrv_, const void **out) noexcept {
    (void) out;
    nrf_smsdrv_t *smsdrv = get_smsdrv(smsdrv_);

    avs_log(sms_driver, ERROR,
            "anjay_smsdrv_system_socket_t method not implemented");
    smsdrv->error = AVS_ENOTSUP;
    return -1;
}

avs_errno_t sms_error(anjay_smsdrv_t *smsdrv) noexcept {
    return get_smsdrv(smsdrv)->error;
}

void sms_free(anjay_smsdrv_t *smsdrv_) noexcept {
    nrf_smsdrv_t *smsdrv = get_smsdrv(smsdrv_);
    delete smsdrv;
}

nrf_smsdrv_t::nrf_smsdrv_t(CellularSMS *sms)
        : driver(), sms(sms), sms_inbox(), error(AVS_NO_ERROR) {
    driver.send = sms_send;
    driver.should_try_recv = sms_should_try_recv;
    driver.recv_all = sms_recv_all;
    driver.system_socket = sms_system_socket;
    driver.get_error = sms_error;
    driver.free = sms_free;
}

} // namespace

anjay_smsdrv_t *nrf_smsdrv_create(CellularSMS *sms) {
    nsapi_error_t error = sms->initialize(CellularSMS::CellularSMSMmodePDU,
                                          CellularSMS::CellularSMSEncoding8Bit);
    if (error) {
        avs_log(sms_driver, ERROR, "sms->initialize failed, error = %d", error);
        return nullptr;
    }
    nrf_smsdrv_t *smsdrv = new (std::nothrow) nrf_smsdrv_t(sms);
    return smsdrv ? &smsdrv->driver : nullptr;
}

bool nrf_smsdrv_has_unread(anjay_smsdrv_t *smsdrv) {
    return sms_should_try_recv(smsdrv,
                               avs_time_duration_from_scalar(0, AVS_TIME_S))
           == SMS_SHOULD_TRY_RECV_YES;
}

#endif // MBED_CONF_CELLULAR_USE_SMS
