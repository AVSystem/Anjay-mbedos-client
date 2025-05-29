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

#include "mbed.h"

#ifdef MBED_CLOUD_CLIENT_FOTA_ENABLE

#include "fw_update.h"

#include "mbed_cloud_fota_wrapper.h"

#include <anjay/fw_update.h>

#include <avsystem/commons/avs_log.h>

#include <cstdlib>

#define LOG(...) avs_log(fw_update, __VA_ARGS__)

using namespace std;

class FirmwareUpdateContext {
    unique_ptr<MbedCloudFotaFlasher> flasher_;

public:
    FirmwareUpdateContext() : flasher_() {}

    void reset_firmware() {
        flasher_.reset();
    }

    void perform_upgrade() {
        if (flasher_) {
            flasher_->flash();
        }
    }

    int finish_writing() {
        if (!flasher_) {
            return -1;
        }
        int result = flasher_->finish();
        if (result) {
            flasher_.reset();
        }
        return result;
    }

    int write_firmware(const void *data, size_t length) {
        if (!flasher_ && !(flasher_ = make_unique<MbedCloudFotaFlasher>())) {
            return -1;
        }
        if (!length) {
            return 0;
        }
        return flasher_->write(data, length);
    }
};

int fw_update_stream_open(void *user_ptr,
                          const char *package_uri,
                          const struct anjay_etag *package_etag) {
    FirmwareUpdateContext *ctx =
            reinterpret_cast<FirmwareUpdateContext *>(user_ptr);
    ctx->reset_firmware();
    return 0;
}

int fw_update_stream_write(void *user_ptr, const void *data, size_t length) {
    FirmwareUpdateContext *ctx =
            reinterpret_cast<FirmwareUpdateContext *>(user_ptr);
    return ctx->write_firmware(data, length);
}

int fw_update_stream_finish(void *user_ptr) {
    FirmwareUpdateContext *ctx =
            reinterpret_cast<FirmwareUpdateContext *>(user_ptr);
    return ctx->finish_writing();
}

void fw_update_reset(void *user_ptr) {
    FirmwareUpdateContext *ctx =
            reinterpret_cast<FirmwareUpdateContext *>(user_ptr);
    ctx->reset_firmware();
}

int fw_update_perform_upgrade(void *user_ptr) {
    FirmwareUpdateContext *ctx =
            reinterpret_cast<FirmwareUpdateContext *>(user_ptr);
    LOG(DEBUG, "Performing upgrade... let's hope for the best.");
    ctx->perform_upgrade();

    // Could not reset the board?!
    ctx->reset_firmware();
    return -1;
}

static const anjay_fw_update_handlers_t FW_HANDLERS = {
    /* stream_open   */ fw_update_stream_open,
    /* stream_write  */ fw_update_stream_write,
    /* stream_finish */ fw_update_stream_finish,
    /* reset         */ fw_update_reset,
    /* get_name      */ nullptr,
    /* get_version    */ nullptr,
    /* perform_upgrade */ fw_update_perform_upgrade,
    /* get_security_info */ nullptr,
    /* get_coap_tx_params */ nullptr
};

static FirmwareUpdateContext CONTEXT;

int fw_update_object_install(anjay_t *anjay) {
    anjay_fw_update_initial_state_t state{};
    state.result = MbedCloudFotaGlobal::INSTANCE.initial_result();
    return anjay_fw_update_install(anjay, &FW_HANDLERS, &CONTEXT, &state);
}

#endif // MBED_CLOUD_CLIENT_FOTA_ENABLE
