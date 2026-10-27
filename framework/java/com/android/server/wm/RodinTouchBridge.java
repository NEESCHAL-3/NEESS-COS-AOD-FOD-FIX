package com.android.server.wm;

import android.os.IBinder;
import android.os.Parcel;
import android.util.Log;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;

/** Routes the existing ColorOS double-tap node request to Rodin's touch HAL. */
public final class RodinTouchBridge {
    public static final int UNHANDLED = Integer.MIN_VALUE;
    private static final String SERVICE = "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";
    private static final String DESCRIPTOR = "vendor.xiaomi.hw.touchfeature.ITouchFeature";
    private static final String PREFERENCE = "oplus_customize_gesture_double_touch";
    private static Context sContext;
    private RodinTouchBridge() {}

    public static synchronized void initialize(Context context) {
        if (context == null || sContext != null) return;
        sContext = context;
        Handler handler = new Handler(Looper.getMainLooper());
        ContentObserver observer = new ContentObserver(handler) {
            @Override public void onChange(boolean selfChange) { applySelectedMode(); }
        };
        context.getContentResolver().registerContentObserver(
                Settings.Secure.getUriFor(PREFERENCE), false, observer);
        IntentFilter filter = new IntentFilter(Intent.ACTION_BOOT_COMPLETED);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context context, Intent intent) { applySelectedMode(); }
        }, filter, Context.RECEIVER_NOT_EXPORTED);
        handler.post(RodinTouchBridge::applySelectedMode);
        Log.i("RodinTouchBridge", "Watching the ColorOS double-tap preference");
        RodinAodTapBridge.initialize(context);
    }

    private static boolean selected() {
        return sContext != null && Settings.Secure.getInt(
                sContext.getContentResolver(), PREFERENCE, 0) != 0;
    }

    private static void applySelectedMode() {
        boolean enabled = selected();
        try {
            // Preserve the original ColorOS proximity lock when applying a preference.
            Class<?> cls = Class.forName("com.android.server.input.ColorTouchNodeController");
            Object controller = cls.getMethod("getInstance").invoke(null);
            if (enabled && (Boolean) cls.getMethod("skipNodeFileWrite", int.class, String.class)
                    .invoke(controller, 1, "1")) enabled = false;
            // Keep Android's native preference aligned; the old stale value must not
            // enable the driver after the ColorOS toggle has been switched off.
            int desired = selected() ? 1 : 0;
            if (Settings.Secure.getInt(sContext.getContentResolver(), "double_tap_to_wake", 0) != desired)
                Settings.Secure.putInt(sContext.getContentResolver(), "double_tap_to_wake", desired);
            sendMode(enabled ? 1 : 0);
        } catch (Exception error) {
            Log.e("RodinTouchBridge", "Unable to apply the double-tap preference", error);
        }
    }

    public static int writeNodeFile(int deviceId, int nodeFlag, String info) {
        if (deviceId != 0 || nodeFlag != 1) return UNHANDLED;
        if (!"0".equals(info) && !"1".equals(info)) return -1;
        return sendMode("1".equals(info) && selected() ? 1 : 0);
    }

    private static int sendMode(int enabled) { return sendNativeMode(14, enabled); }

    public static int sendNativeMode(int mode, int enabled) {
        Parcel request = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        try {
            IBinder service = (IBinder) Class.forName("android.os.ServiceManager")
                    .getMethod("checkService", String.class).invoke(null, SERVICE);
            if (service == null || !DESCRIPTOR.equals(service.getInterfaceDescriptor())) {
                Log.e("RodinTouchBridge", "Xiaomi touch HAL is unavailable");
                return -1;
            }
            request.writeInterfaceToken(DESCRIPTOR);
            request.writeInt(0); // Primary touch controller.
            request.writeInt(mode); // Xiaomi native touch mode.
            request.writeInt(enabled);
            if (!service.transact(9, request, reply, 0)) return -1;
            reply.readException();
            int result = reply.readInt();
            Log.i("RodinTouchBridge", "Touch mode=" + mode + " enabled=" + enabled + " HAL result=" + result);
            return result;
        } catch (Exception error) {
            Log.e("RodinTouchBridge", "Double-tap HAL request failed", error);
            return -1;
        } finally {
            request.recycle();
            reply.recycle();
        }
    }
}
