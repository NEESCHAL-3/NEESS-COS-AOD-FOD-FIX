package com.android.server.wm;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.PowerManager;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Calendar;

/** Adapts the controller's positionless single-tap wake key to native ColorOS AOD. */
public final class RodinAodTapBridge {
    private static final String TAG = "RodinAodTapBridge";
    private static final String POLICY = "com.android.server.oplus.input.exinputservice.screenoffgesture.OplusScreenOffGesturePolicy";
    private static volatile Context sContext;
    private static Handler sHandler;
    private static PowerManager sPower;
    private static SensorManager sSensors;
    private static Sensor sProximity, sNonUi;
    private static boolean sNonUiListening, sNonUiKnown, sNonUiBlocked;
    private static PowerManager.WakeLock sWakeLock;
    private static boolean sListening, sKnown, sFar;
    private static volatile boolean sArmed;
    private static long sLastTap;
    private RodinAodTapBridge() {}

    public static synchronized void initialize(Context context) {
        if (context == null || sContext != null) return;
        try {
            Class<?> route=Class.forName("com.android.server.policy.PhoneWindowManagerExtImpl");
            if (route.getField("RODIN_TAP_AOD_ROUTE_VERSION").getInt(null)!=1) return;
        } catch (ReflectiveOperationException absent) {
            Log.i(TAG,"Single-tap input route is not installed; detector remains unchanged");
            return;
        }
        if (context.getSystemService(PowerManager.class)==null || context.getSystemService(SensorManager.class)==null) {
            Log.w(TAG,"Required system services unavailable; single-tap bridge not started");
            return;
        }
        sContext = context;
        HandlerThread thread = new HandlerThread("RodinAodTap");
        thread.start();
        sHandler = new Handler(thread.getLooper());
        sPower = context.getSystemService(PowerManager.class);
        sSensors = context.getSystemService(SensorManager.class);
        sProximity = sSensors == null ? null : sSensors.getDefaultSensor(Sensor.TYPE_PROXIMITY, true);
        if (sProximity == null && sSensors != null) sProximity = sSensors.getDefaultSensor(Sensor.TYPE_PROXIMITY);
        sNonUi=sSensors.getDefaultSensor(33171027,true);
        if (sNonUi==null) sNonUi=sSensors.getDefaultSensor(33171027);
        sWakeLock = sPower.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Rodin:AodTap");
        sWakeLock.setReferenceCounted(false);
        ContentObserver observer = new ContentObserver(sHandler) {
            @Override public void onChange(boolean selfChange) { refresh(); }
        };
        for (String key : new String[]{"Setting_AodEnable", "Setting_AodUserSetTime",
                "Setting_AodSetTimeBeginHour", "Setting_AodSetTimeBeginMin",
                "Setting_AodSetTimeEndHour", "Setting_AodSetTimeEndMin", "rodin_tap_to_aod_enabled"})
            context.getContentResolver().registerContentObserver(Settings.Secure.getUriFor(key), false, observer);
        IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_BOOT_COMPLETED);
        filter.addAction(Intent.ACTION_TIME_CHANGED);
        filter.addAction(Intent.ACTION_TIMEZONE_CHANGED);
        context.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) { refresh(); }
        }, filter, Context.RECEIVER_NOT_EXPORTED);
        sHandler.post(() -> {
            RodinTouchBridge.sendNativeMode(11,0);
            updateState();
        });
        Log.i(TAG, "Native single-tap bridge initialized");
    }

    public static void refresh() {
        Handler handler = sHandler;
        if (handler != null) handler.post(RodinAodTapBridge::updateState);
    }

    private static int setting(String name, int fallback) {
        return Settings.Secure.getInt(sContext.getContentResolver(), name, fallback);
    }

    private static boolean scheduleAllowsNow() {
        if (setting("Setting_AodUserSetTime", 0) != 1) return true;
        int sh=setting("Setting_AodSetTimeBeginHour", -1), sm=setting("Setting_AodSetTimeBeginMin", -1);
        int eh=setting("Setting_AodSetTimeEndHour", -1), em=setting("Setting_AodSetTimeEndMin", -1);
        if (sh<0 || sh>23 || eh<0 || eh>23 || sm<0 || sm>59 || em<0 || em>59) return false;
        int start=sh*60+sm, end=eh*60+em;
        Calendar now=Calendar.getInstance();
        int minute=now.get(Calendar.HOUR_OF_DAY)*60+now.get(Calendar.MINUTE);
        return start<end ? minute>=start && minute<end : start>end && (minute>=start || minute<end);
    }

    private static Object policy() throws Exception {
        return Class.forName(POLICY).getMethod("getInstance").invoke(null);
    }

    private static boolean requested() {
        if (sContext == null || sPower.isInteractive() || setting("rodin_tap_to_aod_enabled", 1) != 1) return false;
        try {
            Object policy=policy();
            return (Boolean) policy.getClass().getMethod("getGestureState", int.class).invoke(policy,16);
        } catch (Exception error) {
            Log.w(TAG, "Native AOD gesture policy unavailable", error);
            return false;
        }
    }

    private static final SensorEventListener PROXIMITY = new SensorEventListener() {
        @Override public void onAccuracyChanged(Sensor sensor, int accuracy) {}
        @Override public void onSensorChanged(SensorEvent event) {
            if (!sListening || event.values.length == 0) return;
            if (event.sensor.getType()==33171027) {
                float state=event.values[0];
                sNonUiKnown=Float.isFinite(state) && (state==0 || state==1 || state==2);
                // Stock MIUI treats NonUI 1 and 2 as NEAR; 0 releases pocket mode.
                sNonUiBlocked=!sNonUiKnown || state!=0;
                Log.i(TAG,"Native NonUI pocket blocked="+sNonUiBlocked);
            } else if (event.sensor.getType()==Sensor.TYPE_PROXIMITY) {
                float distance=event.values[0];
                float threshold=Math.min(sProximity.getMaximumRange(),5.0f);
                sKnown=Float.isFinite(distance) && distance>=0 && threshold>0;
                sFar=sKnown && distance>=threshold;
            }
            applyMode(requested() && uncovered());
        }
    };

    private static boolean uncovered() {
        return sKnown && sFar && (!sNonUiListening || (sNonUiKnown && !sNonUiBlocked));
    }

    private static void applyMode(boolean enabled) {
        if (sArmed == enabled) return;
        int result=RodinTouchBridge.sendNativeMode(11,enabled ? 1 : 0);
        sArmed=enabled && result==0;
        Log.i(TAG,"Single-tap detector armed="+sArmed+" HAL result="+result);
    }

    private static void updateState() {
        boolean wanted=requested();
        if (!wanted || sProximity==null) {
            applyMode(false);
            if (sListening) sSensors.unregisterListener(PROXIMITY);
            sListening=false; sKnown=false; sFar=false;
            sNonUiListening=false; sNonUiKnown=false; sNonUiBlocked=true;
            return;
        }
        if (!sListening) {
            sKnown=false; sFar=false;
            sListening=sSensors.registerListener(PROXIMITY,sProximity,SensorManager.SENSOR_DELAY_NORMAL,sHandler);
            sNonUiKnown=false; sNonUiBlocked=true;
            sNonUiListening=sNonUi!=null && sSensors.registerListener(PROXIMITY,sNonUi,SensorManager.SENSOR_DELAY_NORMAL,sHandler);
        }
        applyMode(sListening && uncovered());
    }

    public static boolean handleKey(KeyEvent event) {
        if (event == null || event.getScanCode()!=354 || sContext==null || sPower.isInteractive()) return false;
        InputDevice device=event.getDevice();
        if (device==null || device.isVirtual() || !device.supportsSource(0x1002)) return false;
        if (event.getAction()==KeyEvent.ACTION_DOWN && event.getRepeatCount()==0 && sArmed) {
            sWakeLock.acquire(1500);
            sHandler.post(RodinAodTapBridge::deliverTap);
        }
        return true;
    }

    private static void deliverTap() {
        try {
            long now=SystemClock.elapsedRealtime();
            if (!requested() || !uncovered() || now-sLastTap<300) return;
            sLastTap=now;
            // The power-saving daemon hides clock content without changing the
            // user's AOD preference. Restore that content on a physical tap,
            // but never enable the AOD master switch or override its schedule.
            if (setting("Setting_AodEnable",0)==1 && scheduleAllowsNow()
                    && setting("Setting_AodSwitchEnable",0)==0) {
                Settings.Secure.putInt(sContext.getContentResolver(),"Setting_AodSwitchEnable",1);
                Log.i(TAG,"AOD clock content restored for hardware tap");
            }
            Object policy=policy();
            Field lock=policy.getClass().getDeclaredField("mLock");
            lock.setAccessible(true);
            Method notify=policy.getClass().getDeclaredMethod("notifyAllCallbacks",int.class,String.class);
            notify.setAccessible(true);
            // This controller reports a global wake key, without a touch position.
            // Give only gesture 16 its whole-display target anchor. This is not a
            // measured coordinate and is never used for region-specific gestures.
            synchronized(lock.get(policy)) { notify.invoke(policy,16,"16,0:0"); }
            Log.i(TAG,"Hardware single tap delivered to native AOD callback");
        } catch (Exception error) {
            Log.e(TAG,"Native single-tap delivery failed",error);
        } finally {
            if (sWakeLock.isHeld()) sWakeLock.release();
        }
    }
}
