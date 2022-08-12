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

#ifdef MBED_CLOUD_CLIENT_FOTA_ENABLE

#include <utility>

#include <avsystem/commons/avs_utils.h>

#include <anjay/fw_update.h>

#include <mbedtls/asn1.h>

#include "anjay_mbed_fota_conversions.h"
#include "mbed_cloud_fota_wrapper.h"

#include "fota.h"
#include "fota_app_ifs.h"
#include "fota_block_device.h"
#include "fota_curr_fw.h"
#include "fota_event_handler.h"
#include "fota_ext_downloader.h"
#include "fota_internal.h"
#include "fota_manifest.h"
#include "fota_nvm.h"
#include "fota_source.h"

namespace {

constexpr char COMPONENT_NAME[FOTA_COMPONENT_MAX_NAME_SIZE] = "MAIN";

fota_header_info_t CURR_FW_HEADER;

int LAST_RESULT;

class NextFotaAction {
public:
    enum class Type { NONE, AUTHORIZE, CALLBACK };

private:
    Type type_;
    fota_deferred_data_callabck_t cb_;

public:
    NextFotaAction() : type_(Type::NONE), cb_(nullptr) {}

    explicit NextFotaAction(Type type) : type_(type), cb_(nullptr) {
        assert(type_ != Type::CALLBACK);
    }

    explicit NextFotaAction(fota_deferred_data_callabck_t cb)
            : type_(Type::CALLBACK), cb_(cb) {
        assert(cb_);
    }

    Type type() const {
        return type_;
    }

    explicit operator bool() const {
        return type_ != Type::NONE;
    }

    void perform() {
        NextFotaAction next{};
        std::swap(*this, next);
        switch (next.type_) {
        case Type::NONE:
            return;
        case Type::AUTHORIZE:
            fota_on_authorize(FOTA_INSTALL_STATE_AUTHORIZE);
            return;
        case Type::CALLBACK:
            next.cb_(nullptr, 0);
            return;
        }
        assert(0);
    }
};

NextFotaAction NEXT_ACTION;

#if defined(MBED_CONF_ANJAY_MBED_FOTA_UPDATE_CERT)
constexpr const char UPDATE_CERT_PEM[] =
        AVS_QUOTE_MACRO((MBED_CONF_ANJAY_MBED_FOTA_UPDATE_CERT));
constexpr auto UPDATE_CERT_DER =
        avs::constexpr_conversions::pem2der<sizeof(UPDATE_CERT_PEM),
                                            UPDATE_CERT_PEM>();
#elif !defined(FOTA_TEST_MANIFEST_BYPASS_VALIDATION)
#error "anjay-mbed-fota.update-cert is required for FOTA"
#endif // defined(MBED_CONF_ANJAY_MBED_FOTA_UPDATE_CERT)

#if defined(MBED_CONF_ANJAY_MBED_FOTA_VENDOR_ID)
constexpr const char VENDOR_ID_HEXLIFIED[] =
        AVS_QUOTE_MACRO((MBED_CONF_ANJAY_MBED_FOTA_VENDOR_ID));
constexpr auto VENDOR_ID =
        avs::constexpr_conversions::unhexlify<sizeof(VENDOR_ID_HEXLIFIED),
                                              VENDOR_ID_HEXLIFIED>();
#elif !defined(FOTA_TEST_MANIFEST_BYPASS_VALIDATION)
#error "anjay-mbed-fota.vendor-id is required for FOTA"
#endif // defined(MBED_CONF_ANJAY_MBED_FOTA_VENDOR_ID)

#if defined(MBED_CONF_ANJAY_MBED_FOTA_CLASS_ID)
constexpr const char CLASS_ID_HEXLIFIED[] =
        AVS_QUOTE_MACRO((MBED_CONF_ANJAY_MBED_FOTA_CLASS_ID));
constexpr auto CLASS_ID =
        avs::constexpr_conversions::unhexlify<sizeof(CLASS_ID_HEXLIFIED),
                                              CLASS_ID_HEXLIFIED>();
#elif !defined(FOTA_TEST_MANIFEST_BYPASS_VALIDATION)
#error "anjay-mbed-fota.class-id is required for FOTA"
#endif // defined(MBED_CONF_ANJAY_MBED_FOTA_CLASS_ID)

anjay_fw_update_initial_result_t initial_result_impl() {
    const fota_candidate_config_t *candidate_config;
    size_t bd_read_size;
    size_t bd_prog_size;
    if (!(candidate_config = fota_candidate_get_config()) || fota_bd_init()
        || fota_bd_get_read_size(&bd_read_size)
        || fota_bd_get_program_size(&bd_prog_size)) {
        // Cannot access FOTA block device, assume we're idle
        return ANJAY_FW_UPDATE_INITIAL_NEUTRAL;
    }

    size_t addr = candidate_config->storage_start_addr;
#if FOTA_HEADER_HAS_CANDIDATE_READY
    fota_candidate_ready_header_t ready_header{};
    if (fota_candidate_read_candidate_ready_header(&addr, bd_read_size,
                                                   bd_prog_size, &ready_header)
        || ready_header.magic != FOTA_CANDIDATE_READY_MAGIC
        || ready_header.footer != FOTA_CANDIDATE_READY_MAGIC
        || memcmp(ready_header.comp_name, COMPONENT_NAME,
                  FOTA_COMPONENT_MAX_NAME_SIZE)
                   != 0) {
        // Ready header invalid or not present - installing not attempted
        return ANJAY_FW_UPDATE_INITIAL_NEUTRAL;
    }
#endif // FOTA_HEADER_HAS_CANDIDATE_READY

    fota_header_info_t candidate_header{};
    if (fota_candidate_read_header(&addr, bd_read_size, bd_prog_size,
                                   &candidate_header)
        || candidate_header.magic != FOTA_FW_HEADER_MAGIC
        || candidate_header.footer != FOTA_FW_HEADER_MAGIC) {
        // Candidate header invalid or not present - installing not attempted
        return ANJAY_FW_UPDATE_INITIAL_NEUTRAL;
    }

    if (candidate_header.fw_size == CURR_FW_HEADER.fw_size
        && candidate_header.version == CURR_FW_HEADER.version
        && memcmp(candidate_header.digest, CURR_FW_HEADER.digest,
                  FOTA_CRYPTO_HASH_SIZE)
                   == 0) {
        // Candidate is equal to the current version - upgrade successful
        return ANJAY_FW_UPDATE_INITIAL_SUCCESS;
    }

    // We don't support download resumption for now, so if the candidate is
    // different than the currently running firmware, it means failure.
    return ANJAY_FW_UPDATE_INITIAL_FAILED;
}

} // namespace

void fota_app_on_download_progress(size_t downloaded_size,
                                   size_t current_chunk_size,
                                   size_t total_size) {}

int fota_app_on_complete(int32_t status) {
    return FOTA_STATUS_SUCCESS;
}

int fota_app_on_install_authorization(void) {
    if (NEXT_ACTION) {
        return FOTA_STATUS_RESOURCE_BUSY;
    }
    NEXT_ACTION = NextFotaAction(NextFotaAction::Type::AUTHORIZE);
    return FOTA_STATUS_SUCCESS;
}

int fota_app_on_download_authorization(
        const manifest_firmware_info_t *candidate_info,
        fota_component_version_t curr_fw_version) {
    return fota_app_on_install_authorization();
}

int fota_event_handler_defer_with_data(fota_deferred_data_callabck_t cb,
                                       void *data,
                                       size_t size) {
    assert(!size);
    if (NEXT_ACTION) {
        return FOTA_STATUS_RESOURCE_BUSY;
    }
    NEXT_ACTION = NextFotaAction(cb);
    return FOTA_STATUS_SUCCESS;
}

int fota_event_cancel(uint8_t event_id) {
    if (event_id == 0) {
        NEXT_ACTION = NextFotaAction();
    }
    return FOTA_STATUS_SUCCESS;
}

void fota_source_enable_auto_observable_resources_reporting(bool enable) {}

void report_state_random_delay(bool enable) {}

int fota_source_report_update_result(int result) {
    if (LAST_RESULT == FOTA_STATUS_SUCCESS
        || LAST_RESULT == FOTA_STATUS_FW_UPDATE_OK) {
        LAST_RESULT = result;
    }
    return FOTA_STATUS_SUCCESS;
}

int fota_source_report_state(fota_source_state_e state,
                             report_sent_callback_t on_sent,
                             report_sent_callback_t on_failure) {
    if (on_sent) {
        on_sent();
    }
    return fota_is_active_update() ? 0 : -1;
}

int fota_source_report_state_in_ms(fota_source_state_e state,
                                   report_sent_callback_t on_sent,
                                   report_sent_callback_t on_failure,
                                   size_t in_ms) {
    return FOTA_STATUS_SUCCESS;
}

int fota_source_report_update_customer_result(int result) {
    return FOTA_STATUS_SUCCESS;
}

int fota_source_firmware_request_fragment(const char *uri, size_t offset) {
    return FOTA_STATUS_INVALID_ARGUMENT;
}

int fota_nvm_fw_encryption_key_set(
        const uint8_t buffer[FOTA_ENCRYPT_KEY_SIZE]) {
    return FOTA_STATUS_SUCCESS;
}

int fota_nvm_fw_encryption_key_delete(void) {
    return FOTA_STATUS_SUCCESS;
}

#ifndef FOTA_TEST_MANIFEST_BYPASS_VALIDATION
int fota_nvm_get_update_certificate(uint8_t *buffer,
                                    size_t size,
                                    size_t *bytes_read) {
    *bytes_read = AVS_MIN(UPDATE_CERT_DER.size(), size);
    memcpy(buffer, UPDATE_CERT_DER.data(), *bytes_read);
    return FOTA_STATUS_SUCCESS;
}

int fota_nvm_get_vendor_id(uint8_t buffer[FOTA_GUID_SIZE]) {
    static_assert(VENDOR_ID.size() == FOTA_GUID_SIZE,
                  "anjay-mbed-fota.vendor-id has a wrong length");
    memcpy(buffer, VENDOR_ID.data(), FOTA_GUID_SIZE);
    return FOTA_STATUS_SUCCESS;
}

int fota_nvm_get_class_id(uint8_t buffer[FOTA_GUID_SIZE]) {
    static_assert(CLASS_ID.size() == FOTA_GUID_SIZE,
                  "anjay-mbed-fota.class-id has a wrong length");
    memcpy(buffer, CLASS_ID.data(), FOTA_GUID_SIZE);
    return FOTA_STATUS_SUCCESS;
}
#endif // FOTA_TEST_MANIFEST_BYPASS_VALIDATION

MbedCloudFotaGlobal MbedCloudFotaGlobal::INSTANCE;

MbedCloudFotaGlobal::MbedCloudFotaGlobal() {
    int result = fota_curr_fw_read_header(&CURR_FW_HEADER);
    assert(!result);

    char semver[FOTA_COMPONENT_MAX_SEMVER_STR_SIZE];
    fota_component_version_int_to_semver(CURR_FW_HEADER.version, semver);

    fota_component_desc_info_t desc = { 0 };
    desc.need_reboot = true;
    desc.curr_fw_get_digest = fota_curr_fw_get_digest;
    result = fota_component_add(&desc, COMPONENT_NAME, semver);
    assert(!result);

    (void) result;
}

anjay_fw_update_initial_result_t MbedCloudFotaGlobal::initial_result() {
    anjay_fw_update_initial_result_t result = initial_result_impl();
    fota_candidate_erase();
    return result;
}

MbedCloudFotaFlasher::MbedCloudFotaFlasher()
        : input_offset_(0),
          manifest_size_(0),
          header_buf_{ 0 },
          manifest_buf_() {
    assert(!fota_is_active_update());
    LAST_RESULT = FOTA_STATUS_SUCCESS;
    NEXT_ACTION = NextFotaAction();
}

MbedCloudFotaFlasher::~MbedCloudFotaFlasher() {
    abort();
}

void MbedCloudFotaFlasher::abort() {
    if (fota_is_active_update()) {
        fota_multicast_node_on_abort();
        fota_candidate_erase();
    }
}

int MbedCloudFotaFlasher::failure() {
    // Note: The numerical values come from
    // https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/10252.xml
    switch (LAST_RESULT) {
    case 9: // Not enough storage for the new asset
    case -FOTA_STATUS_INSUFFICIENT_STORAGE:
        return ANJAY_FW_UPDATE_ERR_NOT_ENOUGH_SPACE;
    case 10: // Out of memory during download process
    case -FOTA_STATUS_OUT_OF_MEMORY:
        return ANJAY_FW_UPDATE_ERR_OUT_OF_MEMORY;
    case 4:  // Manifest failed integrity check
    case 6:  // Manifest certificate not found
    case 7:  // Manifest signature failed
    case 8:  // Dependent manifest not found
    case 12: // Asset failed integrity check
    case -FOTA_STATUS_MANIFEST_SIGNATURE_INVALID:
    case -FOTA_STATUS_MANIFEST_PAYLOAD_CORRUPTED:
    case -FOTA_STATUS_MANIFEST_VERSION_REJECTED:
    case -FOTA_STATUS_MANIFEST_WRONG_VENDOR_ID:
    case -FOTA_STATUS_MANIFEST_WRONG_CLASS_ID:
    case -FOTA_STATUS_MANIFEST_PRECURSOR_MISMATCH:
        return ANJAY_FW_UPDATE_ERR_INTEGRITY_FAILURE;
    case 5:  // Manifest rejected
    case 13: // Unsupported asset type
    case 16: // Unsupported delta format
    case 17: // Unsupported encryption format
    case -FOTA_STATUS_MANIFEST_MALFORMED:
    case -FOTA_STATUS_MANIFEST_PAYLOAD_UNSUPPORTED:
    case -FOTA_STATUS_MANIFEST_SCHEMA_UNSUPPORTED:
    case -FOTA_STATUS_MANIFEST_CUSTOM_DATA_TOO_BIG:
    case -FOTA_STATUS_UNEXPECTED_COMPONENT:
    case -FOTA_STATUS_COMB_PACKAGE_MALFORMED:
    case -FOTA_STATUS_COMB_PACKAGE_WRONG_IMAGE_NUM:
    case -FOTA_STATUS_COMB_PACKAGE_IMAGE_ID_NAME_TOO_LONG:
    case -FOTA_STATUS_COMB_PACKAGE_VENDOR_DATA_TOO_LONG:
        return ANJAY_FW_UPDATE_ERR_UNSUPPORTED_PACKAGE_TYPE;
    case 0:  // Uninitialised
    case 1:  // Success
    case 2:  // Manifest timeout
    case 3:  // Manifest not found
    case 11: // Connection lost during download process
    case 14: // Invalid asset URI
    case 15: // Timed out downloading asset
    case -FOTA_STATUS_FW_UPDATE_OK:
    case 19: // Asset updated successfully after recovery
    case -FOTA_STATUS_MANIFEST_INVALID_URI:
    case -FOTA_STATUS_DOWNLOAD_FRAGMENT_FAILED:
    case -FOTA_STATUS_STORAGE_WRITE_FAILED:
    case -FOTA_STATUS_STORAGE_READ_FAILED:
    case -FOTA_STATUS_INSTALL_AUTH_NOT_GRANTED:
    case -FOTA_STATUS_DOWNLOAD_AUTH_NOT_GRANTED:
    case -FOTA_STATUS_FW_INSTALLATION_FAILED:
    case -FOTA_STATUS_INTERNAL_ERROR:
    case -FOTA_STATUS_INTERNAL_DELTA_ERROR:
    case -FOTA_STATUS_INTERNAL_CRYPTO_ERROR:
    case -FOTA_STATUS_NOT_FOUND:
    case -FOTA_STATUS_MULTICAST_UPDATE_ABORTED:
    default:
        // NOTE: success codes also map to -1, because
        // this function is only called in an error condition.
        return -1;
    }
}

size_t MbedCloudFotaFlasher::read_manifest_length() {
    unsigned char *p = header_buf_;
    const unsigned char *const start = p;
    const unsigned char *const end = p + sizeof(header_buf_);
    // Pelion FOTA manifests are ASN.1 DER-encoded.
    // Let's read the outermost length.
    size_t length = 0;
    int result = mbedtls_asn1_get_tag(
            &p, end, &length, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (result && result != MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
        return 0;
    }
    // Note: we don't treat MBEDTLS_ERR_ASN1_OUT_OF_DATA as an error condition,
    // because mbedtls_asn1_get_tag() expects the the end pointer to be beyond
    // all the data whose length has just been read. The length pointer shall
    // be now set anyway.
    if (p == start || !length) {
        return 0;
    }
    return (p - start) + length;
}

int MbedCloudFotaFlasher::write_header(const void *data, size_t data_size) {
    assert(input_offset_ <= sizeof(header_buf_));
    size_t to_write = AVS_MIN(data_size, sizeof(header_buf_) - input_offset_);
    memcpy(header_buf_ + input_offset_, data, to_write);
    input_offset_ += to_write;
    if (input_offset_ >= sizeof(header_buf_)) {
        if ((manifest_size_ = read_manifest_length()) < sizeof(header_buf_)
            || !(manifest_buf_ =
                         std::make_unique<unsigned char[]>(manifest_size_))) {
            fota_source_report_update_result(-FOTA_STATUS_OUT_OF_MEMORY);
            return failure();
        }
        memcpy(manifest_buf_.get(), header_buf_, sizeof(header_buf_));
    }
    return 0;
}

int MbedCloudFotaFlasher::write_manifest(const void *data, size_t data_size) {
    assert(input_offset_ <= manifest_size_);
    size_t to_write = AVS_MIN(data_size, manifest_size_ - input_offset_);
    assert(!to_write || manifest_buf_);
    memcpy(manifest_buf_.get() + input_offset_, data, to_write);
    input_offset_ += to_write;
    if (input_offset_ >= manifest_size_) {
        fota_on_manifest(manifest_buf_.get(), manifest_size_);
        manifest_buf_.reset();
        if (!fota_is_active_update()) {
            fota_source_report_update_result(-FOTA_STATUS_MANIFEST_MALFORMED);
            return failure();
        }
        if (!NEXT_ACTION) {
            fota_source_report_update_result(-FOTA_STATUS_INTERNAL_ERROR);
            abort();
            return failure();
        }
        assert(NEXT_ACTION.type() == NextFotaAction::Type::AUTHORIZE);
        NEXT_ACTION.perform();
        assert(!NEXT_ACTION);
        if (!fota_is_active_update()) {
            fota_source_report_update_result(-FOTA_STATUS_INTERNAL_ERROR);
            return failure();
        }
    }
    return 0;
}

int MbedCloudFotaFlasher::write_image(const void *data, size_t data_size) {
    int result = fota_ext_downloader_write_image_fragment(
            data, input_offset_ - manifest_size_, data_size);
    if (!result) {
        input_offset_ += data_size;
    }
    if (result) {
        fota_source_report_update_result(-result);
    }
    return result;
}

int MbedCloudFotaFlasher::write(const void *data, size_t data_size) {
    int result = 0;
    while (!result && data_size) {
        size_t original_input_offset = input_offset_;
        if (!manifest_size_) {
            result = write_header(data, data_size);
        } else if (input_offset_ < manifest_size_) {
            result = write_manifest(data, data_size);
        } else {
            result = write_image(data, data_size);
        }
        if (!result) {
            size_t bytes_written = input_offset_ - original_input_offset;
            assert(bytes_written <= data_size);
            data = (const char *) data + bytes_written;
            data_size -= bytes_written;
        }
    }
    return result;
}

int MbedCloudFotaFlasher::finish() {
    int result = fota_ext_downloader_on_image_ready();
    if (result) {
        fota_source_report_update_result(-result);
        return failure();
    }
    if (!NEXT_ACTION) {
        fota_source_report_update_result(-FOTA_STATUS_INTERNAL_ERROR);
        abort();
        return failure();
    }
    assert(NEXT_ACTION.type() == NextFotaAction::Type::CALLBACK);
    // NEXT_ACTION should now refer to fota_multicast_node_on_fragment()
    while (NEXT_ACTION.type() == NextFotaAction::Type::CALLBACK) {
        NEXT_ACTION.perform();
    }
    if (!NEXT_ACTION) {
        fota_source_report_update_result(-FOTA_STATUS_INTERNAL_ERROR);
        abort();
        return failure();
    }
    // If everything went well,
    // fota_app_on_install_authorization() has just been called
    assert(NEXT_ACTION.type() == NextFotaAction::Type::AUTHORIZE);
    return 0;
}

void MbedCloudFotaFlasher::flash() {
    if (NEXT_ACTION.type() == NextFotaAction::Type::AUTHORIZE) {
        NEXT_ACTION.perform();
    }
    // If we're still here, then something went wrong
    fota_source_report_update_result(-FOTA_STATUS_FW_INSTALLATION_FAILED);
}

#endif // MBED_CLOUD_CLIENT_FOTA_ENABLE
