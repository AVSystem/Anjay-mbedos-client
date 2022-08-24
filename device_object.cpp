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

/**
 * Generated by anjay_codegen.py on 2019-07-16 15:56:25
 *
 * LwM2M Object: Device
 * ID: 3, URN: urn:oma:lwm2m:oma:3:1.1, Mandatory, Single
 *
 * This LwM2M Object provides a range of device related information which
 * can be queried by the LwM2M Server, and a device reboot and factory
 * reset function.
 */
#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_memory.h>

#include "mbed_power_mgmt.h"

#include "device_object.h"

#define DEVICE_OBJ_LOG(...) avs_log(device_obj, __VA_ARGS__)

/**
 * Manufacturer: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Human readable manufacturer name
 */
#define RID_MANUFACTURER 0

/**
 * Model Number: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * A model identifier (manufacturer specified string)
 */
#define RID_MODEL_NUMBER 1

/**
 * Firmware Version: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Current firmware version of the Device.The Firmware Management
 * function could rely on this resource.
 */
#define RID_FIRMWARE_VERSION 3

/**
 * Reboot: E, Single, Mandatory
 * type: N/A, range: N/A, unit: N/A
 * Reboot the LwM2M Device to restore the Device from unexpected firmware
 * failure.
 */
#define RID_REBOOT 4

/**
 * Error Code: R, Multiple, Mandatory
 * type: integer, range: 0..8, unit: N/A
 * 0=No error 1=Low battery power 2=External power supply off 3=GPS
 * module failure 4=Low received signal strength 5=Out of memory 6=SMS
 * failure 7=IP connectivity failure 8=Peripheral malfunction  When the
 * single Device Object Instance is initiated, there is only one error
 * code Resource Instance whose value is equal to 0 that means no error.
 * When the first error happens, the LwM2M Client changes error code
 * Resource Instance to any non-zero value to indicate the error type.
 * When any other error happens, a new error code Resource Instance is
 * created. When an error associated with a Resource Instance is no
 * longer present, that Resource Instance is deleted. When the single
 * existing error is no longer present, the LwM2M Client returns to the
 * original no error state where Instance 0 has value 0. This error code
 * Resource MAY be observed by the LwM2M Server. How to deal with LwM2M
 * Client’s error report depends on the policy of the LwM2M Server.
 */
#define RID_ERROR_CODE 11

/**
 * Current Time: RW, Single, Optional
 * type: time, range: N/A, unit: N/A
 * Current UNIX time of the LwM2M Client. The LwM2M Client should be
 * responsible to increase this time value as every second elapses. The
 * LwM2M Server is able to write this Resource to make the LwM2M Client
 * synchronized with the LwM2M Server.
 */
#define RID_CURRENT_TIME 13

/**
 * Supported Binding and Modes: R, Single, Mandatory
 * type: string, range: N/A, unit: N/A
 * Indicates which bindings and modes are supported in the LwM2M Client.
 * The possible values are those listed in the LwM2M Core Specification.
 */
#define RID_SUPPORTED_BINDING_AND_MODES 16

/**
 * Software Version: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Current software version of the device (manufacturer specified
 * string). On elaborated LwM2M device, SW could be split in 2 parts: a
 * firmware one and a higher level software on top. Both pieces of
 * Software are together managed by LwM2M Firmware Update Object (Object
 * ID 5)
 */
#define RID_SOFTWARE_VERSION 19

typedef struct device_struct {
    const anjay_dm_object_def_t *def;

    avs_time_duration_t current_time_offset;
    bool reboot;
} device_t;

static inline device_t *get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, device_t, def);
}

static int instance_reset(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid) {
    (void) anjay;

    device_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    obj->current_time_offset = AVS_TIME_DURATION_ZERO;

    return 0;
}

static int list_resources(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_MANUFACTURER, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_MODEL_NUMBER, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_FIRMWARE_VERSION, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_REBOOT, ANJAY_DM_RES_E, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_ERROR_CODE, ANJAY_DM_RES_RM,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_CURRENT_TIME, ANJAY_DM_RES_RW,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_SUPPORTED_BINDING_AND_MODES, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_SOFTWARE_VERSION, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    return 0;
}

static const char *device_manufacturer(void) {
#ifdef TARGET_STM
    return "STMicroelectronics";
#elif TARGET_NORDIC
    return "Nordic Semiconductor";
#else
#warning "Add device manufacturer"
    return "Unknown";
#endif
}

static const char *model_number(void) {
#ifdef TARGET_DISCO_L496AG
    return "STM32L496G-DISCO";
#elif TARGET_NUCLEO_F429ZI
    return "NUCLEO-F429ZI";
#elif TARGET_NRF52840_DK
    return "nRF52840 DK";
#else
#warning "Unsupported board, using TARGET_NAME as model number"
    return MBED_STRINGIFY(TARGET_NAME);
#endif
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;

    device_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_MANUFACTURER:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, device_manufacturer());

    case RID_MODEL_NUMBER:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, model_number());

    case RID_FIRMWARE_VERSION:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, "22.08");

    case RID_ERROR_CODE:
        assert(riid == 0);
        return anjay_ret_i32(ctx, 0);

    case RID_CURRENT_TIME: {
        assert(riid == ANJAY_ID_INVALID);
        int64_t seconds_since_unix_epoch;
        if (avs_time_real_to_scalar(
                    &seconds_since_unix_epoch, AVS_TIME_S,
                    avs_time_real_add(avs_time_real_now(),
                                      obj->current_time_offset))) {
            return ANJAY_ERR_INTERNAL;
        }
        return anjay_ret_i64(ctx, seconds_since_unix_epoch);
    }

    case RID_SUPPORTED_BINDING_AND_MODES:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, "UQ");

    case RID_SOFTWARE_VERSION:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, anjay_get_version());

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int resource_write(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_rid_t rid,
                          anjay_riid_t riid,
                          anjay_input_ctx_t *ctx) {
    (void) anjay;

    device_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_CURRENT_TIME: {
        assert(riid == ANJAY_ID_INVALID);
        int64_t expected_seconds_since_unix_epoch;
        int result = anjay_get_i64(ctx, &expected_seconds_since_unix_epoch);
        if (result) {
            return result;
        }
        obj->current_time_offset = avs_time_real_diff(
                avs_time_real_from_scalar(expected_seconds_since_unix_epoch,
                                          AVS_TIME_S),
                avs_time_real_now());
        return 0;
    }
    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int resource_execute(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *obj_ptr,
                            anjay_iid_t iid,
                            anjay_rid_t rid,
                            anjay_execute_ctx_t *arg_ctx) {
    (void) arg_ctx;

    device_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_REBOOT:
        obj->reboot = true;
        return 0;

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int list_resource_instances(anjay_t *anjay,
                                   const anjay_dm_object_def_t *const *obj_ptr,
                                   anjay_iid_t iid,
                                   anjay_rid_t rid,
                                   anjay_dm_list_ctx_t *ctx) {
    (void) anjay;

    device_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_ERROR_CODE:
        anjay_dm_emit(ctx, 0);
        return 0;
    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

namespace {

struct ObjDef : public anjay_dm_object_def_t {
    ObjDef() : anjay_dm_object_def_t() {
        oid = 3;

        handlers.list_instances = anjay_dm_list_instances_SINGLE;
        handlers.instance_reset = instance_reset;

        handlers.list_resources = list_resources;
        handlers.resource_read = resource_read;
        handlers.resource_write = resource_write;
        handlers.resource_execute = resource_execute;
        handlers.list_resource_instances = list_resource_instances;

        // TODO: implement these if transactional write/create is required
        handlers.transaction_begin = anjay_dm_transaction_NOOP;
        handlers.transaction_validate = anjay_dm_transaction_NOOP;
        handlers.transaction_commit = anjay_dm_transaction_NOOP;
        handlers.transaction_rollback = anjay_dm_transaction_NOOP;
    }
} const OBJ_DEF;

const anjay_dm_object_def_t **device_object_create(void) {
    device_t *obj = (device_t *) avs_calloc(1, sizeof(device_t));
    if (!obj) {
        return NULL;
    }
    obj->def = &OBJ_DEF;
    return &obj->def;
}

void device_object_release(const anjay_dm_object_def_t ***def) {
    if (*def) {
        device_t *obj = get_obj(*def);
        avs_free(obj);
        *def = NULL;
    }
}

const anjay_dm_object_def_t **OBJ_DEF_PTR;

} // namespace

int device_object_install(anjay_t *anjay) {
    if (OBJ_DEF_PTR) {
        DEVICE_OBJ_LOG(ERROR, "Device Object has been already installed");
        return -1;
    }

    OBJ_DEF_PTR = device_object_create();
    return anjay_register_object(anjay, OBJ_DEF_PTR);
}

void device_object_uninstall(anjay_t *anjay) {
    if (OBJ_DEF_PTR) {
        if (anjay_unregister_object(anjay, OBJ_DEF_PTR)) {
            DEVICE_OBJ_LOG(ERROR, "Error during unregistering Device Object");
        }
        device_object_release(&OBJ_DEF_PTR);
    }
}

void device_object_update(anjay_t *anjay) {
    if (!OBJ_DEF_PTR) {
        return;
    }
    device_t *obj = get_obj(OBJ_DEF_PTR);

    if (obj->reboot) {
        DEVICE_OBJ_LOG(INFO, "Rebooting...\n");
        system_reset();
    }
    anjay_notify_changed(anjay, 3, 0, RID_CURRENT_TIME);
}
