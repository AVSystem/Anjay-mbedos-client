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

/**
 * Generated by anjay_codegen.py on 2020-05-20 16:28:35
 *
 * LwM2M Object: Accelerometer
 * ID: 3313, URN: urn:oma:lwm2m:ext:3313, Optional, Multiple
 *
 * This IPSO object can be used to represent a 1-3 axis accelerometer.
 */
#include <algorithm>
#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list_cxx.hpp>
#include <avsystem/commons/avs_log.h>

#ifdef TARGET_DISCO_L496AG

#    include <XNucleoIKS01A2.h>

#    include "accelerometer.h"

#    define ACCELEROMETER_OBJ_LOG(...) avs_log(accelerometer_obj, __VA_ARGS__)

#    define SENSOR_ID LSM303AGR_ACC_WHO_AM_I

using namespace std;

namespace {

/**
 * Sensor Units: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Measurement Units Definition.
 */
constexpr anjay_rid_t RID_SENSOR_UNITS = 5701;

/**
 * X Value: R, Single, Mandatory
 * type: float, range: N/A, unit: N/A
 * The measured value along the X axis.
 */
constexpr anjay_rid_t RID_X_VALUE = 5702;

/**
 * Y Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The measured value along the Y axis.
 */
constexpr anjay_rid_t RID_Y_VALUE = 5703;

/**
 * Z Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The measured value along the Z axis.
 */
constexpr anjay_rid_t RID_Z_VALUE = 5704;

constexpr anjay_oid_t ACCELEROMETER_OID = 3313;

struct AccelerometerObject {
    const anjay_dm_object_def_t *const def;

    LSM303AGRAccSensor *sensor;

    int32_t x_value;
    int32_t y_value;
    int32_t z_value;

    AccelerometerObject();
    ~AccelerometerObject();
    AccelerometerObject(const AccelerometerObject &) = delete;
    AccelerometerObject &operator=(const AccelerometerObject &) = delete;
};

inline AccelerometerObject *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    static const AccelerometerObject *const FAKE_OBJECT_PTR = nullptr;
    static const auto DEF_PTR_OFFSET =
            intptr_t(reinterpret_cast<const char *>(&FAKE_OBJECT_PTR[1].def)
                     - reinterpret_cast<const char *>(&FAKE_OBJECT_PTR[1]));
    return reinterpret_cast<AccelerometerObject *>(intptr_t(obj_ptr)
                                                   - DEF_PTR_OFFSET);
}

int list_resources(anjay_t *,
                   const anjay_dm_object_def_t *const *,
                   anjay_iid_t,
                   anjay_dm_resource_list_ctx_t *ctx) {
    anjay_dm_emit_res(ctx, RID_SENSOR_UNITS, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_X_VALUE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_Y_VALUE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_Z_VALUE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    return 0;
}

int resource_read(anjay_t *,
                  const anjay_dm_object_def_t *const *obj_ptr,
                  anjay_iid_t iid,
                  anjay_rid_t rid,
                  anjay_riid_t riid,
                  anjay_output_ctx_t *ctx) {
    AccelerometerObject *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_SENSOR_UNITS:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, "mg");

    case RID_X_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->x_value);

    case RID_Y_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->y_value);

    case RID_Z_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->z_value);

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

struct ObjDef : public anjay_dm_object_def_t {
    ObjDef() : anjay_dm_object_def_t() {
        oid = ACCELEROMETER_OID;

        handlers.list_instances = anjay_dm_list_instances_SINGLE;

        handlers.list_resources = list_resources;
        handlers.resource_read = resource_read;

        handlers.transaction_begin = anjay_dm_transaction_NOOP;
        handlers.transaction_validate = anjay_dm_transaction_NOOP;
        handlers.transaction_commit = anjay_dm_transaction_NOOP;
        handlers.transaction_rollback = anjay_dm_transaction_NOOP;
    }
} const OBJ_DEF;

AccelerometerObject::AccelerometerObject() : def(&OBJ_DEF) {}

AccelerometerObject::~AccelerometerObject() {}

const anjay_dm_object_def_t **accelerometer_object_create(void) {
    LSM303AGRAccSensor *sensor =
            XNucleoIKS01A2::instance(D14, D15)->accelerometer;
    uint8_t id = 0;
    int32_t sensor_value[3];
    if (sensor->read_id(&id) || id != SENSOR_ID || sensor->enable()
            || sensor->get_x_axes(sensor_value)) {
        ACCELEROMETER_OBJ_LOG(WARNING, "Failed to initialize accelerometer");
        return NULL;
    }

    AccelerometerObject *obj = new (nothrow) AccelerometerObject;
    if (!obj) {
        return NULL;
    }

    obj->sensor = sensor;
    obj->x_value = sensor_value[0];
    obj->y_value = sensor_value[1];
    obj->z_value = sensor_value[2];

    return const_cast<const anjay_dm_object_def_t **>(&obj->def);
}

void accelerometer_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        delete get_obj(def);
    }
}

const anjay_dm_object_def_t **OBJ_DEF_PTR;

} // namespace

int accelerometer_object_install(anjay_t *anjay) {
    if (OBJ_DEF_PTR) {
        ACCELEROMETER_OBJ_LOG(
                ERROR, "Accelerometer Object has been already installed");
        return -1;
    }

    OBJ_DEF_PTR = accelerometer_object_create();
    if (!OBJ_DEF_PTR) {
        return 0;
    }
    return anjay_register_object(anjay, OBJ_DEF_PTR);
}

void accelerometer_object_uninstall(anjay_t *anjay) {
    if (OBJ_DEF_PTR) {
        if (anjay_unregister_object(anjay, OBJ_DEF_PTR)) {
            ACCELEROMETER_OBJ_LOG(
                    ERROR, "Error during unregistering Accelerometer Object");
        }
        accelerometer_object_release(OBJ_DEF_PTR);
        OBJ_DEF_PTR = nullptr;
    }
}

void accelerometer_object_update(anjay_t *anjay) {
    if (!OBJ_DEF_PTR) {
        return;
    }

    AccelerometerObject *obj = get_obj(OBJ_DEF_PTR);

    int32_t value[3];
    if (obj->sensor->get_x_axes(value)) {
        ACCELEROMETER_OBJ_LOG(ERROR, "Failed to get accelerometer values");
        return;
    }

    if (value[0] != obj->x_value) {
        obj->x_value = value[0];
        (void) anjay_notify_changed(anjay, ACCELEROMETER_OID, 0, RID_X_VALUE);
    }

    if (value[1] != obj->y_value) {
        obj->y_value = value[1];
        (void) anjay_notify_changed(anjay, ACCELEROMETER_OID, 0, RID_Y_VALUE);
    }

    if (value[2] != obj->z_value) {
        obj->z_value = value[2];
        (void) anjay_notify_changed(anjay, ACCELEROMETER_OID, 0, RID_Z_VALUE);
    }
}

#endif // TARGET_DISCO_L496AG
