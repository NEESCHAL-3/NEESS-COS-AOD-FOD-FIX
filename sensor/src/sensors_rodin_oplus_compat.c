#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "RodinSensorCompat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)

#define REAL_HAL_PATH "/vendor/lib64/hw/sensors.mediatek.V2.0.so"

#define HW_MODULE_TAG 0x48574d54u

#define XIAOMI_PICKUP_TYPE 33171036
#define OPLUS_TILT_TYPE    65611
#define OPLUS_TILT_HANDLE  0x6f11

typedef struct hw_module_t hw_module_t;
typedef struct hw_device_t hw_device_t;
typedef struct sensors_module_t sensors_module_t;
typedef struct sensor_dev_prefix sensor_dev_prefix;

typedef struct {
    int (*open)(const hw_module_t *,
                const char *,
                hw_device_t **);
} hw_module_methods_t;

struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version;
    uint16_t hal_api_version;

    const char *id;
    const char *name;
    const char *author;

    hw_module_methods_t *methods;
    void *dso;

    uint64_t reserved[25];
};

struct hw_device_t {
    uint32_t tag;
    uint32_t version;

    hw_module_t *module;

    uint64_t reserved[12];

    int (*close)(hw_device_t *);
};

typedef struct {
    const char *name;
    const char *vendor;

    int32_t version;
    int32_t handle;
    int32_t type;

    float maxRange;
    float resolution;
    float power;

    int32_t minDelay;

    uint32_t fifoReservedEventCount;
    uint32_t fifoMaxEventCount;

    const char *stringType;
    const char *requiredPermission;

    int64_t maxDelay;
    uint32_t flags;

    void *reserved[2];
} sensor_t;

typedef struct {
    int32_t version;
    int32_t sensor;
    int32_t type;
    int32_t reserved0;

    int64_t timestamp;

    float data[16];

    uint32_t flags;
    uint32_t reserved1[3];
} sensors_event_t;

typedef int (*activate_fn)(
    sensor_dev_prefix *, int, int);

typedef int (*setdelay_fn)(
    sensor_dev_prefix *, int, int64_t);

typedef int (*poll_fn)(
    sensor_dev_prefix *,
    sensors_event_t *,
    int);

typedef int (*batch_fn)(
    sensor_dev_prefix *,
    int,
    int,
    int64_t,
    int64_t);

typedef int (*flush_fn)(
    sensor_dev_prefix *,
    int);

typedef int (*inject_fn)(
    sensor_dev_prefix *,
    const sensors_event_t *);

struct sensor_dev_prefix {
    hw_device_t common;

    activate_fn activate;
    setdelay_fn setDelay;
    poll_fn poll;

    batch_fn batch;
    flush_fn flush;
    inject_fn inject_sensor_data;
};

struct sensors_module_t {
    hw_module_t common;

    int (*get_sensors_list)(
        sensors_module_t *,
        const sensor_t **);

    int (*set_operation_mode)(
        unsigned int);
};

_Static_assert(sizeof(hw_module_t) == 248,
               "hw_module_t ABI mismatch");

_Static_assert(sizeof(sensors_module_t) == 264,
               "sensors_module_t ABI mismatch");

_Static_assert(offsetof(hw_device_t, close) == 0x70,
               "close offset mismatch");

_Static_assert(offsetof(sensor_dev_prefix, activate) == 0x78,
               "activate offset mismatch");

_Static_assert(offsetof(sensor_dev_prefix, setDelay) == 0x80,
               "setDelay offset mismatch");

_Static_assert(offsetof(sensor_dev_prefix, poll) == 0x88,
               "poll offset mismatch");

_Static_assert(offsetof(sensor_dev_prefix, batch) == 0x90,
               "batch offset mismatch");

_Static_assert(offsetof(sensor_dev_prefix, flush) == 0x98,
               "flush offset mismatch");

_Static_assert(offsetof(sensor_dev_prefix, inject_sensor_data) == 0xa0,
               "inject offset mismatch");

_Static_assert(sizeof(sensor_t) == 104,
               "sensor_t ABI mismatch");

_Static_assert(sizeof(sensors_event_t) == 104,
               "sensors_event_t ABI mismatch");

static void *g_real_dso;
static sensors_module_t *g_real_module;

static pthread_once_t g_real_once =
    PTHREAD_ONCE_INIT;

static pthread_mutex_t g_lock =
    PTHREAD_MUTEX_INITIALIZER;

static int g_real_opened;

static sensor_t *g_list;
static int g_list_count;

static int g_pickup_handle = -1;
static int g_synth_handle = OPLUS_TILT_HANDLE;

static sensor_dev_prefix *g_dev;

static int (*g_orig_close)(hw_device_t *);
static activate_fn g_orig_activate;
static setdelay_fn g_orig_setDelay;
static poll_fn g_orig_poll;
static batch_fn g_orig_batch;
static flush_fn g_orig_flush;
static inject_fn g_orig_inject;

static int g_pickup_enabled;
static int g_synth_enabled;

static sensors_event_t *g_scratch;
static size_t g_scratch_cap;

#define PENDING_MAX 32

static sensors_event_t g_pending[PENDING_MAX];
static unsigned g_pending_head;
static unsigned g_pending_count;

static unsigned g_synth_event_log_count;

static void load_real_once(void)
{
    g_real_dso =
        dlopen(REAL_HAL_PATH,
               RTLD_NOW | RTLD_LOCAL);

    if (!g_real_dso) {
        LOGE("dlopen real HAL failed: %s",
             dlerror());
        return;
    }

    g_real_module =
        (sensors_module_t *)
        dlsym(g_real_dso, "HMI");

    if (!g_real_module) {
        LOGE("real HMI not found: %s",
             dlerror());
        return;
    }

    LOGI("real HAL loaded tag=0x%08x moduleApi=%u halApi=%u",
         g_real_module->common.tag,
         g_real_module->common.module_api_version,
         g_real_module->common.hal_api_version);
}

static int ensure_real(void)
{
    pthread_once(&g_real_once,
                 load_real_once);

    return g_real_module != NULL;
}

static int handle_exists(
    const sensor_t *list,
    int count,
    int handle)
{
    for (int i = 0; i < count; ++i) {
        if (list[i].handle == handle)
            return 1;
    }

    return 0;
}

static int build_sensor_list_locked(void)
{
    if (g_list)
        return g_list_count;

    if (!g_real_opened) {
        LOGE("sensor list requested before real HAL open");
        return 0;
    }

    if (!g_real_module ||
        !g_real_module->get_sensors_list) {
        LOGE("real get_sensors_list missing");
        return 0;
    }

    const sensor_t *real_list = NULL;

    int real_count =
        g_real_module->get_sensors_list(
            g_real_module,
            &real_list);

    LOGI("real get_sensors_list count=%d list=%p",
         real_count, real_list);

    if (real_count > 0 && real_list) {
        for (int i = 0; i < real_count; ++i) {
            LOGI("real[%d] handle=%d type=%d name=%s",
                 i,
                 real_list[i].handle,
                 real_list[i].type,
                 real_list[i].name ?
                     real_list[i].name :
                     "(null)");
        }
    }

    if (real_count <= 0 ||
        !real_list) {
        LOGE("real sensor list invalid count=%d ptr=%p",
             real_count,
             real_list);
        return 0;
    }

    g_pickup_handle = -1;

    for (int i = 0;
         i < real_count;
         ++i) {

        if (real_list[i].type ==
            XIAOMI_PICKUP_TYPE) {

            g_pickup_handle =
                real_list[i].handle;

            LOGI("found Xiaomi pickup handle=%d flags=0x%x name=%s",
                 g_pickup_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");

            break;
        }
    }

    if (g_pickup_handle < 0) {
        LOGE("Xiaomi pickup type %d not found",
             XIAOMI_PICKUP_TYPE);

        g_list =
            calloc((size_t)real_count,
                   sizeof(sensor_t));

        if (!g_list)
            return 0;

        memcpy(g_list,
               real_list,
               (size_t)real_count *
               sizeof(sensor_t));

        g_list_count = real_count;

        return g_list_count;
    }

    while (handle_exists(
               real_list,
               real_count,
               g_synth_handle)) {

        ++g_synth_handle;
    }

    g_list =
        calloc((size_t)real_count + 1,
               sizeof(sensor_t));

    if (!g_list) {
        LOGE("calloc sensor list failed");
        return 0;
    }

    memcpy(g_list,
           real_list,
           (size_t)real_count *
           sizeof(sensor_t));

    sensor_t *src = NULL;

    for (int i = 0;
         i < real_count;
         ++i) {

        if (real_list[i].handle ==
            g_pickup_handle) {

            src = &g_list[i];
            break;
        }
    }

    if (!src) {
        free(g_list);
        g_list = NULL;
        return 0;
    }

    sensor_t *dst =
        &g_list[real_count];

    *dst = *src;

    dst->name =
        "Oplus Tilt Detector Compat";

    dst->vendor =
        "RodinCompat";

    dst->handle =
        g_synth_handle;

    dst->type =
        OPLUS_TILT_TYPE;

    dst->stringType =
        "oplus.sensor.tilt_detector";

    g_list_count =
        real_count + 1;

    LOGI("added synthetic type=%d handle=%d from pickup=%d flags=0x%x",
         OPLUS_TILT_TYPE,
         g_synth_handle,
         g_pickup_handle,
         dst->flags);

    return g_list_count;
}

static void queue_pending_locked(
    const sensors_event_t *ev)
{
    if (g_pending_count >=
        PENDING_MAX) {

        LOGW("pending queue full, dropping synthetic event");
        return;
    }

    unsigned slot =
        (g_pending_head +
         g_pending_count) %
        PENDING_MAX;

    g_pending[slot] = *ev;

    ++g_pending_count;
}

static int pop_pending_locked(
    sensors_event_t *out,
    int max)
{
    int n = 0;

    while (n < max &&
           g_pending_count) {

        out[n++] =
            g_pending[g_pending_head];

        g_pending_head =
            (g_pending_head + 1) %
            PENDING_MAX;

        --g_pending_count;
    }

    return n;
}

static int compat_activate(
    sensor_dev_prefix *dev,
    int handle,
    int enabled)
{
    if (!g_orig_activate)
        return -EINVAL;

    enabled = !!enabled;

    if (handle != g_synth_handle &&
        handle != g_pickup_handle) {

        return g_orig_activate(
            dev,
            handle,
            enabled);
    }

    pthread_mutex_lock(&g_lock);

    int old_pickup =
        g_pickup_enabled;

    int old_synth =
        g_synth_enabled;

    int old_real =
        old_pickup ||
        old_synth;

    if (handle ==
        g_synth_handle) {

        g_synth_enabled =
            enabled;
    } else {
        g_pickup_enabled =
            enabled;
    }

    int new_real =
        g_pickup_enabled ||
        g_synth_enabled;

    int rc = 0;

    if (old_real != new_real) {
        rc =
            g_orig_activate(
                dev,
                g_pickup_handle,
                new_real);

        if (rc != 0) {
            g_pickup_enabled =
                old_pickup;

            g_synth_enabled =
                old_synth;
        }
    }

    LOGI("activate handle=%d enabled=%d pickupLogical=%d synthLogical=%d real=%d rc=%d",
         handle,
         enabled,
         g_pickup_enabled,
         g_synth_enabled,
         new_real,
         rc);

    pthread_mutex_unlock(&g_lock);

    return rc;
}

static int compat_setDelay(
    sensor_dev_prefix *dev,
    int handle,
    int64_t ns)
{
    if (!g_orig_setDelay)
        return -EINVAL;

    if (handle ==
        g_synth_handle) {

        handle =
            g_pickup_handle;
    }

    return g_orig_setDelay(
        dev,
        handle,
        ns);
}

static int compat_batch(
    sensor_dev_prefix *dev,
    int handle,
    int flags,
    int64_t sampling_ns,
    int64_t latency_ns)
{
    if (!g_orig_batch)
        return -EINVAL;

    if (handle ==
        g_synth_handle) {

        handle =
            g_pickup_handle;
    }

    return g_orig_batch(
        dev,
        handle,
        flags,
        sampling_ns,
        latency_ns);
}

static int compat_flush(
    sensor_dev_prefix *dev,
    int handle)
{
    if (!g_orig_flush)
        return -EINVAL;

    /*
     * Avoid returning a flush-complete event
     * carrying the real Xiaomi handle for
     * the synthetic Oplus sensor.
     */
    if (handle ==
        g_synth_handle) {

        return -EINVAL;
    }

    return g_orig_flush(
        dev,
        handle);
}

static int compat_inject(
    sensor_dev_prefix *dev,
    const sensors_event_t *event)
{
    if (!g_orig_inject)
        return -EINVAL;

    if (event &&
        event->sensor ==
            g_synth_handle) {

        return -EINVAL;
    }

    return g_orig_inject(
        dev,
        event);
}

static int ensure_scratch(
    size_t count)
{
    if (count <=
        g_scratch_cap)
        return 1;

    sensors_event_t *p =
        realloc(
            g_scratch,
            count *
            sizeof(sensors_event_t));

    if (!p)
        return 0;

    g_scratch = p;
    g_scratch_cap = count;

    return 1;
}

static int compat_poll(
    sensor_dev_prefix *dev,
    sensors_event_t *out,
    int count)
{
    if (!g_orig_poll ||
        !out ||
        count <= 0) {

        return -EINVAL;
    }

    pthread_mutex_lock(&g_lock);

    int pending =
        pop_pending_locked(
            out,
            count);

    pthread_mutex_unlock(&g_lock);

    if (pending > 0)
        return pending;

    if (!ensure_scratch(
            (size_t)count)) {

        return -ENOMEM;
    }

    int n =
        g_orig_poll(
            dev,
            g_scratch,
            count);

    if (n <= 0)
        return n;

    pthread_mutex_lock(&g_lock);

    int pickup_logical =
        g_pickup_enabled;

    int synth_logical =
        g_synth_enabled;

    int written = 0;

    for (int i = 0;
         i < n;
         ++i) {

        sensors_event_t ev =
            g_scratch[i];

        int is_pickup =
            (g_pickup_handle >= 0 &&
             ev.sensor ==
                g_pickup_handle);

        if (!is_pickup) {
            if (written < count)
                out[written++] = ev;

            continue;
        }

        /*
         * Preserve the real Xiaomi event only
         * if some real client actually enabled
         * the Xiaomi pickup sensor.
         */
        if (pickup_logical &&
            written < count) {

            out[written++] = ev;
        }

        /*
         * Rodin pickup:
         *   value 1.0 = pickup
         *
         * ColorOS synthetic 65611 expects:
         *   value 0.0 = wake/pickup callback
         */
        if (synth_logical &&
            ev.data[0] == 1.0f) {

            sensors_event_t syn =
                ev;

            syn.sensor =
                g_synth_handle;

            syn.type =
                OPLUS_TILT_TYPE;

            memset(
                syn.data,
                0,
                sizeof(syn.data));

            if (written < count) {
                out[written++] =
                    syn;
            } else {
                queue_pending_locked(
                    &syn);
            }

            if (g_synth_event_log_count <
                20) {

                LOGI("pickup -> synthetic 65611 value=0 ts=%" PRId64,
                     syn.timestamp);

                ++g_synth_event_log_count;
            }
        }
    }

    pthread_mutex_unlock(&g_lock);

    /*
     * It is possible that the real poll returned
     * only a suppressed Xiaomi pickup event with
     * no synthetic event (for example value=2).
     * Poll again rather than returning zero.
     */
    if (written == 0) {
        return compat_poll(
            dev,
            out,
            count);
    }

    return written;
}

static int compat_close(
    hw_device_t *hw)
{
    sensor_dev_prefix *dev =
        (sensor_dev_prefix *)hw;

    pthread_mutex_lock(&g_lock);

    int (*orig_close)(
        hw_device_t *) =
        g_orig_close;

    /*
     * Restore the real MTK callback table before
     * handing the genuine object back to its close.
     */
    if (dev == g_dev) {
        dev->common.close =
            g_orig_close;

        dev->activate =
            g_orig_activate;

        dev->setDelay =
            g_orig_setDelay;

        dev->poll =
            g_orig_poll;

        dev->batch =
            g_orig_batch;

        dev->flush =
            g_orig_flush;

        dev->inject_sensor_data =
            g_orig_inject;

        g_dev = NULL;

        g_pickup_enabled = 0;
        g_synth_enabled = 0;

        g_pending_head = 0;
        g_pending_count = 0;
    }

    pthread_mutex_unlock(&g_lock);

    if (orig_close)
        return orig_close(hw);

    return 0;
}

static int compat_open(
    const hw_module_t *module,
    const char *id,
    hw_device_t **device)
{
    (void)module;

    if (!device)
        return -EINVAL;

    if (!ensure_real() ||
        !g_real_module->common.methods ||
        !g_real_module->common.methods->open) {

        LOGE("real module/open unavailable");
        return -ENODEV;
    }

    hw_device_t *real_hw =
        NULL;

    int rc =
        g_real_module->common.methods->open(
            &g_real_module->common,
            id,
            &real_hw);

    if (rc != 0 ||
        !real_hw) {

        LOGE("real open failed rc=%d dev=%p",
             rc,
             real_hw);

        return rc ?
            rc :
            -ENODEV;
    }

    sensor_dev_prefix *dev =
        (sensor_dev_prefix *)real_hw;

    pthread_mutex_lock(&g_lock);

    g_real_opened = 1;

    /*
     * IMPORTANT:
     * Do not query/cache the MTK sensor list here.
     * Let the multihal request it through
     * compat_get_sensors_list() at the normal time.
     */

    if (g_dev &&
        g_dev != dev) {

        LOGE("unexpected second sensor device %p existing=%p",
             dev,
             g_dev);

        pthread_mutex_unlock(
            &g_lock);

        if (real_hw->close)
            real_hw->close(
                real_hw);

        return -EBUSY;
    }

    if (!g_dev) {
        g_dev = dev;

        g_orig_close =
            dev->common.close;

        g_orig_activate =
            dev->activate;

        g_orig_setDelay =
            dev->setDelay;

        g_orig_poll =
            dev->poll;

        g_orig_batch =
            dev->batch;

        g_orig_flush =
            dev->flush;

        g_orig_inject =
            dev->inject_sensor_data;

        dev->common.close =
            compat_close;

        dev->activate =
            compat_activate;

        dev->setDelay =
            compat_setDelay;

        dev->poll =
            compat_poll;

        dev->batch =
            compat_batch;

        dev->flush =
            compat_flush;

        dev->inject_sensor_data =
            compat_inject;

        LOGI("patched real MTK device=%p pickupHandle=%d synthHandle=%d",
             dev,
             g_pickup_handle,
             g_synth_handle);
    }

    pthread_mutex_unlock(
        &g_lock);

    *device =
        real_hw;

    return 0;
}

static int compat_get_sensors_list(
    sensors_module_t *module,
    const sensor_t **list)
{
    (void)module;

    if (!list)
        return -EINVAL;

    if (!ensure_real())
        return 0;

    pthread_mutex_lock(&g_lock);

    int count =
        build_sensor_list_locked();

    *list =
        g_list;

    pthread_mutex_unlock(&g_lock);

    return count;
}

static int compat_set_operation_mode(
    unsigned int mode)
{
    if (!ensure_real() ||
        !g_real_module->set_operation_mode) {

        return -EINVAL;
    }

    return
        g_real_module->
        set_operation_mode(
            mode);
}

static hw_module_methods_t
g_methods = {
    .open = compat_open
};

__attribute__((visibility("default")))
sensors_module_t HMI = {
    .common = {
        .tag = HW_MODULE_TAG,
        .module_api_version = 1,
        .hal_api_version = 0,

        .id = "sensors",

        .name =
            "Rodin Oplus Sensor Compatibility HAL",

        .author =
            "RodinCompat",

        .methods =
            &g_methods,

        .dso =
            NULL,

        .reserved =
            {0}
    },

    .get_sensors_list =
        compat_get_sensors_list,

    .set_operation_mode =
        compat_set_operation_mode
};
