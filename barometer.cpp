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

/**
 * Generated by anjay_codegen.py on 2019-10-24 11:45:20
 *
 * LwM2M Object: Barometer
 * ID: 3315, URN: urn:oma:lwm2m:ext:3315, Optional, Multiple
 *
 * Description: This IPSO object should be used with an air pressure
 * sensor to report a barometer measurement.  It also provides resources
 * for minimum/maximum measured values and the minimum/maximum range that
 * can be measured by the barometer sensor. An example measurement unit
 * is kPa (ucum:kPa).
 */
#if (SENSORS_IKS01A2 == 1)

#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_memory.h>

#include <XNucleoIKS01A2.h>

#include "barometer.h"

#define BAROMETER_OBJ_LOG(...) avs_log(barometer_obj, __VA_ARGS__)

#define BAROMETER_OID 3315

/**
 * Min Measured Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The minimum value measured by the sensor since power ON or reset
 */
#define RID_MIN_MEASURED_VALUE 5601

/**
 * Max Measured Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The maximum value measured by the sensor since power ON or reset
 */
#define RID_MAX_MEASURED_VALUE 5602

/**
 * Min Range Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The minimum value that can be measured by the sensor
 */
#define RID_MIN_RANGE_VALUE 5603

/**
 * Max Range Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The maximum value that can be measured by the sensor
 */
#define RID_MAX_RANGE_VALUE 5604

/**
 * Reset Min and Max Measured Values: E, Single, Optional
 * type: N/A, range: N/A, unit: N/A
 * Reset the Min and Max Measured Values to Current Value
 */
#define RID_RESET_MIN_AND_MAX_MEASURED_VALUES 5605

/**
 * Sensor Value: R, Single, Mandatory
 * type: float, range: N/A, unit: N/A
 * Last or Current Measured Value from the Sensor
 */
#define RID_SENSOR_VALUE 5700

/**
 * Sensor Units: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * If present, the type of sensor defined as the UCUM Unit Definition
 * e.g. “Cel” for Temperature in Celcius.
 */
#define RID_SENSOR_UNITS 5701

// Convert from mbar to Pa
constexpr double VALUE_SCALE = 100.0;

#define MIN_RANGE_VALUE 26000.0  // Pa
#define MAX_RANGE_VALUE 126000.0 // Pa

typedef struct barometer_struct {
    const anjay_dm_object_def_t *def;
    LPS22HBSensor *sensor;
    float min_value;
    float max_value;
    float curr_value;
} barometer_t;

static inline barometer_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, barometer_t, def);
}

static void reset_min_max_values(barometer_t *obj) {
    obj->max_value = obj->curr_value;
    obj->min_value = obj->curr_value;
}

static int instance_reset(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid) {
    (void) anjay;
    (void) iid;
    assert(iid == 0);

    barometer_t *obj = get_obj(obj_ptr);
    assert(obj);

    reset_min_max_values(obj);
    return 0;
}

static int list_resources(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_MIN_MEASURED_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_MAX_MEASURED_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_MIN_RANGE_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_MAX_RANGE_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_RESET_MIN_AND_MAX_MEASURED_VALUES,
                      ANJAY_DM_RES_E, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_SENSOR_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_SENSOR_UNITS, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    return 0;
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;
    (void) iid;
    assert(iid == 0);

    barometer_t *obj = get_obj(obj_ptr);
    assert(obj);

    switch (rid) {
    case RID_MIN_MEASURED_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->min_value * VALUE_SCALE);

    case RID_MAX_MEASURED_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->max_value * VALUE_SCALE);

    case RID_MIN_RANGE_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, MIN_RANGE_VALUE);

    case RID_MAX_RANGE_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, MAX_RANGE_VALUE);

    case RID_SENSOR_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->curr_value * VALUE_SCALE);

    case RID_SENSOR_UNITS:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, "Pa");

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
    (void) iid;
    assert(iid == 0);

    barometer_t *obj = get_obj(obj_ptr);
    assert(obj);

    switch (rid) {
    case RID_RESET_MIN_AND_MAX_MEASURED_VALUES:
        reset_min_max_values(obj);
        barometer_object_update(anjay);
        return 0;

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

namespace {

struct ObjDef : public anjay_dm_object_def_t {
    ObjDef() : anjay_dm_object_def_t() {
        oid = BAROMETER_OID;

        handlers.list_instances = anjay_dm_list_instances_SINGLE;
        handlers.instance_reset = instance_reset;

        handlers.list_resources = list_resources;
        handlers.resource_read = resource_read;
        handlers.resource_execute = resource_execute;

        handlers.transaction_begin = anjay_dm_transaction_NOOP;
        handlers.transaction_validate = anjay_dm_transaction_NOOP;
        handlers.transaction_commit = anjay_dm_transaction_NOOP;
        handlers.transaction_rollback = anjay_dm_transaction_NOOP;
    }
} const OBJ_DEF;

const anjay_dm_object_def_t **barometer_object_create(void) {
    LPS22HBSensor *sensor = XNucleoIKS01A2::instance(D14, D15)->pt_sensor;
    float sensor_value;
    if (sensor->enable() || sensor->get_pressure(&sensor_value)) {
        BAROMETER_OBJ_LOG(WARNING, "Failed to initialize barometer");
        return NULL;
    }

    barometer_t *obj = (barometer_t *) avs_calloc(1, sizeof(barometer_t));
    if (!obj) {
        (void) sensor->disable();
        return NULL;
    }
    obj->def = &OBJ_DEF;
    obj->sensor = sensor;
    obj->curr_value = sensor_value;
    reset_min_max_values(obj);

    return &obj->def;
}

void barometer_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        barometer_t *obj = get_obj(def);
        avs_free(obj);
    }
}

const anjay_dm_object_def_t **OBJ_DEF_PTR;

} // namespace

int barometer_object_install(anjay_t *anjay) {
    if (OBJ_DEF_PTR) {
        BAROMETER_OBJ_LOG(ERROR, "Barometer Object has been already installed");
        return -1;
    }

    OBJ_DEF_PTR = barometer_object_create();
    if (!OBJ_DEF_PTR) {
        return 0;
    }
    return anjay_register_object(anjay, OBJ_DEF_PTR);
}

void barometer_object_uninstall(anjay_t *anjay) {
    if (OBJ_DEF_PTR) {
        if (anjay_unregister_object(anjay, OBJ_DEF_PTR)) {
            BAROMETER_OBJ_LOG(ERROR,
                              "Error during unregistering Barometer Object");
        }
        barometer_object_release(OBJ_DEF_PTR);
        OBJ_DEF_PTR = nullptr;
    }
}

void barometer_object_update(anjay_t *anjay) {
    if (!OBJ_DEF_PTR) {
        return;
    }
    barometer_t *obj = get_obj(OBJ_DEF_PTR);

    float value;
    if (obj->sensor->get_pressure(&value)) {
        BAROMETER_OBJ_LOG(ERROR, "Failed to get pressure");
        return;
    }

    if (value != obj->curr_value) {
        obj->curr_value = value;
        (void) anjay_notify_changed(anjay, BAROMETER_OID, 0, RID_SENSOR_VALUE);
    }
    if (value > obj->max_value) {
        obj->max_value = value;
        (void) anjay_notify_changed(anjay, BAROMETER_OID, 0,
                                    RID_MAX_MEASURED_VALUE);
    }
    if (value < obj->min_value) {
        obj->min_value = value;
        (void) anjay_notify_changed(anjay, BAROMETER_OID, 0,
                                    RID_MIN_MEASURED_VALUE);
    }
}

#endif // SENSORS_IKS01A2
