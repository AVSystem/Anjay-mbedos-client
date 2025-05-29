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

#include "persistence.h"

#include <anjay/access_control.h>
#include <anjay/anjay.h>
#include <anjay/security.h>
#include <anjay/server.h>

#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_stream_inbuf.h>
#include <avsystem/commons/avs_stream_membuf.h>

#include <array>
#include <mbed.h>
#include <memory>
#include <new>
#include <vector>

#include <kvstore_global_api/kvstore_global_api.h>

#define LOG(...) avs_log(persistence, __VA_ARGS__)

namespace {

typedef avs_error_t RestoreFn(anjay_t *anjay, avs_stream_t *stream);
typedef avs_error_t PersistFn(anjay_t *anjay, avs_stream_t *stream);
typedef bool IsModifiedFn(anjay_t *anjay);
typedef void PurgeFn(anjay_t *anjay);

struct Target {
    const char *const name;
    RestoreFn &restore;
    PersistFn &persist;
    IsModifiedFn &is_modified;
    PurgeFn &purge;

    Target(const char *name,
           RestoreFn &restore,
           PersistFn &persist,
           IsModifiedFn &is_modified,
           PurgeFn &purge)
            : name(name),
              restore(restore),
              persist(persist),
              is_modified(is_modified),
              purge(purge) {}
};

#if MBED_CONF_APP_WITH_EST
#define anjay_est_state_is_modified anjay_est_state_is_ready_for_persistence
inline void anjay_est_state_purge(anjay_t *anjay){
    // TODO: does simply purging Server Object state invalidate EST too?
};
#endif // MBED_CONF_APP_WITH_EST

#define DECL_TARGET(Name)                                                   \
    Target(AVS_QUOTE(Name), anjay_##Name##_restore, anjay_##Name##_persist, \
           anjay_##Name##_is_modified, anjay_##Name##_purge)

const Target targets[] = {
    DECL_TARGET(security_object),
    DECL_TARGET(server_object),
    DECL_TARGET(access_control),
#if MBED_CONF_APP_WITH_EST
    DECL_TARGET(est_state),
#endif // MBED_CONF_APP_WITH_EST
};
bool previous_attempt_failed;

#undef DECL_TARGET

std::string kv_key_wrap(const char *key) {
    static const std::string prefix =
            "/" AVS_QUOTE_MACRO(MBED_CONF_STORAGE_DEFAULT_KV) "/persistence_";
    return prefix + key;
}

} // namespace

int persistence_purge() {
    for (auto const &target : targets) {
        auto res = kv_remove(kv_key_wrap(target.name).c_str());
        if (res && res != MBED_ERROR_ITEM_NOT_FOUND) {
            LOG(ERROR, "Couldn't delete persisted %s from storage",
                target.name);
            return -1;
        }
    }

    return 0;
}

namespace {
int restore_target(anjay_t *anjay, Target const &target) {
    auto key = kv_key_wrap(target.name);

    kv_info_t info;
    if (kv_get_info(key.c_str(), &info)) {
        LOG(ERROR, "Couldn't find size of persisted %s", target.name);
        return -1;
    }
    const size_t &size = info.size;

    std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[size]);
    if (!buf) {
        LOG(ERROR, "Out of memory");
        return -1;
    }
    avs_stream_inbuf_t stream = AVS_STREAM_INBUF_STATIC_INITIALIZER;
    avs_stream_inbuf_set_buffer(&stream, buf.get(), size);

    size_t actual_size;
    if (kv_get(key.c_str(), buf.get(), size, &actual_size)) {
        LOG(ERROR, "Couldn't load %s from persistence", target.name);
        return -1;
    }
    assert(actual_size == size);
    if (avs_is_err(target.restore(anjay,
                                  reinterpret_cast<avs_stream_t *>(&stream)))) {
        LOG(ERROR, "Couldn't restore %s from persistence", target.name);
        return -1;
    }

    return 0;
}
} // namespace

int restore_anjay_from_persistence(anjay_t *anjay) {
    assert(anjay);

    for (auto const &target : targets) {
        int result = restore_target(anjay, target);

        if (result) {
            LOG(ERROR, "Couldn't restore Anjay from persistence");

            for (auto const &target : targets) {
                target.purge(anjay);
            }
            persistence_purge();
            return result;
        }
    }

    return 0;
}

namespace {
int persist_target(anjay_t *anjay, Target const &target) {
    std::unique_ptr<avs_stream_t, void (*)(avs_stream_t *)> stream(
            avs_stream_membuf_create(),
            [](avs_stream_t *stream) { avs_stream_cleanup(&stream); });

    if (!stream) {
        LOG(ERROR, "Out of memory");
        return -1;
    }

    void *collected_stream_ptr;
    size_t collected_stream_len;
    if (avs_is_err(target.persist(anjay, stream.get()))
        || avs_is_err(avs_stream_membuf_take_ownership(
                   stream.get(), &collected_stream_ptr,
                   &collected_stream_len))) {
        LOG(ERROR, "Couldn't persist %s", target.name);
        return -1;
    }
    std::unique_ptr<void, void (*)(void *)> collected_stream(
            collected_stream_ptr, avs_free);

    auto key = kv_key_wrap(target.name);
    if (kv_set(key.c_str(), collected_stream.get(), collected_stream_len, 0)) {
        LOG(ERROR, "Couldn't save persisted %s to storage", target.name);
        previous_attempt_failed = true;
        return -1;
    }

    LOG(INFO, "%s persisted, len: %zu", target.name, collected_stream_len);
    return 0;
}
} // namespace

int persist_anjay_if_required(anjay_t *anjay) {
    assert(anjay);

    bool anything_persisted = false;

    for (auto const &target : targets) {
        if (!previous_attempt_failed && !target.is_modified(anjay)) {
            continue;
        }

        int result = persist_target(anjay, target);
        if (result) {
            return result;
        }

        anything_persisted = true;
    }

    if (anything_persisted) {
        previous_attempt_failed = false;
        LOG(INFO, "All targets successfully persisted");
    }

    return 0;
}
