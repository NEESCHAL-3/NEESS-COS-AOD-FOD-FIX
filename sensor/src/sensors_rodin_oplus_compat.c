#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "RodinSensorCompat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)

#define REAL_HAL_PATH "/vendor/lib64/hw/sensors.mediatek.V2.0.so"

#define HW_MODULE_TAG 0x48574d54u

#define XIAOMI_PICKUP_TYPE    33171036
#define XIAOMI_FOD_TYPE       33171030
#define XIAOMI_AOD_TYPE       33171029
#define XIAOMI_IMU_HAND_TYPE  33171120
#define XIAOMI_ALS_RAW_TYPE   33171111
#define SENSOR_TYPE_LIGHT     5
#define MTK_TILT_TYPE         22
#define MTK_SMD_TYPE          17
#define OPLUS_TILT_TYPE       65611
#define OPLUS_TILT_HANDLE     0x6f11

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
static int g_tilt_handle = -1;
static int g_fod_handle = -1;
static int g_aod_handle = -1;
static int g_smd_handle = -1;
static int g_hand_handle = -1;
static int g_als_handle = 5;
static int g_als_raw_handle = 0x5e;
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
static int g_tilt_enabled;
static int g_als_enabled;
static float g_last_lux = -1.0f;
static int64_t g_last_lux_report_ts;

static sensors_event_t *g_scratch;
static size_t g_scratch_cap;

#define PENDING_MAX 64

static sensors_event_t g_pending[PENDING_MAX];
static unsigned g_pending_head;
static unsigned g_pending_count;

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
    g_tilt_handle = -1;
    g_fod_handle = -1;
    g_aod_handle = -1;
    g_smd_handle = -1;
    g_hand_handle = -1;

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
        } else if (real_list[i].type == MTK_TILT_TYPE) {
            g_tilt_handle =
                real_list[i].handle;

            LOGI("found MTK tilt handle=%d flags=0x%x name=%s",
                 g_tilt_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
        } else if (real_list[i].type == XIAOMI_FOD_TYPE) {
            g_fod_handle =
                real_list[i].handle;

            LOGI("found Xiaomi FOD handle=%d flags=0x%x name=%s",
                 g_fod_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
        } else if (real_list[i].type == XIAOMI_AOD_TYPE) {
            g_aod_handle =
                real_list[i].handle;

            LOGI("found Xiaomi AOD handle=%d flags=0x%x name=%s",
                 g_aod_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
        } else if (real_list[i].type == MTK_SMD_TYPE) {
            g_smd_handle =
                real_list[i].handle;

            LOGI("found MTK SMD handle=%d flags=0x%x name=%s",
                 g_smd_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
        } else if (real_list[i].type == XIAOMI_IMU_HAND_TYPE) {
            g_hand_handle =
                real_list[i].handle;

            LOGI("found Xiaomi IMU hand handle=%d flags=0x%x name=%s",
                 g_hand_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
        } else if (real_list[i].type == SENSOR_TYPE_LIGHT) {
            g_als_handle =
                real_list[i].handle;

            LOGI("found front ALS handle=%d flags=0x%x name=%s",
                 g_als_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
        } else if (real_list[i].type == XIAOMI_ALS_RAW_TYPE) {
            g_als_raw_handle =
                real_list[i].handle;

            LOGI("found front ALS raw handle=%d flags=0x%x name=%s",
                 g_als_raw_handle,
                 real_list[i].flags,
                 real_list[i].name ?
                    real_list[i].name :
                    "(null)");
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

    if (g_als_handle >= 0 && handle == g_als_handle) {
        pthread_mutex_lock(&g_lock);
        g_als_enabled = enabled;
        LOGI("activate front ALS handle=%d enabled=%d (raw_handle=%d)",
             g_als_handle, enabled, g_als_raw_handle);
        int r1 = g_orig_activate(dev, g_als_handle, enabled);
        int r2 = 0;
        if (g_als_raw_handle >= 0) {
            r2 = g_orig_activate(dev, g_als_raw_handle, enabled);
            if (enabled && g_orig_setDelay) {
                g_orig_setDelay(dev, g_als_raw_handle, 200000000LL);
            }
        }
        pthread_mutex_unlock(&g_lock);
        return (r1 == 0) ? r1 : r2;
    }

    if (handle != g_synth_handle &&
        handle != g_pickup_handle &&
        handle != g_tilt_handle) {

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

    int old_tilt =
        g_tilt_enabled;

    int old_real =
        old_pickup ||
        old_synth ||
        old_tilt;

    int rc = 0;

    if (handle == g_synth_handle) {

        g_synth_enabled =
            enabled;
    } else if (handle == g_pickup_handle) {
        g_pickup_enabled =
            enabled;
    } else if (handle == g_tilt_handle) {
        g_tilt_enabled =
            enabled;
    }

    int new_real =
        g_pickup_enabled ||
        g_synth_enabled ||
        g_tilt_enabled;

    if (old_real != new_real) {
        if (g_pickup_handle >= 0) {
            int r = g_orig_activate(
                dev,
                g_pickup_handle,
                new_real);
            LOGI("activate pickup handle=%d -> rc=%d", g_pickup_handle, r);
            if (rc == 0) rc = r;
        }

        if (rc != 0 && g_pickup_handle >= 0) {
            g_pickup_enabled =
                old_pickup;

            g_synth_enabled =
                old_synth;

            g_tilt_enabled =
                old_tilt;

        }
    }

    LOGI("activate handle=%d enabled=%d pickupLogical=%d synthLogical=%d tiltLogical=%d real=%d rc=%d (tilt=%d fod=%d aod=%d smd=%d hand=%d)",
         handle,
         enabled,
         g_pickup_enabled,
         g_synth_enabled,
         g_tilt_enabled,
         new_real,
         rc,
         g_tilt_handle,
         g_fod_handle,
         g_aod_handle,
         g_smd_handle,
         g_hand_handle);

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

    if (g_als_handle >= 0 && handle == g_als_handle) {
        if (g_als_raw_handle >= 0) {
            g_orig_setDelay(dev, g_als_raw_handle, ns);
        }
        return g_orig_setDelay(dev, handle, ns);
    }

    if (handle == g_synth_handle ||
        handle == g_tilt_handle) {

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

    if (g_als_handle >= 0 && handle == g_als_handle) {
        if (g_als_raw_handle >= 0) {
            g_orig_batch(dev, g_als_raw_handle, flags, sampling_ns, latency_ns);
        }
        return g_orig_batch(dev, handle, flags, sampling_ns, latency_ns);
    }

    if (handle == g_synth_handle ||
        handle == g_tilt_handle) {

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

struct cali_point {
    float bl;
    float clear;
    float ir;
};

static const struct cali_point k_cali[] = {
    { 0.0f,    0.0f,     0.0f },
    { 10.0f,   125.0f,   15.0f },
    { 50.0f,   547.0f,   71.0f },
    { 100.0f,  1071.0f,  137.0f },
    { 150.0f,  1573.0f,  198.0f },
    { 200.0f,  2108.0f,  269.0f },
    { 250.0f,  2619.0f,  331.0f },
    { 300.0f,  3151.0f,  400.0f },
    { 350.0f,  3520.0f,  446.0f },
    { 400.0f,  3914.0f,  497.0f },
    { 450.0f,  4333.0f,  549.0f },
    { 500.0f,  4779.0f,  605.0f },
    { 550.0f,  5294.0f,  670.0f },
    { 600.0f,  5833.0f,  739.0f },
    { 650.0f,  6422.0f,  814.0f },
    { 800.0f,  8297.0f,  1041.0f },
    { 950.0f,  9742.0f,  1224.0f },
    { 1100.0f, 11436.0f, 1433.0f },
    { 1250.0f, 13139.0f, 1651.0f },
    { 1400.0f, 14667.0f, 1838.0f },
    { 1550.0f, 16309.0f, 2045.0f },
    { 1700.0f, 18034.0f, 2259.0f },
    { 1850.0f, 19634.0f, 2457.0f },
    { 2047.0f, 21931.0f, 2740.0f },
    { 2300.0f, 25363.0f, 3171.0f },
    { 2600.0f, 30336.0f, 3766.0f },
};
#define CALI_LEN (sizeof(k_cali) / sizeof(k_cali[0]))

static int g_backlight_fd = -1;

static float get_current_backlight(void) {
    if (g_backlight_fd < 0) {
        g_backlight_fd = open("/sys/class/mi_display/disp-DSI-0/backlight", O_RDONLY | O_CLOEXEC);
        if (g_backlight_fd < 0) {
            g_backlight_fd = open("/sys/devices/virtual/mi_display/disp-DSI-0/backlight", O_RDONLY | O_CLOEXEC);
        }
    }
    if (g_backlight_fd >= 0) {
        char buf[32];
        ssize_t n = pread(g_backlight_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            return (float)atoi(buf);
        }
    }
    return 0.0f;
}

static void get_display_leakage(float bl, float *out_clear, float *out_ir) {
    if (bl <= 0.0f) {
        *out_clear = 0.0f;
        *out_ir = 0.0f;
        return;
    }
    if (bl >= k_cali[CALI_LEN - 1].bl) {
        float d_bl = bl - k_cali[CALI_LEN - 1].bl;
        *out_clear = k_cali[CALI_LEN - 1].clear + 16.577f * d_bl;
        *out_ir = k_cali[CALI_LEN - 1].ir + 1.983f * d_bl;
        return;
    }
    for (size_t i = 1; i < CALI_LEN; ++i) {
        if (bl <= k_cali[i].bl) {
            float frac = (bl - k_cali[i-1].bl) / (k_cali[i].bl - k_cali[i-1].bl);
            *out_clear = k_cali[i-1].clear + frac * (k_cali[i].clear - k_cali[i-1].clear);
            *out_ir = k_cali[i-1].ir + frac * (k_cali[i].ir - k_cali[i-1].ir);
            return;
        }
    }
    *out_clear = k_cali[CALI_LEN - 1].clear;
    *out_ir = k_cali[CALI_LEN - 1].ir;
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

    int tilt_logical =
        g_tilt_enabled;

    int written = 0;

    for (int i = 0;
         i < n;
         ++i) {

        sensors_event_t ev =
            g_scratch[i];

        int is_als = (g_als_handle >= 0 && ev.sensor == g_als_handle);
        if (is_als) {
            /* Drop dummy/stale events from real HAL (e.g. boot-time 1.00) */
            continue;
        }

        int is_als_raw = ((g_als_raw_handle >= 0 && ev.sensor == g_als_raw_handle) ||
                          ev.type == XIAOMI_ALS_RAW_TYPE);
        if (is_als_raw) {
            float clear = ev.data[0];
            float ir = ev.data[1];

            float bl = get_current_backlight();
            float leak_c = 0.0f, leak_ir = 0.0f;
            get_display_leakage(bl, &leak_c, &leak_ir);

            float c_net = clear - leak_c;
            float ir_net = ir - leak_ir;
            if (c_net < 0.0f) c_net = 0.0f;
            if (ir_net < 0.0f) ir_net = 0.0f;

            float lux = 0.0f;
            if (c_net >= 80.0f) {
                float ratio = ir_net / (c_net + 0.001f);
                float scale = 79.832f / 6400.0f;
                if (ratio < 4.9f) {
                    float l1 = (c_net - 0.265f * ir_net) * scale;
                    float l2 = (0.797f * c_net - 0.160f * ir_net) * scale;
                    lux = (l1 > l2) ? l1 : l2;
                } else {
                    lux = (c_net * 0.10f) * scale;
                }
                if (lux < 0.0f) lux = 0.0f;
            }

            if (written < count) {
                out[written++] = ev;
            } else {
                queue_pending_locked(&ev);
            }

            if (g_als_enabled) {
                int64_t now_ts = ev.timestamp;
                float diff = fabsf(lux - g_last_lux);
                if (g_last_lux < 0.0f || diff >= 1.0f || (diff / (g_last_lux + 0.1f) >= 0.05f) ||
                    (lux == 0.0f && g_last_lux > 0.0f) ||
                    (now_ts - g_last_lux_report_ts >= 1000000000LL)) {

                    sensors_event_t syn;
                    memset(&syn, 0, sizeof(syn));
                    syn.version = sizeof(sensors_event_t);
                    syn.sensor = g_als_handle >= 0 ? g_als_handle : 5;
                    syn.type = SENSOR_TYPE_LIGHT;
                    syn.timestamp = now_ts;
                    syn.data[0] = lux;

                    g_last_lux = lux;
                    g_last_lux_report_ts = now_ts;

                    if (written < count) {
                        out[written++] = syn;
                    } else {
                        queue_pending_locked(&syn);
                    }

                    LOGI("synthesized front ALS lux=%.2f (c_net=%.1f ir_net=%.1f c_raw=%.1f bl=%.0f) -> handle=%d",
                         lux, c_net, ir_net, clear, bl, syn.sensor);
                }
            }
            continue;
        }

        int is_pickup =
            (g_pickup_handle >= 0 &&
             ev.sensor ==
                g_pickup_handle);

        int is_tilt =
            (g_tilt_handle >= 0 &&
             ev.sensor ==
                g_tilt_handle);

        if (!is_pickup) {
            /* Oplus expects 0 for lift; the MTK tilt detector emits 1. */
            if (is_tilt && ev.data[0] > 0.0f)
                ev.data[0] = 0.0f;

            if (written < count)
                out[written++] = ev;

            continue;
        }

        /*
         * Preserve the real Xiaomi pickup event only
         * if some real client actually enabled
         * the Xiaomi pickup sensor.
         */
        if (is_pickup && pickup_logical &&
            written < count) {

            out[written++] = ev;
        }

        /* Xiaomi pickup fires on gentler motion than MTK tilt. */
        if (ev.data[0] == 1.0f && (synth_logical || tilt_logical)) {
                sensors_event_t syn =
                    ev;

                syn.sensor =
                    tilt_logical ? g_tilt_handle : g_synth_handle;

                syn.type =
                    tilt_logical ? MTK_TILT_TYPE : OPLUS_TILT_TYPE;

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

                LOGI("instant pickup trigger (handle=%d val=%.1f) -> tilt type=%d value=0 ts=%" PRId64,
                     ev.sensor,
                     ev.data[0],
                     syn.type,
                     syn.timestamp);

                if (tilt_logical && synth_logical) {
                    syn.sensor = g_synth_handle;
                    syn.type = OPLUS_TILT_TYPE;
                    if (written < count)
                        out[written++] = syn;
                    else
                        queue_pending_locked(&syn);
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
        g_tilt_enabled = 0;
        g_als_enabled = 0;
        g_last_lux = -1.0f;
        g_last_lux_report_ts = 0;

        if (g_backlight_fd >= 0) {
            close(g_backlight_fd);
            g_backlight_fd = -1;
        }

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
