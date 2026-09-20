.class public Lcom/android/server/power/OplusFeatureAOD;
.super Ljava/lang/Object;
.source "OplusFeatureAOD.java"

# interfaces
.implements Lcom/android/server/power/IOplusFeatureAOD;


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Lcom/android/server/power/OplusFeatureAOD$InstallCtsBroadcastReceiver;
    }
.end annotation


# static fields
.field public static final AodUserSetEnable:Ljava/lang/String; = "Setting_AodEnable"

.field public static DEBUG:Z = false

.field public static final FINGERPRINT_UNLOCK:Ljava/lang/String; = "show_fingerprint_when_screen_off"

.field public static final FINGERPRINT_UNLOCK_SWITCH:Ljava/lang/String; = "oplus_customize_fingerprint_unlock_switch"

.field private static final TAG:Ljava/lang/String; = "OplusFeatureAOD"

.field public static final ZEN_AOD_USER_SET:Ljava/lang/String; = "display_zen_aod"

.field public static isOplusTestMode:Z

.field private static mContext:Landroid/content/Context;

.field private static mPMS:Lcom/android/server/power/PowerManagerService;

.field private static sInstance:Lcom/android/server/power/OplusFeatureAOD;


# instance fields
.field private mAodUserSetEnable:I

.field public mDozeStateMap:Ljava/util/HashMap;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/util/HashMap<",
            "Ljava/lang/Integer;",
            "Ljava/lang/Integer;",
            ">;"
        }
    .end annotation
.end field

.field public mFingerprintOpticalSupport:Z

.field private mFingerprintUnlock:I

.field private mFingerprintUnlockswitch:I

.field public mFlinger:Landroid/os/IBinder;

.field public final mMapLock:Ljava/lang/Object;

.field public mOplusAodSupport:Z

.field public mScreenState:I

.field private mZenAodUserSetEnable:I


# direct methods
.method static constructor <clinit>()V
    .registers 2

    .line 37
    const/4 v0, 0x0

    sput-boolean v0, Lcom/android/server/power/OplusFeatureAOD;->isOplusTestMode:Z

    .line 44
    const-string v1, "persist.sys.assert.panic"

    invoke-static {v1, v0}, Landroid/os/SystemProperties;->getBoolean(Ljava/lang/String;Z)Z

    move-result v0

    sput-boolean v0, Lcom/android/server/power/OplusFeatureAOD;->DEBUG:Z

    .line 45
    const/4 v0, 0x0

    sput-object v0, Lcom/android/server/power/OplusFeatureAOD;->sInstance:Lcom/android/server/power/OplusFeatureAOD;

    .line 47
    sput-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    .line 49
    sput-object v0, Lcom/android/server/power/OplusFeatureAOD;->mContext:Landroid/content/Context;

    return-void
.end method

.method public constructor <init>()V
    .registers 5

    .line 77
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 51
    const/4 v0, 0x0

    iput-boolean v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintOpticalSupport:Z

    .line 53
    new-instance v1, Ljava/lang/Object;

    invoke-direct {v1}, Ljava/lang/Object;-><init>()V

    iput-object v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mMapLock:Ljava/lang/Object;

    .line 54
    new-instance v1, Ljava/util/HashMap;

    invoke-direct {v1}, Ljava/util/HashMap;-><init>()V

    iput-object v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    .line 55
    iput v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    .line 56
    iput v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlock:I

    .line 57
    iput v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlockswitch:I

    .line 58
    iput v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mZenAodUserSetEnable:I

    .line 59
    iput-boolean v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mOplusAodSupport:Z

    .line 60
    iput v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mScreenState:I

    .line 78
    const-string v1, "create"

    const-string v2, "OplusFeatureAOD"

    invoke-static {v2, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    .line 79
    sget-object v1, Lcom/android/server/content/IOplusFeatureConfigManagerInternal;->DEFAULT:Lcom/android/server/content/IOplusFeatureConfigManagerInternal;

    new-array v0, v0, [Ljava/lang/Object;

    invoke-static {v1, v0}, Landroid/common/OplusFeatureCache;->getOrCreate(Landroid/common/IOplusCommonFeature;[Ljava/lang/Object;)Landroid/common/IOplusCommonFeature;

    move-result-object v0

    check-cast v0, Lcom/android/server/content/IOplusFeatureConfigManagerInternal;

    const-string v1, "oplus.software.display.aod_support"

    invoke-interface {v0, v1}, Lcom/android/server/content/IOplusFeatureConfigManagerInternal;->hasFeature(Ljava/lang/String;)Z

    move-result v0

    const/4 v1, 0x1

    if-eqz v0, :cond_6f

    .line 80
    iput-boolean v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mOplusAodSupport:Z

    .line 81
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDozeAfterScreenOff(Z)V

    .line 82
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDecoupleHalAutoSuspendModeFromDisplayConfig(Z)V

    .line 83
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDecoupleHalInteractiveModeFromDisplayConfig(Z)V

    .line 84
    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    const-string v3, "mOplusAodSupport = "

    invoke-virtual {v0, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    iget-boolean v3, p0, Lcom/android/server/power/OplusFeatureAOD;->mOplusAodSupport:Z

    invoke-virtual {v0, v3}, Ljava/lang/StringBuilder;->append(Z)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-static {v2, v0}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 87
    :cond_6f
    const-string v0, "persist.vendor.fingerprint.sensor_type"

    const-string/jumbo v2, "unknow"

    invoke-static {v0, v2}, Landroid/os/SystemProperties;->get(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;

    move-result-object v0

    const-string v2, "optical"

    invoke-virtual {v2, v0}, Ljava/lang/Object;->equals(Ljava/lang/Object;)Z

    move-result v0

    if-eqz v0, :cond_89

    .line 88
    const-string v0, "Biometrics_DEBUG"

    const-string v2, "[OplusFeatureAOD] sensor is optical"

    invoke-static {v0, v2}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 89
    iput-boolean v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintOpticalSupport:Z

    .line 91
    :cond_89
    return-void
.end method

.method public static varargs getInstance([Ljava/lang/Object;)Lcom/android/server/power/OplusFeatureAOD;
    .registers 4
    .param p0, "vars"    # [Ljava/lang/Object;

    .line 64
    const/4 v0, 0x0

    aget-object v0, p0, v0

    check-cast v0, Landroid/content/Context;

    sput-object v0, Lcom/android/server/power/OplusFeatureAOD;->mContext:Landroid/content/Context;

    .line 65
    const/4 v0, 0x1

    aget-object v0, p0, v0

    check-cast v0, Lcom/android/server/power/PowerManagerService;

    .line 66
    .local v0, "power":Lcom/android/server/power/PowerManagerService;
    const-string v1, "OplusFeatureAOD"

    const-string v2, "PowerManagerService getInstance"

    invoke-static {v1, v2}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 67
    sput-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    .line 69
    const-class v1, Lcom/android/server/power/OplusFeatureAOD;

    monitor-enter v1

    .line 70
    :try_start_18
    sget-object v2, Lcom/android/server/power/OplusFeatureAOD;->sInstance:Lcom/android/server/power/OplusFeatureAOD;

    if-nez v2, :cond_23

    .line 71
    new-instance v2, Lcom/android/server/power/OplusFeatureAOD;

    invoke-direct {v2}, Lcom/android/server/power/OplusFeatureAOD;-><init>()V

    sput-object v2, Lcom/android/server/power/OplusFeatureAOD;->sInstance:Lcom/android/server/power/OplusFeatureAOD;

    .line 73
    :cond_23
    sget-object v2, Lcom/android/server/power/OplusFeatureAOD;->sInstance:Lcom/android/server/power/OplusFeatureAOD;

    monitor-exit v1

    return-object v2

    .line 74
    :catchall_27
    move-exception v2

    monitor-exit v1
    :try_end_29
    .catchall {:try_start_18 .. :try_end_29} :catchall_27

    throw v2
.end method


# virtual methods
.method public clearDozeStateMap()V
    .registers 3

    .line 367
    iget-object v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mMapLock:Ljava/lang/Object;

    monitor-enter v0

    .line 368
    :try_start_3
    iget-object v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-virtual {v1}, Ljava/util/HashMap;->clear()V

    .line 369
    monitor-exit v0
    :try_end_9
    .catchall {:try_start_3 .. :try_end_9} :catchall_11

    .line 370
    const-string v0, "OplusFeatureAOD"

    const-string v1, "clearDozeStateMap"

    invoke-static {v0, v1}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 371
    return-void

    .line 369
    :catchall_11
    move-exception v1

    :try_start_12
    monitor-exit v0
    :try_end_13
    .catchall {:try_start_12 .. :try_end_13} :catchall_11

    throw v1
.end method

.method public handleAodChanged()V
    .registers 7

    .line 299
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mContext:Landroid/content/Context;

    invoke-virtual {v0}, Landroid/content/Context;->getContentResolver()Landroid/content/ContentResolver;

    move-result-object v0

    .line 300
    .local v0, "resolver":Landroid/content/ContentResolver;
    const-string v1, "Setting_AodEnable"

    const/4 v2, 0x0

    const/4 v3, -0x2

    invoke-static {v0, v1, v2, v3}, Landroid/provider/Settings$Secure;->getIntForUser(Landroid/content/ContentResolver;Ljava/lang/String;II)I

    move-result v1

    iput v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    .line 302
    iget v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    const-string v4, "OplusFeatureAOD"

    if-nez v1, :cond_2f

    .line 305
    sget-boolean v1, Lcom/android/server/power/OplusFeatureAOD;->isOplusTestMode:Z

    const/4 v5, 0x1

    if-eqz v1, :cond_1e

    .line 306
    iput v5, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    goto :goto_26

    .line 308
    :cond_1e
    const-string v1, "doze_always_on"

    invoke-static {v0, v1, v2, v3}, Landroid/provider/Settings$Secure;->getIntForUser(Landroid/content/ContentResolver;Ljava/lang/String;II)I

    move-result v1

    iput v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    .line 311
    :goto_26
    iget v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    if-ne v1, v5, :cond_2f

    .line 312
    const-string v1, "mAodUserSetEnable set by Settings.Secure.DOZE_ALWAYS_ON"

    invoke-static {v4, v1}, Landroid/util/Slog;->w(Ljava/lang/String;Ljava/lang/String;)I

    .line 316
    :cond_2f
    const-string/jumbo v1, "show_fingerprint_when_screen_off"

    invoke-static {v0, v1, v2, v3}, Landroid/provider/Settings$Secure;->getIntForUser(Landroid/content/ContentResolver;Ljava/lang/String;II)I

    move-result v1

    iput v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlock:I

    .line 318
    const-string v1, "oplus_customize_fingerprint_unlock_switch"

    invoke-static {v0, v1, v2, v3}, Landroid/provider/Settings$Secure;->getIntForUser(Landroid/content/ContentResolver;Ljava/lang/String;II)I

    move-result v1

    iput v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlockswitch:I

    .line 321
    const-string v1, "display_zen_aod"

    invoke-static {v0, v1, v2, v3}, Landroid/provider/Settings$Secure;->getIntForUser(Landroid/content/ContentResolver;Ljava/lang/String;II)I

    move-result v1

    iput v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mZenAodUserSetEnable:I

    .line 323
    sget-boolean v1, Lcom/android/server/power/OplusFeatureAOD;->DEBUG:Z

    if-eqz v1, :cond_84

    .line 324
    new-instance v1, Ljava/lang/StringBuilder;

    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    const-string v2, "aod set change "

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    iget v2, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    const-string v2, " "

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    iget v3, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlock:I

    invoke-virtual {v1, v3}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    iget v3, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlockswitch:I

    invoke-virtual {v1, v3}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    iget v2, p0, Lcom/android/server/power/OplusFeatureAOD;->mZenAodUserSetEnable:I

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    invoke-static {v4, v1}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 327
    :cond_84
    return-void
.end method

.method public isShouldGoAod()Z
    .registers 4

    .line 242
    const/4 v0, 0x0

    .line 243
    .local v0, "shouldGoDoze":Z
    iget v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mAodUserSetEnable:I

    const/4 v2, 0x1

    if-eq v1, v2, :cond_15

    iget v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlockswitch:I

    if-ne v1, v2, :cond_e

    iget v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintUnlock:I

    if-eq v1, v2, :cond_15

    :cond_e
    iget v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mZenAodUserSetEnable:I

    if-ne v1, v2, :cond_13

    goto :goto_15

    .line 248
    :cond_13
    const/4 v0, 0x0

    goto :goto_16

    .line 246
    :cond_15
    :goto_15
    const/4 v0, 0x1

    .line 250
    :goto_16
    return v0
.end method

.method public notifySfUnBlockScreenOn()V
    .registers 7

    .line 346
    const-string v0, "notifySfUnBlockScreenOn"

    const-string v1, "OplusFeatureAOD"

    invoke-static {v1, v0}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 348
    iget-object v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFlinger:Landroid/os/IBinder;

    if-nez v0, :cond_18

    .line 349
    const-string v0, "notifySfUnBlockScreenOn, mFlinger is null "

    invoke-static {v1, v0}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 350
    const-string v0, "SurfaceFlinger"

    invoke-static {v0}, Landroid/os/ServiceManager;->getService(Ljava/lang/String;)Landroid/os/IBinder;

    move-result-object v0

    iput-object v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFlinger:Landroid/os/IBinder;

    .line 354
    :cond_18
    :try_start_18
    iget-object v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFlinger:Landroid/os/IBinder;

    if-eqz v0, :cond_31

    .line 355
    invoke-static {}, Landroid/os/Parcel;->obtain()Landroid/os/Parcel;

    move-result-object v0

    .line 356
    .local v0, "data":Landroid/os/Parcel;
    const-string v2, "android.ui.ISurfaceComposer"

    invoke-virtual {v0, v2}, Landroid/os/Parcel;->writeInterfaceToken(Ljava/lang/String;)V

    .line 357
    iget-object v2, p0, Lcom/android/server/power/OplusFeatureAOD;->mFlinger:Landroid/os/IBinder;

    const/4 v3, 0x0

    const/4 v4, 0x1

    const/16 v5, 0x4e23

    invoke-interface {v2, v5, v0, v3, v4}, Landroid/os/IBinder;->transact(ILandroid/os/Parcel;Landroid/os/Parcel;I)Z

    .line 358
    invoke-virtual {v0}, Landroid/os/Parcel;->recycle()V
    :try_end_31
    .catch Landroid/os/RemoteException; {:try_start_18 .. :try_end_31} :catch_32

    .line 362
    .end local v0    # "data":Landroid/os/Parcel;
    :cond_31
    goto :goto_38

    .line 360
    :catch_32
    move-exception v0

    .line 361
    .local v0, "ex":Landroid/os/RemoteException;
    const-string v2, "get SurfaceFlinger Service failed"

    invoke-static {v1, v2}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 363
    .end local v0    # "ex":Landroid/os/RemoteException;
    :goto_38
    return-void
.end method

.method public onDisplayStateChange(Landroid/service/dreams/DreamManagerInternal;I)V
    .registers 5
    .param p1, "dreamManager"    # Landroid/service/dreams/DreamManagerInternal;
    .param p2, "state"    # I

    .line 331
    if-eqz p1, :cond_3f

    .line 332
    sget-boolean v0, Lcom/android/server/power/OplusFeatureAOD;->DEBUG:Z

    if-eqz v0, :cond_2c

    .line 333
    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    const-string v1, "onDisplayStateChange state = "

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {v0, p2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v0

    const-string v1, "isDreaming = "

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {p1}, Landroid/service/dreams/DreamManagerInternal;->isDreaming()Z

    move-result v1

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Z)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    const-string v1, "OplusFeatureAOD"

    invoke-static {v1, v0}, Landroid/util/Slog;->i(Ljava/lang/String;Ljava/lang/String;)I

    .line 335
    :cond_2c
    invoke-virtual {p1}, Landroid/service/dreams/DreamManagerInternal;->isDreaming()Z

    move-result v0

    if-eqz v0, :cond_3f

    const/4 v0, 0x2

    if-ne p2, v0, :cond_3f

    .line 336
    iget-boolean v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFingerprintOpticalSupport:Z

    if-nez v0, :cond_3f

    .line 337
    const/4 v0, 0x0

    const-string v1, "power on"

    invoke-virtual {p1, v0, v1}, Landroid/service/dreams/DreamManagerInternal;->stopDream(ZLjava/lang/String;)V

    .line 341
    :cond_3f
    return-void
.end method

.method public registerInstallCtsBroadcastReceiver()V
    .registers 4

    .line 267
    new-instance v0, Landroid/content/IntentFilter;

    invoke-direct {v0}, Landroid/content/IntentFilter;-><init>()V

    .line 268
    .local v0, "intentFilter":Landroid/content/IntentFilter;
    const-string v1, "android.intent.action.PACKAGE_ADDED"

    invoke-virtual {v0, v1}, Landroid/content/IntentFilter;->addAction(Ljava/lang/String;)V

    .line 269
    const-string v1, "android.intent.action.PACKAGE_REMOVED"

    invoke-virtual {v0, v1}, Landroid/content/IntentFilter;->addAction(Ljava/lang/String;)V

    .line 270
    const-string v1, "package"

    invoke-virtual {v0, v1}, Landroid/content/IntentFilter;->addDataScheme(Ljava/lang/String;)V

    .line 271
    sget-object v1, Lcom/android/server/power/OplusFeatureAOD;->mContext:Landroid/content/Context;

    new-instance v2, Lcom/android/server/power/OplusFeatureAOD$InstallCtsBroadcastReceiver;

    invoke-direct {v2, p0}, Lcom/android/server/power/OplusFeatureAOD$InstallCtsBroadcastReceiver;-><init>(Lcom/android/server/power/OplusFeatureAOD;)V

    invoke-virtual {v1, v2, v0}, Landroid/content/Context;->registerReceiver(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;

    .line 272
    return-void
.end method

.method public setAodSettingStatus()V
    .registers 3

    .line 255
    invoke-virtual {p0}, Lcom/android/server/power/OplusFeatureAOD;->isShouldGoAod()Z

    move-result v0

    if-eqz v0, :cond_1a

    .line 256
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    const/4 v1, 0x1

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDecoupleHalAutoSuspendModeFromDisplayConfig(Z)V

    .line 257
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDreamsActivateOnSleepSetting(Z)V

    goto :goto_2d

    .line 259
    :cond_1a
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    const/4 v1, 0x0

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDecoupleHalAutoSuspendModeFromDisplayConfig(Z)V

    .line 260
    sget-object v0, Lcom/android/server/power/OplusFeatureAOD;->mPMS:Lcom/android/server/power/PowerManagerService;

    invoke-virtual {v0}, Lcom/android/server/power/PowerManagerService;->getWrapper()Lcom/android/server/power/IPowerManagerServiceWrapper;

    move-result-object v0

    invoke-interface {v0, v1}, Lcom/android/server/power/IPowerManagerServiceWrapper;->setDreamsActivateOnSleepSetting(Z)V

    .line 262
    :goto_2d
    return-void
.end method

.method public setDozeOverride(II)V
    .registers 10
    .param p1, "screenState"    # I
    .param p2, "screenBrightness"    # I

    const/4 v0, 0x4

    if-ne p1, v0, :cond_d

    const-string v0, "persist.sys.rodin.aod_keep_doze"

    const/4 v1, 0x0

    invoke-static {v0, v1}, Landroid/os/SystemProperties;->getBoolean(Ljava/lang/String;Z)Z

    move-result v0

    if-eqz v0, :cond_d

    const/4 p1, 0x3

    .line 95
    :cond_d
    const-string v0, "OplusFeatureAOD"

    new-instance v1, Ljava/lang/StringBuilder;

    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v2, "setDozeOverride screenState:"

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1, p1}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    const-string v2, " screenBrightness:"

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1, p2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    const-string v2, " mScreenState:"

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    iget v2, p0, Lcom/android/server/power/OplusFeatureAOD;->mScreenState:I

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    invoke-static {v0, v1}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 97
    iget v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mScreenState:I

    const/4 v1, 0x2

    if-ne v0, v1, :cond_42

    .line 98
    return-void

    .line 101
    :cond_42
    const/4 v0, 0x0

    .line 102
    .local v0, "value":I
    packed-switch p1, :pswitch_data_8a

    goto :goto_4f

    .line 107
    :pswitch_47
    const/4 v0, 0x1

    .line 108
    goto :goto_4f

    .line 110
    :pswitch_49
    const/4 v0, 0x2

    .line 111
    goto :goto_4f

    .line 113
    :pswitch_4b
    const/4 v0, 0x3

    .line 114
    goto :goto_4f

    .line 104
    :pswitch_4d
    const/4 v0, 0x0

    .line 105
    nop

    .line 118
    :goto_4f
    invoke-static {}, Landroid/os/Binder;->getCallingPid()I

    move-result v2

    .line 119
    .local v2, "pid":I
    iget-object v3, p0, Lcom/android/server/power/OplusFeatureAOD;->mMapLock:Ljava/lang/Object;

    monitor-enter v3

    .line 120
    :try_start_56
    iget-object v4, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-static {v2}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v5

    invoke-static {v0}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v6

    invoke-virtual {v4, v5, v6}, Ljava/util/HashMap;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;

    .line 123
    iget-object v4, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-virtual {v4}, Ljava/util/HashMap;->size()I

    move-result v4

    if-le v4, v1, :cond_85

    .line 124
    const-string v1, "OplusFeatureAOD"

    const-string/jumbo v4, "systemUI have been killed, clear map"

    invoke-static {v1, v4}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 125
    iget-object v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-virtual {v1}, Ljava/util/HashMap;->clear()V

    .line 126
    iget-object v1, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-static {v2}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v4

    invoke-static {v0}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v5

    invoke-virtual {v1, v4, v5}, Ljava/util/HashMap;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;

    .line 128
    :cond_85
    monitor-exit v3

    .line 130
    return-void

    .line 128
    :catchall_87
    move-exception v1

    monitor-exit v3
    :try_end_89
    .catchall {:try_start_56 .. :try_end_89} :catchall_87

    throw v1

    :pswitch_data_8a
    .packed-switch 0x1
        :pswitch_4d
        :pswitch_4b
        :pswitch_49
        :pswitch_47
    .end packed-switch
.end method

.method public setDozeOverrideFromDreamManager(II)V
    .registers 9
    .param p1, "screenState"    # I
    .param p2, "screenBrightness"    # I

    const/4 v0, 0x4

    if-ne p1, v0, :cond_d

    const-string v0, "persist.sys.rodin.aod_keep_doze"

    const/4 v1, 0x0

    invoke-static {v0, v1}, Landroid/os/SystemProperties;->getBoolean(Ljava/lang/String;Z)Z

    move-result v0

    if-eqz v0, :cond_d

    const/4 p1, 0x3

    .line 135
    :cond_d
    const/4 v0, 0x0

    .line 136
    .local v0, "value":I
    packed-switch p1, :pswitch_data_36

    goto :goto_1c

    .line 141
    :pswitch_12
    const/4 v0, 0x1

    .line 142
    goto :goto_1c

    .line 147
    :pswitch_14
    const/4 v0, 0x3

    .line 148
    goto :goto_1c

    .line 150
    :pswitch_16
    const/4 v0, 0x4

    .line 151
    goto :goto_1c

    .line 138
    :pswitch_18
    const/4 v0, 0x0

    .line 139
    goto :goto_1c

    .line 144
    :pswitch_1a
    const/4 v0, 0x2

    .line 145
    nop

    .line 155
    :goto_1c
    invoke-static {}, Landroid/os/Binder;->getCallingPid()I

    move-result v1

    .line 156
    .local v1, "pid":I
    iget-object v2, p0, Lcom/android/server/power/OplusFeatureAOD;->mMapLock:Ljava/lang/Object;

    monitor-enter v2

    .line 157
    :try_start_23
    iget-object v3, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-static {v1}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v4

    invoke-static {v0}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v5

    invoke-virtual {v3, v4, v5}, Ljava/util/HashMap;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;

    .line 158
    monitor-exit v2

    .line 159
    return-void

    .line 158
    :catchall_32
    move-exception v3

    monitor-exit v2
    :try_end_34
    .catchall {:try_start_23 .. :try_end_34} :catchall_32

    throw v3

    nop

    :pswitch_data_36
    .packed-switch 0x0
        :pswitch_1a
        :pswitch_18
        :pswitch_16
        :pswitch_14
        :pswitch_12
    .end packed-switch
.end method

.method public setDozeOverrideFromDreamManagerInternal(II)I
    .registers 11
    .param p1, "screenState"    # I
    .param p2, "screenBrightness"    # I

    const/4 v0, 0x4

    if-ne p1, v0, :cond_d

    const-string v0, "persist.sys.rodin.aod_keep_doze"

    const/4 v1, 0x0

    invoke-static {v0, v1}, Landroid/os/SystemProperties;->getBoolean(Ljava/lang/String;Z)Z

    move-result v0

    if-eqz v0, :cond_d

    const/4 p1, 0x3

    .line 171
    :cond_d
    const/4 v0, 0x0

    .line 172
    .local v0, "value":I
    packed-switch p1, :pswitch_data_84

    goto :goto_1c

    .line 177
    :pswitch_12
    const/4 v0, 0x1

    .line 178
    goto :goto_1c

    .line 183
    :pswitch_14
    const/4 v0, 0x3

    .line 184
    goto :goto_1c

    .line 186
    :pswitch_16
    const/4 v0, 0x4

    .line 187
    goto :goto_1c

    .line 174
    :pswitch_18
    const/4 v0, 0x0

    .line 175
    goto :goto_1c

    .line 180
    :pswitch_1a
    const/4 v0, 0x2

    .line 181
    nop

    .line 191
    :goto_1c
    const/4 v1, 0x0

    .line 192
    .local v1, "tmp":I
    iget-object v2, p0, Lcom/android/server/power/OplusFeatureAOD;->mMapLock:Ljava/lang/Object;

    monitor-enter v2

    .line 193
    :try_start_20
    iget-object v3, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-virtual {v3}, Ljava/util/HashMap;->keySet()Ljava/util/Set;

    move-result-object v3

    invoke-interface {v3}, Ljava/util/Set;->iterator()Ljava/util/Iterator;

    move-result-object v3

    :goto_2a
    invoke-interface {v3}, Ljava/util/Iterator;->hasNext()Z

    move-result v4

    if-eqz v4, :cond_71

    invoke-interface {v3}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v4

    check-cast v4, Ljava/lang/Integer;

    invoke-virtual {v4}, Ljava/lang/Integer;->intValue()I

    move-result v4

    .line 194
    .local v4, "key":I
    iget-object v5, p0, Lcom/android/server/power/OplusFeatureAOD;->mDozeStateMap:Ljava/util/HashMap;

    invoke-static {v4}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v6

    invoke-virtual {v5, v6}, Ljava/util/HashMap;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v5

    check-cast v5, Ljava/lang/Integer;

    invoke-virtual {v5}, Ljava/lang/Integer;->intValue()I

    move-result v5

    move v1, v5

    .line 195
    const-string v5, "OplusFeatureAOD"

    new-instance v6, Ljava/lang/StringBuilder;

    invoke-direct {v6}, Ljava/lang/StringBuilder;-><init>()V

    const-string v7, "key:"

    invoke-virtual {v6, v7}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v6

    invoke-virtual {v6, v4}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v6

    const-string v7, " tmp:"

    invoke-virtual {v6, v7}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v6

    invoke-virtual {v6, v1}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v6

    invoke-virtual {v6}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v6

    invoke-static {v5, v6}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 196
    if-le v1, v0, :cond_70

    .line 197
    move v0, v1

    .line 199
    .end local v4    # "key":I
    :cond_70
    goto :goto_2a

    .line 200
    :cond_71
    monitor-exit v2

    .line 201
    packed-switch v0, :pswitch_data_92

    goto :goto_80

    .line 215
    :pswitch_76
    const/4 p1, 0x2

    .line 216
    goto :goto_80

    .line 212
    :pswitch_78
    const/4 p1, 0x3

    .line 213
    goto :goto_80

    .line 209
    :pswitch_7a
    const/4 p1, 0x0

    .line 210
    goto :goto_80

    .line 206
    :pswitch_7c
    const/4 p1, 0x4

    .line 207
    goto :goto_80

    .line 203
    :pswitch_7e
    const/4 p1, 0x1

    .line 204
    nop

    .line 221
    :goto_80
    return p1

    .line 200
    :catchall_81
    move-exception v3

    monitor-exit v2
    :try_end_83
    .catchall {:try_start_20 .. :try_end_83} :catchall_81

    throw v3

    :pswitch_data_84
    .packed-switch 0x0
        :pswitch_1a
        :pswitch_18
        :pswitch_16
        :pswitch_14
        :pswitch_12
    .end packed-switch

    :pswitch_data_92
    .packed-switch 0x0
        :pswitch_7e
        :pswitch_7c
        :pswitch_7a
        :pswitch_78
        :pswitch_76
    .end packed-switch
.end method

.method public systemReady()V
    .registers 3

    .line 228
    const-string v0, "SurfaceFlinger"

    invoke-static {v0}, Landroid/os/ServiceManager;->getService(Ljava/lang/String;)Landroid/os/IBinder;

    move-result-object v0

    iput-object v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFlinger:Landroid/os/IBinder;

    .line 229
    iget-object v0, p0, Lcom/android/server/power/OplusFeatureAOD;->mFlinger:Landroid/os/IBinder;

    const-string v1, "OplusFeatureAOD"

    if-eqz v0, :cond_14

    .line 230
    const-string v0, "get SurfaceFlinger Service sucess"

    invoke-static {v1, v0}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    goto :goto_19

    .line 232
    :cond_14
    const-string v0, "get SurfaceFlinger Service failed"

    invoke-static {v1, v0}, Landroid/util/Slog;->d(Ljava/lang/String;Ljava/lang/String;)I

    .line 236
    :goto_19
    invoke-virtual {p0}, Lcom/android/server/power/OplusFeatureAOD;->registerInstallCtsBroadcastReceiver()V

    .line 238
    return-void
.end method
