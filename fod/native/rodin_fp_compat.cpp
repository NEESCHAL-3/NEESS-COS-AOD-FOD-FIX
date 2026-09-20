#include <sys/system_properties.h>
#include <sys/wait.h>
#include <android/binder_ibinder.h>

#include <android/binder_parcel.h>
#include <android/binder_status.h>

#if __has_include(<android/binder_auto_utils.h>)
#include <android/binder_auto_utils.h>
#else
namespace ndk {

class ScopedAStatus {
public:
    explicit ScopedAStatus(AStatus* status = nullptr)
        : mStatus(status) {}

    ~ScopedAStatus() {
        if (mStatus)
            AStatus_delete(mStatus);
    }

    ScopedAStatus(ScopedAStatus&& other) noexcept
        : mStatus(other.mStatus) {
        other.mStatus = nullptr;
    }

    ScopedAStatus& operator=(ScopedAStatus&& other) noexcept {
        if (this != &other) {
            if (mStatus)
                AStatus_delete(mStatus);

            mStatus = other.mStatus;
            other.mStatus = nullptr;
        }

        return *this;
    }

    ScopedAStatus(const ScopedAStatus&) = delete;
    ScopedAStatus& operator=(const ScopedAStatus&) = delete;

    bool isOk() const {
        return mStatus &&
               AStatus_isOk(mStatus);
    }

    static ScopedAStatus fromStatus(
            binder_status_t status) {

        return ScopedAStatus(
            AStatus_fromStatus(status));
    }

private:
    AStatus* mStatus = nullptr;
};

} // namespace ndk
#endif
#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/input.h>
#include <mutex>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/ioctl.h>

#define LOG_TAG "RodinFpCompat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static constexpr const char* FP_DESCRIPTOR =
    "android.hardware.biometrics.fingerprint.IFingerprint";

static constexpr const char* SESSION_DESCRIPTOR =
    "android.hardware.biometrics.fingerprint.ISession";

static constexpr const char* XIAOMI_FP_EXT =
    "vendor.xiaomi.hardware.fingerprintextension.IXiaomiFingerprint/default";

static constexpr const char* TOUCH_FEATURE =
    "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";

static constexpr int32_t FOD_KEY = 0x152;

/* ColorOS private fingerprint protocol */
static constexpr transaction_code_t OPLUS_PRIVATE_TX = 1019;
static constexpr transaction_code_t OPLUS_CB_TX      = 1001;

static constexpr int32_t OPLUS_TOUCH_DOWN = 1201;
static constexpr int32_t OPLUS_TOUCH_UP   = 1202;

using DefineFn = AIBinder_Class* (*)(
    const char*,
    AIBinder_Class_onCreate,
    AIBinder_Class_onDestroy,
    AIBinder_Class_onTransact);

static DefineFn gRealDefine = nullptr;
static AIBinder_Class_onTransact gRealFpTransact = nullptr;
static AIBinder_Class_onTransact gRealSessionTransact = nullptr;

static std::once_flag gResolveOnce;
static std::once_flag gWorkerOnce;

static std::mutex gCallbackLock;
static AIBinder* gSessionCallback = nullptr;

static std::atomic<bool> gTouchListener{false};
static std::atomic<bool> gXiaomiFodArmed{false};
static std::atomic<bool> gFodSessionTerminal{false};
static std::atomic<bool> gFodSysfsFallback{false};
static std::atomic<bool> gPhysicalFodDown{false};

/*
 * A physical FOD press may arrive while ColorOS has already
 * stopped the previous listener but has not started the next
 * authentication operation yet.
 *
 * Never throw that physical edge away.  Remember it and replay
 * exactly one DOWN after the next real AUTH/ENROLL operation is
 * armed.
 */
static std::atomic<bool> gPendingFodDown{false};

enum : int32_t {
    FP_OP_NONE   = 0,
    FP_OP_ENROLL = 1,
    FP_OP_AUTH   = 2,
};

static std::atomic<int32_t> gFpOperation{FP_OP_NONE};

static std::atomic<int32_t> gLastEnrollRemaining{-1};

/*
 * ================================================================
 * NEES4 SHARED AUTHORIZATION
 *
 * The cryptographic verifier lives in /system/bin/nees_aodd.
 *
 * It verifies ONE signed manifest binding:
 *
 *   - canonical /system/build.prop
 *   - sensors.mt6899.so
 *   - /system/bin/nees_aodd
 *   - /vendor/lib64/librodin_fp_compat.so
 *
 * This library refuses to provide any Oplus compatibility behavior
 * unless the exact signed AOD + FOD pair is installed.
 * ================================================================
 */

static bool sharedAuthorizationReady() {
    char value[PROP_VALUE_MAX] = {};

    const int len =
        __system_property_get(
            "sys.nees4.authorized",
            value);

    return len > 0 &&
           strcmp(value, "1") == 0;
}

using CheckServiceFn = AIBinder* (*)(const char*);

static AIBinder* checkService(const char* name) {
    static CheckServiceFn fn =
        reinterpret_cast<CheckServiceFn>(
            dlsym(RTLD_DEFAULT, "AServiceManager_checkService"));

    if (!fn) {
        LOGE("AServiceManager_checkService unavailable");
        return nullptr;
    }

    return fn(name);
}


/* ================================================================
 * Binder helpers
 * ================================================================ */

static void resolveReal() {
    gRealDefine = reinterpret_cast<DefineFn>(
        dlsym(RTLD_NEXT, "AIBinder_Class_define"));
}

static void storeCallback(AIBinder* cb) {
    std::lock_guard<std::mutex> lock(gCallbackLock);

    if (gSessionCallback)
        AIBinder_decStrong(gSessionCallback);

    gSessionCallback = cb;

    LOGI("captured ColorOS ISessionCallback=%p", cb);
}

static AIBinder* retainCallback() {
    std::lock_guard<std::mutex> lock(gCallbackLock);

    if (!gSessionCallback)
        return nullptr;

    AIBinder_incStrong(gSessionCallback);
    return gSessionCallback;
}


/* ================================================================
 * ColorOS enrollment haptic
 *
 * Native equivalent of:
 *
 * FingerprintUtils.vibrateOnFingerEnrollSuccess(context)
 *
 * This ROM has no Luxun feature, therefore ColorOS selects
 * WaveformEffect effectType=2.
 *
 * IMPORTANT:
 * Called ONLY from real onEnrollmentProgress() acceptance.
 * ================================================================ */

static void* enrollHapticTokenCreate(void* args) {
    return args;
}

static void enrollHapticTokenDestroy(void*) {
}

static binder_status_t enrollHapticTokenTransact(
        AIBinder*,
        transaction_code_t,
        const AParcel*,
        AParcel*) {

    return STATUS_UNKNOWN_TRANSACTION;
}

static AIBinder* getEnrollHapticToken() {

    static std::once_flag once;
    static AIBinder* token = nullptr;

    std::call_once(
        once,
        [] {
            AIBinder_Class* clazz =
                AIBinder_Class_define(
                    "com.nees.rodin.EnrollHapticToken",
                    enrollHapticTokenCreate,
                    enrollHapticTokenDestroy,
                    enrollHapticTokenTransact);

            if (!clazz) {
                LOGE("enroll haptic token class create failed");
                return;
            }

            token =
                AIBinder_new(
                    clazz,
                    nullptr);

            LOGI("enroll haptic token=%p", token);
        });

    return token;
}


static AIBinder_Class* getLinearmotorInterfaceClass() {

    static AIBinder_Class* clazz =
        AIBinder_Class_define(
            "com.oplus.os.ILinearmotorVibratorService",
            enrollHapticTokenCreate,
            enrollHapticTokenDestroy,
            enrollHapticTokenTransact);

    return clazz;
}



/* ================================================================
 * Enrollment accepted-sample haptic
 *
 * Rodin uses Xiaomi vibrator HAL.
 * ColorOS linearmotor converts this enrollment haptic to OEM
 * effect 407, which this Xiaomi vibrator stack does not physically
 * reproduce.
 *
 * Use the real standard vibrator HAL instead:
 *
 *   android.hardware.vibrator.IVibrator/default
 *   perform(TICK, MEDIUM, nullptr)
 *
 * Still called ONLY for genuine accepted enrollment progress.
 * ================================================================ */

static AIBinder_Class* getXiaomiVibratorClass() {

    static AIBinder_Class* clazz =
        AIBinder_Class_define(
            "android.hardware.vibrator.IVibrator",
            enrollHapticTokenCreate,
            enrollHapticTokenDestroy,
            enrollHapticTokenTransact);

    return clazz;
}


static bool playXiaomiEnrollHaptic() {

    static constexpr const char* SERVICE =
        "android.hardware.vibrator.IVibrator/default";

    /*
     * Stable AIDL:
     *
     * getCapabilities = 1
     * off             = 2
     * on              = 3
     * perform         = 4
     */
    static constexpr transaction_code_t TX_PERFORM = 4;

    /*
     * android.hardware.vibrator.Effect::TICK = 2
     * EffectStrength::MEDIUM = 1
     */
    static constexpr int32_t EFFECT_TICK = 2;
    static constexpr int8_t STRENGTH_MEDIUM = 1;

    AIBinder* service =
        checkService(SERVICE);

    if (!service) {
        LOGE("Xiaomi vibrator HAL unavailable");
        return false;
    }

    AIBinder_Class* clazz =
        getXiaomiVibratorClass();

    if (!clazz ||
        !AIBinder_associateClass(service, clazz)) {

        LOGE("Xiaomi vibrator Binder association failed");
        AIBinder_decStrong(service);
        return false;
    }

    AParcel* in = nullptr;
    AParcel* out = nullptr;

    binder_status_t st =
        AIBinder_prepareTransaction(
            service,
            &in);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(
            in,
            EFFECT_TICK);

    if (st == STATUS_OK)
        st = AParcel_writeByte(
            in,
            STRENGTH_MEDIUM);

    /*
     * IVibratorCallback is optional for our one-shot haptic.
     */
    if (st == STATUS_OK)
        st = AParcel_writeStrongBinder(
            in,
            nullptr);

    if (st == STATUS_OK) {
        st = AIBinder_transact(
            service,
            TX_PERFORM,
            &in,
            &out,
            0);
    }

    bool ok =
        st == STATUS_OK;

    int32_t durationMs = -1;

    if (out) {

        AStatus* status = nullptr;

        const binder_status_t rst =
            AParcel_readStatusHeader(
                out,
                &status);

        if (rst != STATUS_OK || !status) {
            ok = false;
        } else {

            if (!AStatus_isOk(status))
                ok = false;

            AStatus_delete(status);

            if (ok)
                AParcel_readInt32(
                    out,
                    &durationMs);
        }

        AParcel_delete(out);
    }

    AIBinder_decStrong(service);

    LOGI(
        "ENROLL HAPTIC Xiaomi TICK medium status=%d duration=%d",
        ok,
        durationMs);

    return ok;
}



/* ================================================================
 * Xiaomi TouchFeature
 *
 * Equivalent to:
 *
 * service call vendor.xiaomi.hw.touchfeature.ITouchFeature/default \
 *     9 i32 0 i32 10 i32 1
 *
 * This lets Xiaomi choose the actual Goodix/FocalTech touch backend.
 * No direct touchscreen sysfs writes.
 * ================================================================ */

static bool setTouchFeature(
        int32_t touchId,
        int32_t mode,
        int32_t value) {

    AIBinder* service =
        checkService(TOUCH_FEATURE);

    if (!service) {
        LOGE("ITouchFeature unavailable");
        return false;
    }

    AParcel* in = nullptr;
    AParcel* out = nullptr;

    binder_status_t st =
        AIBinder_prepareTransaction(service, &in);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, touchId);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, mode);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, value);

    if (st == STATUS_OK) {
        st = AIBinder_transact(
            service,
            9,
            &in,
            &out,
            0);
    }

    if (out)
        AParcel_delete(out);

    AIBinder_decStrong(service);

    LOGI("TouchFeature id=%d mode=%d value=%d status=%d",
         touchId, mode, value, st);

    return st == STATUS_OK;
}

static bool setFodNode(bool enabled) {
    static constexpr const char* nodes[] = {
        "/sys/devices/platform/goodix_ts.0/fod_enable",
        "/sys/devices/virtual/touch/touch_dev/fod_enable",
        "/sys/class/touch/touch_dev/fod_enable",
    };

    for (const char* path : nodes) {
        int fd = open(path, O_WRONLY | O_CLOEXEC);

        if (fd < 0)
            continue;

        const char value = enabled ? '1' : '0';
        const ssize_t ret = write(fd, &value, 1);
        close(fd);

        if (ret == 1) {
            LOGI("FOD node %s -> %d", path, enabled ? 1 : 0);
            return true;
        }
    }

    LOGE("no writable Rodin FOD node");
    return false;
}

static bool armFodTouch() {
    if (setTouchFeature(0, 10, 1)) {
        gFodSysfsFallback.store(false);
        return true;
    }

    const bool ok = setFodNode(true);
    gFodSysfsFallback.store(ok);

    LOGI("TouchFeature failed; sysfs fallback=%d", ok);
    return ok;
}


/* ================================================================
 * Xiaomi fingerprint extension
 * ================================================================ */

static bool xiaomiConditionUpdate(
        int32_t index,
        int32_t value) {

    AIBinder* service =
        checkService(XIAOMI_FP_EXT);

    if (!service) {
        LOGE("IXiaomiFingerprint unavailable");
        return false;
    }

    AParcel* in = nullptr;
    AParcel* out = nullptr;

    binder_status_t st =
        AIBinder_prepareTransaction(service, &in);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, index);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, value);

    if (st == STATUS_OK) {
        st = AIBinder_transact(
            service,
            1,
            &in,
            &out,
            0);
    }

    if (out)
        AParcel_delete(out);

    AIBinder_decStrong(service);

    LOGI("conditionUpdate(%d,%d) status=%d",
         index, value, st);

    return st == STATUS_OK;
}


/* ================================================================
 * ColorOS fingerprint touch callbacks
 * ================================================================ */

static void sendColorOsTouch(int32_t cmd) {
    AIBinder* callback = retainCallback();

    if (!callback) {
        LOGE("touch %d: ISessionCallback unavailable", cmd);
        return;
    }

    AParcel* in = nullptr;
    AParcel* out = nullptr;

    binder_status_t st =
        AIBinder_prepareTransaction(callback, &in);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, cmd);

    /*
     * Exact ColorOS donor callback format:
     *
     * cmdId
     * result = 8
     * byte[8]
     */
    if (st == STATUS_OK)
        st = AParcel_writeInt32(in, 8);

    int8_t payload[8] = {};

    if (st == STATUS_OK)
        st = AParcel_writeByteArray(in, payload, 8);

    if (st == STATUS_OK) {
        st = AIBinder_transact(
            callback,
            OPLUS_CB_TX,
            &in,
            &out,
            FLAG_ONEWAY);
    }

    if (out)
        AParcel_delete(out);

    AIBinder_decStrong(callback);

    LOGI("ColorOS touch callback cmd=%d status=%d",
         cmd, st);
}



/* ================================================================
 * Physical Rodin FOD input
 *
 * Discover whichever touchscreen exports KEY 0x152.
 * Blocking read only: no polling loop while input is idle.
 * ================================================================ */

static bool keySupported(int fd) {
    constexpr size_t BPW =
        sizeof(unsigned long) * 8;

    constexpr size_t WORDS =
        (KEY_MAX / BPW) + 1;

    unsigned long bits[WORDS] = {};

    if (ioctl(
            fd,
            EVIOCGBIT(EV_KEY, sizeof(bits)),
            bits) < 0) {
        return false;
    }

    return bits[FOD_KEY / BPW] &
           (1UL << (FOD_KEY % BPW));
}

static int findFodInput() {
    for (int i = 0; i < 64; ++i) {
        char path[64];

        snprintf(
            path,
            sizeof(path),
            "/dev/input/event%d",
            i);

        int fd =
            open(path, O_RDONLY | O_CLOEXEC);

        if (fd < 0)
            continue;

        if (keySupported(fd)) {
            LOGI(
                "physical FOD input=%s key=%d",
                path,
                FOD_KEY);

            return fd;
        }

        close(fd);
    }

    return -1;
}

static void inputWorker() {
    for (;;) {
        int fd = findFodInput();

        if (fd < 0) {
            LOGE("FOD input device not found");

            std::this_thread::sleep_for(
                std::chrono::seconds(1));

            continue;
        }

        input_event ev{};

        while (read(fd, &ev, sizeof(ev)) ==
               sizeof(ev)) {

            if (ev.type != EV_KEY ||
                ev.code != FOD_KEY) {
                continue;
            }

            if (ev.value == 1) {

                /*
                 * ALWAYS track the real physical state.
                 *
                 * The old code discarded the entire event whenever
                 * gTouchListener=false.  That could leave
                 * gPhysicalFodDown permanently stuck true after a
                 * session closed while the finger was still held.
                 */
                gPhysicalFodDown.store(true);

                const bool listener =
                    gTouchListener.load();

                const int32_t operation =
                    gFpOperation.load();

                /*
                 * Finger arrived before the new ColorOS operation.
                 * Do not lose it and do not send it into a dead
                 * callback/session.  Replay it once AUTH/ENROLL is
                 * actually armed.
                 */
                if (!listener ||
                    operation == FP_OP_NONE) {

                    gPendingFodDown.store(true);

                    LOGI(
                        "PHYSICAL FOD DOWN pending "
                        "listener=%d op=%d",
                        listener,
                        operation);

                    continue;
                }

                /*
                 * Successful terminal contact is still genuinely
                 * being held.  Never relight/retrigger beneath it.
                 */
                if (gFodSessionTerminal.load()) {

                    LOGI(
                        "PHYSICAL FOD DOWN ignored: "
                        "terminal contact held");

                    continue;
                }

                gPendingFodDown.store(false);

                LOGI("PHYSICAL FOD DOWN");

                sendColorOsTouch(
                    OPLUS_TOUCH_DOWN);

            } else if (ev.value == 0) {

                /*
                 * ALWAYS consume physical UP, even if ColorOS has
                 * already stopped its listener.
                 */
                gPhysicalFodDown.store(false);
                gPendingFodDown.store(false);

                const bool listener =
                    gTouchListener.load();

                LOGI(
                    "PHYSICAL FOD UP listener=%d",
                    listener);

                /*
                 * No live ColorOS listener: this is cleanup only.
                 *
                 * Critically, physical state has already been reset,
                 * so the next authentication cannot be suppressed by
                 * a stale DOWN.
                 */
                if (!listener) {

                    if (gXiaomiFodArmed.exchange(false)) {

                        const bool state4 =
                            xiaomiConditionUpdate(
                                4,
                                0);

                        LOGI(
                            "FOD UP cleanup while listener off "
                            "state4=0 status=%d",
                            state4);
                    }

                    continue;
                }

                sendColorOsTouch(
                    OPLUS_TOUCH_UP);

                /*
                 * End this physical contact.
                 */
                if (gXiaomiFodArmed.exchange(false)) {

                    const bool state4 =
                        xiaomiConditionUpdate(4, 0);

                    LOGI(
                        "FOD CONTACT UP state4=0 status=%d",
                        state4);
                }

                /*
                 * Enrollment/authentication session is still active.
                 * Re-arm Xiaomi after mfp-daemon has processed UP so
                 * the next placement produces another real FOD event.
                 *
                 * No wake / doze / AOD / brightness operations here.
                 */
                if (gTouchListener.load() &&
                    !gFodSessionTerminal.load()) {

                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(20));

                    if (gTouchListener.load() &&
                        !gFodSessionTerminal.load()) {

                        const bool state4 =
                            xiaomiConditionUpdate(4, 1);

                        const bool state1 =
                            xiaomiConditionUpdate(1, 1);

                        const bool armed =
                            state4 && state1;

                        gXiaomiFodArmed.store(
                            armed);

                        LOGI(
                            "FOD SESSION REARM state4=%d state1=%d armed=%d",
                            state4,
                            state1,
                            armed);
                    }
                }
            }
        }

        close(fd);
    }
}


/* ================================================================
 * Capture ColorOS ISessionCallback from IFingerprint.createSession
 * ================================================================ */

static void captureSessionCallback(
        const AParcel* in,
        int32_t originalPos) {

    AParcel_setDataPosition(
        in,
        originalPos);

    int32_t sensorId = 0;
    int32_t userId = 0;
    AIBinder* callback = nullptr;

    binder_status_t s1 =
        AParcel_readInt32(
            in,
            &sensorId);

    binder_status_t s2 =
        AParcel_readInt32(
            in,
            &userId);

    binder_status_t s3 =
        AParcel_readStrongBinder(
            in,
            &callback);

    AParcel_setDataPosition(
        in,
        originalPos);

    if (s1 == STATUS_OK &&
        s2 == STATUS_OK &&
        s3 == STATUS_OK &&
        callback) {

        storeCallback(callback);

        LOGI(
            "createSession sensor=%d user=%d",
            sensorId,
            userId);
    }
}

static binder_status_t writeIntReply(
        AParcel* out,
        int32_t value) {

    if (!out)
        return STATUS_OK;

    AStatus* ok =
        AStatus_newOk();

    if (!ok)
        return STATUS_NO_MEMORY;

    binder_status_t st =
        AParcel_writeStatusHeader(
            out,
            ok);

    if (st == STATUS_OK)
        st = AParcel_writeInt32(
            out,
            value);

    AStatus_delete(ok);

    return st;
}

static binder_status_t writeOkReply(
        AParcel* out) {

    return writeIntReply(
        out,
        0);
}


/* ================================================================
 * Fingerprint transaction bridge
 *
 * NORMAL FOD ONLY.
 *
 * Absolutely no:
 *   brightness writes
 *   display ioctls
 *   doze state
 *   AOD handling
 *   panel sysfs
 *   power/wake handling
 * ================================================================ */


/* ===== RODIN OPLUS DISPLAY PANEL COMPAT =====
 *
 * Native replacement for the missing ColorOS display-panel HAL.
 *
 * IMPORTANT:
 *   - Advertise Local-HBM capability only.
 *   - Allow ColorOS highlight state machine to run.
 *   - Xiaomi mfp-daemon remains owner of actual LHBM.
 *   - NO brightness writes.
 *   - NO doze writes.
 *   - NO AOD control.
 *   - NO wake/power control.
 *   - NO panel power manipulation.
 */

static constexpr const char* OPLUS_PANEL_DESCRIPTOR =
    "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature";

static constexpr const char* OPLUS_PANEL_SERVICE =
    "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default";

static constexpr transaction_code_t OPLUS_PANEL_GET_FEATURE = 1;
static constexpr transaction_code_t OPLUS_PANEL_SET_FEATURE = 2;
static constexpr transaction_code_t OPLUS_PANEL_GET_INFO    = 3;

static constexpr transaction_code_t AIDL_GET_HASH =
    16777214;

static constexpr transaction_code_t AIDL_GET_VERSION =
    16777215;

/*
 * SystemUI KeyguardFeatureOption.getUdfpsType():
 *
 * feature 211
 *
 * bit 0x10 = Local-HBM
 *
 * Do NOT advertise the other acceleration/capability bits yet.
 */
static constexpr int32_t OPLUS_FEATURE_UDFPS_TYPE = 211;
static constexpr int32_t OPLUS_UDFPS_LOCAL_HBM   = 0x10;
static constexpr int32_t OPLUS_UDFPS_LOCAL_HBM_ACCEL = 0x400;
static constexpr int32_t OPLUS_UDFPS_TYPE_VALUE =
    OPLUS_UDFPS_LOCAL_HBM | OPLUS_UDFPS_LOCAL_HBM_ACCEL;

/*
 * OnScreenHighLightControl.hbmControl()
 *
 * feature 22 = ColorOS highlight/HBM state.
 *
 * We ACK this only. Xiaomi mfp-daemon already owns the real
 * Rodin local-HBM operation.
 */
static constexpr int32_t OPLUS_FEATURE_HBM_CONTROL = 22;

/*
 * ColorOS AOD smooth-transition capability.
 *
 * 0x1 = direct AOD transition / no OFF-before-DOZE
 * 0x2 = smooth transition supported
 *
 * READ-ONLY compatibility advertisement for first test.
 * No panel operation is performed here.
 */
static constexpr int32_t OPLUS_FEATURE_AOD_SMOOTH = 217;
static constexpr int32_t OPLUS_AOD_SMOOTH_CAPS    = 0xF;

static std::once_flag gOplusPanelRegisterOnce;
static AIBinder_Class* gOplusPanelClass = nullptr;
static AIBinder* gOplusPanelBinder = nullptr;

/*
 * These binder-manager APIs exist on Android itself, but this NDK
 * sysroot does not export them in its link stub. Resolve them from
 * the runtime libbinder_ndk instead of creating hard link deps.
 */
using CheckServiceFn =
    AIBinder* (*)(const char*);

using AddServiceFn =
    binder_status_t (*)(AIBinder*, const char*);

using MarkVintfFn =
    void (*)(AIBinder*);

static CheckServiceFn gCheckService = nullptr;
static AddServiceFn   gAddService = nullptr;
static MarkVintfFn    gMarkVintf = nullptr;
static void* gBinderNdkHandle = nullptr;

static bool resolveBinderManagerApis() {
    if (gCheckService && gAddService)
        return true;

    if (!gBinderNdkHandle) {
        gBinderNdkHandle =
            dlopen("libbinder_ndk.so",
                   RTLD_NOW | RTLD_LOCAL);
    }

    void* scope =
        gBinderNdkHandle ?
            gBinderNdkHandle :
            RTLD_DEFAULT;

    gCheckService =
        reinterpret_cast<CheckServiceFn>(
            dlsym(scope,
                  "AServiceManager_checkService"));

    gAddService =
        reinterpret_cast<AddServiceFn>(
            dlsym(scope,
                  "AServiceManager_addService"));

    gMarkVintf =
        reinterpret_cast<MarkVintfFn>(
            dlsym(scope,
                  "AIBinder_markVintfStability"));

    LOGI("binder-manager runtime APIs check=%p add=%p vintf=%p",
         reinterpret_cast<void*>(gCheckService),
         reinterpret_cast<void*>(gAddService),
         reinterpret_cast<void*>(gMarkVintf));

    return gCheckService && gAddService;
}


static void* oplusPanelOnCreate(void* args) {
    return args;
}

static void oplusPanelOnDestroy(void*) {
}

static binder_status_t panelWriteOk(AParcel* out) {
    AStatus* status = AStatus_newOk();

    if (!status)
        return STATUS_NO_MEMORY;

    binder_status_t rc =
        AParcel_writeStatusHeader(out, status);

    AStatus_delete(status);
    return rc;
}

static bool panelIntArrayAllocator(
        void* opaque,
        int32_t length,
        int32_t** outBuffer) {

    if (!opaque || !outBuffer || length < 0)
        return false;

    auto* values =
        static_cast<std::vector<int32_t>*>(opaque);

    values->resize(
        static_cast<size_t>(length));

    if (length == 0) {
        *outBuffer = nullptr;
        return true;
    }

    *outBuffer = values->data();
    return true;
}

static binder_status_t oplusPanelOnTransact(
        AIBinder*,
        transaction_code_t code,
        const AParcel* in,
        AParcel* out) {

    binder_status_t st = STATUS_OK;

    switch (code) {

    case OPLUS_PANEL_GET_FEATURE: {
        int32_t feature = 0;
        int32_t requestedLength = 0;

        st = AParcel_readInt32(in, &feature);
        if (st != STATUS_OK)
            return st;

        st = AParcel_readInt32(in, &requestedLength);
        if (st != STATUS_OK)
            return st;

        /*
         * ColorOS UDFPS capability query.
         *
         * Always return exactly one integer for feature 211.
         * Do not trust the observed request length here.
         */
        if (feature == OPLUS_FEATURE_UDFPS_TYPE) {
            const int32_t value =
                OPLUS_UDFPS_TYPE_VALUE;

            st = panelWriteOk(out);
            if (st != STATUS_OK)
                return st;

            st = AParcel_writeInt32(out, 0);
            if (st != STATUS_OK)
                return st;

            st = AParcel_writeInt32Array(
                out,
                &value,
                1);

            LOGI("OplusPanel GET feature=211 -> [0x410] "
                 "requestLen=%d status=%d",
                 requestedLength,
                 st);

            return st;
        }

        /*
         * ColorOS smooth AOD capability query.
         *
         * Report capability only:
         *   bit0 = no OFF-before-DOZE
         *   bit1 = smooth transition support
         *
         * Do NOT touch Xiaomi hardware here.
         */
        if (feature == OPLUS_FEATURE_AOD_SMOOTH) {
            const int32_t value =
                OPLUS_AOD_SMOOTH_CAPS;

            st = panelWriteOk(out);
            if (st != STATUS_OK)
                return st;

            st = AParcel_writeInt32(out, 0);
            if (st != STATUS_OK)
                return st;

            st = AParcel_writeInt32Array(
                out,
                &value,
                1);

            LOGI("OplusPanel GET feature=217 -> [0xF] "
                 "requestLen=%d status=%d",
                 requestedLength,
                 st);

            return st;
        }

        /*
         * Unsupported ColorOS panel capabilities:
         *
         * Return a VALID AIDL transaction with result=-1 instead of
         * STATUS_UNKNOWN_TRANSACTION.
         *
         * This prevents RemoteException/"method unimplemented"
         * from disturbing ColorOS AOD state machines.
         */
        size_t outLength = 0;

        if (requestedLength > 0 &&
            requestedLength <= 32) {
            outLength =
                static_cast<size_t>(
                    requestedLength);
        }

        std::vector<int32_t> values(
            outLength,
            0);

        st = panelWriteOk(out);
        if (st != STATUS_OK)
            return st;

        st = AParcel_writeInt32(out, -1);
        if (st != STATUS_OK)
            return st;

        st = AParcel_writeInt32Array(
            out,
            values.empty() ? nullptr : values.data(),
            values.size());

        LOGI("OplusPanel GET unsupported feature=%d "
             "requestLen=%d -> result=-1 status=%d",
             feature,
             requestedLength,
             st);

        return st;
    }

    case OPLUS_PANEL_SET_FEATURE: {
        int32_t feature = 0;

        st = AParcel_readInt32(in, &feature);
        if (st != STATUS_OK)
            return st;

        std::vector<int32_t> values;

        st = AParcel_readInt32Array(
            in,
            &values,
            panelIntArrayAllocator);

        if (st != STATUS_OK)
            return st;

        const int32_t value =
            values.empty() ? -1 : values[0];

        st = panelWriteOk(out);
        if (st != STATUS_OK)
            return st;

        /*
         * Feature 22 is ColorOS UDFPS highlight/HBM control.
         * ACK only. Xiaomi mfp-daemon remains owner of real LHBM.
         */
        if (feature == OPLUS_FEATURE_HBM_CONTROL ||
            feature == 28) {

            /*
             * During real authentication ColorOS sends feature 22 = 0
             * after authentication succeeds, even if the finger is
             * still physically held.
             *
             * Use that terminal edge only for Xiaomi illumination OFF.
             *
             * NO screen wake.
             * NO doze operation.
             * NO brightness write.
             * NO AOD manipulation.
             */
            if (feature == OPLUS_FEATURE_HBM_CONTROL &&
                value == 0 &&
                gFpOperation.load() == FP_OP_AUTH &&
                gPhysicalFodDown.load()) {

                gFodSessionTerminal.store(true);
                gXiaomiFodArmed.store(false);

                const bool off =
                    xiaomiConditionUpdate(
                        4,
                        0);

                LOGI(
                    "AUTH terminal illumination OFF status=%d",
                    off);
            }

            LOGI("OplusPanel FOD display feature=%d value=%d "
                 "ACK only",
                 feature,
                 value);

            return AParcel_writeInt32(out, 0);
        }

        /*
         * AOD/LTPO/panel features stay untouched.
         * Return "unsupported" as the method result WITHOUT throwing
         * a Binder exception.
         */
        LOGI("OplusPanel SET passthrough-unsupported "
             "feature=%d value=%d -> result=-1",
             feature,
             value);

        return AParcel_writeInt32(out, -1);
    }

    case OPLUS_PANEL_GET_INFO:
        /*
         * Leave info requests alone for now.
         * They are not part of the UDFPS capability path.
         */
        return STATUS_UNKNOWN_TRANSACTION;

    case AIDL_GET_VERSION: {
        st = panelWriteOk(out);
        if (st != STATUS_OK)
            return st;

        return AParcel_writeInt32(out, 1);
    }

    case AIDL_GET_HASH: {
        static constexpr const char* hash =
            "rodin-oplus-panel-compat";

        st = panelWriteOk(out);
        if (st != STATUS_OK)
            return st;

        return AParcel_writeString(
            out,
            hash,
            strlen(hash));
    }

    default:
        return STATUS_UNKNOWN_TRANSACTION;
    }
}

static void registerOplusPanelCompat() {
    std::call_once(
        gOplusPanelRegisterOnce,
        []() {

        std::thread([]() {

            if (!resolveBinderManagerApis()) {
                LOGE("binder-manager runtime APIs unavailable");
                return;
            }

            /*
             * mfp-daemon starts early enough that this service will
             * exist before SystemUI lazily evaluates isLocalHBM.
             */
            for (int attempt = 0;
                 attempt < 20;
                 ++attempt) {

                AIBinder* existing =
                    gCheckService(
                        OPLUS_PANEL_SERVICE);

                if (existing) {
                    LOGI("Oplus display-panel service already exists");
                    AIBinder_decStrong(existing);
                    return;
                }

                if (!gOplusPanelClass) {
                    gOplusPanelClass =
                        AIBinder_Class_define(
                            OPLUS_PANEL_DESCRIPTOR,
                            oplusPanelOnCreate,
                            oplusPanelOnDestroy,
                            oplusPanelOnTransact);
                }

                if (!gOplusPanelClass) {
                    LOGE("OplusPanel class define failed");
                    return;
                }

                AIBinder* binder =
                    AIBinder_new(
                        gOplusPanelClass,
                        nullptr);

                if (!binder) {
                    LOGE("OplusPanel binder creation failed");
                    return;
                }

                /*
                 * Oplus interface is a stable vendor AIDL interface.
                 */

                binder_status_t rc =
                    gAddService(
                        binder,
                        OPLUS_PANEL_SERVICE);

                if (rc == STATUS_OK) {
                    gOplusPanelBinder = binder;

                    LOGI("OplusPanel registered: %s",
                         OPLUS_PANEL_SERVICE);

                    return;
                }

                AIBinder_decStrong(binder);

                LOGE("OplusPanel registration attempt=%d status=%d",
                     attempt + 1,
                     rc);

                usleep(250000);
            }

            LOGE("OplusPanel registration FAILED");
        }).detach();
    });
}

__attribute__((constructor))
static void rodinOplusPanelCompatInit() {

    std::thread([]() {

        /*
         * mfp-daemon starts before the late-start AOD helper on
         * some boots.  Never block mfp-daemon startup.
         *
         * Wait for the independently verified NEES4 runtime token.
         */
        for (int attempt = 0;
             attempt < 300;
             ++attempt) {

            if (sharedAuthorizationReady()) {
                LOGI(
                    "NEES4 runtime authorization OK");

                registerOplusPanelCompat();
                return;
            }

            usleep(100000);
        }

        LOGE(
            "NEES4 runtime authorization timeout; "
            "OplusPanel disabled");
    }).detach();
}

/* ===== END RODIN OPLUS DISPLAY PANEL COMPAT ===== */

static binder_status_t fpCompatOnTransact(
        AIBinder* binder,
        transaction_code_t code,
        const AParcel* in,
        AParcel* out) {

    if (!sharedAuthorizationReady()) {
        return gRealFpTransact(
            binder,
            code,
            in,
            out);
    }

    const int32_t originalPos =
        AParcel_getDataPosition(in);

    /*
     * Standard IFingerprint.createSession().
     */
    if (code == 2) {

        binder_status_t st =
            gRealFpTransact(
                binder,
                code,
                in,
                out);

        captureSessionCallback(
            in,
            originalPos);

        return st;
    }

    /*
     * ColorOS private sendFingerprintCmd().
     */
    if (code == OPLUS_PRIVATE_TX) {

        int32_t cmd = 0;
        int32_t len = 0;
        int32_t parcelArrayLen = 0;

        AParcel_readInt32(
            in,
            &cmd);

        AParcel_readInt32(
            in,
            &len);

        /*
         * Parcel.writeByteArray() writes its own array length.
         * This is NOT the command value.
         */
        AParcel_readInt32(
            in,
            &parcelArrayLen);

        AParcel_setDataPosition(
            in,
            originalPos);

        LOGI(
            "ColorOS tx1019 cmd=%d len=%d arrayLen=%d",
            cmd,
            len,
            parcelArrayLen);

        /*
         * OplusFingerprintTouchEventClient.startHalOperation().
         *
         * This is the real ColorOS request for the fingerprint
         * touch-event monitor.
         */
        if (cmd == 1008 &&
            len == 1 &&
            parcelArrayLen == 1) {

            gTouchListener.store(true);

            std::call_once(
                gWorkerOnce,
                [] {
                    std::thread(
                        inputWorker).detach();
                });

            /*
             * Never re-light beneath a successful finger that is
             * still physically held.
             */
            if (!(gFodSessionTerminal.load() &&
                  gPhysicalFodDown.load())) {

                gFodSessionTerminal.store(false);

                const bool state4 =
                    xiaomiConditionUpdate(
                        4,
                        1);

                const bool state1 =
                    xiaomiConditionUpdate(
                        1,
                        1);

                const bool armed =
                    state4 &&
                    state1;

                gXiaomiFodArmed.store(
                    armed);

                LOGI(
                    "ColorOS TOUCH MONITOR START "
                    "state4=%d state1=%d armed=%d",
                    state4,
                    state1,
                    armed);

            } else {

                LOGI(
                    "ColorOS TOUCH MONITOR START "
                    "suppressed: terminal contact held");
            }

            return writeOkReply(out);
        }

        /*
         * FingerprintServiceProviderExtImpl
         *     .getEnrollmentTotalTimes()
         *
         * -1 is intentional.
         *
         * ColorOS Settings treats -1 as "HAL did not provide a
         * fixed total" and derives the total from the first real
         * onEnrollmentProgress remaining value.
         *
         * Goodix and Jiiov can therefore report their own totals.
         */
        if (cmd == 1010 &&
            len == 1 &&
            parcelArrayLen == 1) {

            LOGI(
                "ColorOS getEnrollmentTotalTimes -> -1");

            return writeIntReply(
                out,
                -1);
        }
    }

    return gRealFpTransact(
        binder,
        code,
        in,
        out);
}


static void startFingerprintOperation(
        int32_t operation,
        const char* name) {

    gFpOperation.store(
        operation);

    if (operation == FP_OP_ENROLL)
        gLastEnrollRemaining.store(-1);

    gTouchListener.store(
        true);

    std::call_once(
        gWorkerOnce,
        [] {
            std::thread(
                inputWorker).detach();
        });

    /*
     * PrismaAuth-style post-auth release guard:
     *
     * If the previous successful finger is physically still DOWN,
     * a newly-created auth operation must not relight LHBM.
     */
    /*
     * Consume a DOWN that arrived between the old session stopping
     * and this new operation becoming ready.
     */
    const bool pendingDown =
        gPendingFodDown.exchange(false) &&
        gPhysicalFodDown.load();

    /*
     * Preserve the existing post-auth protection for the SAME
     * successful finger still being held.
     *
     * A pendingDown, however, is a new early contact and must be
     * allowed to start the new operation.
     */
    if (gFodSessionTerminal.load() &&
        gPhysicalFodDown.load() &&
        !pendingDown) {

        LOGI(
            "%s start suppressed: terminal contact held",
            name);

        return;
    }

    gFodSessionTerminal.store(
        false);

    const bool state4 =
        xiaomiConditionUpdate(
            4,
            1);

    const bool state1 =
        xiaomiConditionUpdate(
            1,
            1);

    const bool armed =
        state4 &&
        state1;

    gXiaomiFodArmed.store(
        armed);

    LOGI(
        "%s start state4=%d state1=%d armed=%d",
        name,
        state4,
        state1,
        armed);

    /*
     * If the finger landed before AUTH/ENROLL existed, the physical
     * input edge has already happened and may not occur again while
     * the finger remains held.
     *
     * After Xiaomi is armed, replay exactly one ColorOS DOWN.
     */
    if (pendingDown &&
        armed) {

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));

        if (gPhysicalFodDown.load() &&
            gFpOperation.load() == operation &&
            !gFodSessionTerminal.load()) {

            LOGI(
                "%s replay pending physical FOD DOWN",
                name);

            sendColorOsTouch(
                OPLUS_TOUCH_DOWN);
        }
    }
}


static binder_status_t sessionCompatOnTransact(
        AIBinder* binder,
        transaction_code_t code,
        const AParcel* in,
        AParcel* out) {

    if (!gRealSessionTransact)
        return STATUS_UNKNOWN_TRANSACTION;

    binder_status_t st =
        gRealSessionTransact(
            binder,
            code,
            in,
            out);

    if (st != STATUS_OK)
        return st;

    if (!sharedAuthorizationReady())
        return st;

    /*
     * Stable AIDL ISession transaction order used by the
     * fingerprint V4 interface in this vendor stack.
     */
    if (code == 3 || code == 16) {

        startFingerprintOperation(
            FP_OP_ENROLL,
            "ENROLL");

    } else if (code == 4 || code == 15) {

        startFingerprintOperation(
            FP_OP_AUTH,
            "AUTH");

    } else if (code == 11) {

        gFpOperation.store(
            FP_OP_NONE);

        gTouchListener.store(
            false);

        gFodSessionTerminal.store(
            true);

        gXiaomiFodArmed.store(
            false);

        const bool off =
            xiaomiConditionUpdate(
                4,
                0);

        LOGI(
            "SESSION CLOSE state4=0 status=%d",
            off);
    }

    return st;
}


/* ================================================================
 * libbinder_ndk interposition
 * ================================================================ */

extern "C"
__attribute__((visibility("default")))
AIBinder_Class* AIBinder_Class_define(
        const char* descriptor,
        AIBinder_Class_onCreate onCreate,
        AIBinder_Class_onDestroy onDestroy,
        AIBinder_Class_onTransact onTransact) {

    std::call_once(
        gResolveOnce,
        resolveReal);

    if (!gRealDefine)
        return nullptr;

    if (descriptor &&
        strcmp(
            descriptor,
            FP_DESCRIPTOR) == 0) {

        gRealFpTransact = onTransact;

        LOGI(
            "hooking %s NORMAL-FOD ONLY",
            descriptor);

        return gRealDefine(
            descriptor,
            onCreate,
            onDestroy,
            fpCompatOnTransact);
    }

    if (descriptor &&
        strcmp(
            descriptor,
            SESSION_DESCRIPTOR) == 0) {

        gRealSessionTransact =
            onTransact;

        LOGI(
            "hooking %s lifecycle only",
            descriptor);

        return gRealDefine(
            descriptor,
            onCreate,
            onDestroy,
            sessionCompatOnTransact);
    }

    return gRealDefine(
        descriptor,
        onCreate,
        onDestroy,
        onTransact);
}






/* ================================================================
 * Accepted enrollment callback hook
 *
 * TARGETED C++ symbol only.
 * No global Binder interception.
 * ================================================================ */

using RealBpEnrollProgress =
    ::ndk::ScopedAStatus (*)(
        void*,
        int32_t,
        int32_t);

__attribute__((visibility("default")))
::ndk::ScopedAStatus
rodinBpEnrollProgress(
        void* self,
        int32_t enrollmentId,
        int32_t remaining)
    __asm__(
        "_ZN4aidl7android8hardware10biometrics11fingerprint"
        "17BpSessionCallback20onEnrollmentProgressEii");


::ndk::ScopedAStatus
rodinBpEnrollProgress(
        void* self,
        int32_t enrollmentId,
        int32_t remaining) {

    static RealBpEnrollProgress real =
        reinterpret_cast<RealBpEnrollProgress>(
            dlsym(
                RTLD_NEXT,
                "_ZN4aidl7android8hardware10biometrics11fingerprint"
                "17BpSessionCallback20onEnrollmentProgressEii"));

    if (!real) {

        LOGE(
            "real BpSessionCallback::onEnrollmentProgress "
            "not found");

        return ::ndk::ScopedAStatus::fromStatus(
            STATUS_UNKNOWN_TRANSACTION);
    }

    /*
     * ALWAYS forward the genuine HAL callback first.
     *
     * Settings progress behavior remains completely stock.
     */
    ::ndk::ScopedAStatus status =
        real(
            self,
            enrollmentId,
            remaining);

    if (!status.isOk())
        return status;

    if (!sharedAuthorizationReady())
        return status;

    if (gFpOperation.load() != FP_OP_ENROLL)
        return status;

    const int32_t previous =
        gLastEnrollRemaining.exchange(
            remaining);

    /*
     * Every decrease in remaining count is a REAL accepted
     * enrollment sample.
     *
     * Never vibrate for:
     *
     * finger down
     * finger up
     * dirty image
     * rejected sample
     * duplicate remaining callback
     */
    const bool accepted =
        previous < 0 ||
        remaining < previous;

    if (accepted) {

        LOGI(
            "ENROLL ACCEPTED id=%d remaining=%d previous=%d",
            enrollmentId,
            remaining,
            previous);

        playXiaomiEnrollHaptic();
    }

    /*
     * Final accepted sample.
     *
     * Stop illumination and mark terminal so the input worker
     * cannot re-arm beneath the completed finger.
     */
    if (remaining == 0) {

        gFodSessionTerminal.store(true);
        gXiaomiFodArmed.store(false);

        const bool off =
            xiaomiConditionUpdate(
                4,
                0);

        LOGI(
            "ENROLL COMPLETE terminal state4=0 status=%d",
            off);
    }

    return status;
}
