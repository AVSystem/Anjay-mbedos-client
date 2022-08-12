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

#ifndef MBED_CLOUD_FOTA_WRAPPER_H
#define MBED_CLOUD_FOTA_WRAPPER_H

#include <cstddef>
#include <memory>

#include <anjay/fw_update.h>

class MbedCloudFotaGlobal {
    MbedCloudFotaGlobal();

public:
    static MbedCloudFotaGlobal INSTANCE;

    anjay_fw_update_initial_result_t initial_result();
};

class MbedCloudFotaFlasher {
    size_t input_offset_;
    size_t manifest_size_;
    unsigned char header_buf_[8];
    std::unique_ptr<unsigned char[]> manifest_buf_;

    MbedCloudFotaFlasher(const MbedCloudFotaFlasher &) = delete;
    MbedCloudFotaFlasher &operator=(const MbedCloudFotaFlasher &) = delete;

    void abort();
    int failure();
    size_t read_manifest_length();
    int write_header(const void *data, size_t data_size);
    int write_manifest(const void *data, size_t data_size);
    int write_image(const void *data, size_t data_size);

public:
    MbedCloudFotaFlasher();
    ~MbedCloudFotaFlasher();

    int write(const void *data, size_t data_size);
    int finish();
    void flash();
};

#endif /* MBED_CLOUD_FOTA_WRAPPER_H */
