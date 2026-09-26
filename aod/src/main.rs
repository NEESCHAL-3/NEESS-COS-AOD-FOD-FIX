use std::{
    fs::{self, File, OpenOptions},
    io::{BufRead, BufReader, Read, Write},
    process::{Command, Stdio},
    thread,
    time::Duration,
};

use base64::{engine::general_purpose::STANDARD, Engine};
use ed25519_dalek::{Signature, VerifyingKey};
use sha2::{Digest, Sha256};

const SYSTEM_PROP: &str = "/system/build.prop";
const AUTH_NAME: &str = "ro.nees.aod.auth";

const SENSOR_HAL: &str = "/vendor/lib64/hw/sensors.mt6899.so";
const AOD_DAEMON: &str = "/system/bin/nees_aodd";
const FOD_COMPAT: &str = "/vendor/lib64/librodin_fp_compat.so";

const BL: &str = "/sys/devices/virtual/mi_display/disp_feature/disp-DSI-0/backlight";

const AOD_BL: u32 = 28;
const FP_BL: u32 = 100;

const PUBLIC_KEY: [u8; 32] = [
    0xd5, 0x5e, 0x7f, 0x92, 0x8d, 0xbd, 0x7d, 0x84,
    0x11, 0xb0, 0xad, 0xa9, 0xc6, 0x7e, 0x06, 0x6f,
    0x1a, 0x4e, 0x51, 0xd4, 0x31, 0x99, 0x64, 0x33,
    0x06, 0x35, 0xe4, 0x1c, 0xbd, 0xa3, 0xc4, 0xa5,
];

fn klog(msg: &str) {
    let _ = Command::new("/system/bin/log")
        .args(["-t", "NeesAodD", msg])
        .status();

    println!("NeesAodD: {msg}");
}

fn sha256_bytes(data: &[u8]) -> String {
    let mut h = Sha256::new();
    h.update(data);
    format!("{:x}", h.finalize())
}

fn sha256_file(path: &str) -> Result<String, String> {
    let mut f = File::open(path).map_err(|e| format!("open {path}: {e}"))?;
    let mut h = Sha256::new();
    let mut buf = [0u8; 65536];

    loop {
        let n = f.read(&mut buf).map_err(|e| format!("read {path}: {e}"))?;
        if n == 0 {
            break;
        }
        h.update(&buf[..n]);
    }

    Ok(format!("{:x}", h.finalize()))
}

fn canonical_system_prop() -> Result<(String, String), String> {
    let raw = fs::read_to_string(SYSTEM_PROP)
        .map_err(|e| format!("cannot read {SYSTEM_PROP}: {e}"))?;

    let auth = raw
        .lines()
        .find_map(|line| line.strip_prefix(&format!("{AUTH_NAME}=")))
        .ok_or_else(|| format!("{AUTH_NAME} missing from {SYSTEM_PROP}"))?
        .trim()
        .to_string();

    let mut canonical = String::new();

    for line in raw.lines() {
        if line.starts_with(&format!("{AUTH_NAME}=")) {
            continue;
        }

        canonical.push_str(line);
        canonical.push('\n');
    }

    Ok((auth, sha256_bytes(canonical.as_bytes())))
}

fn authorize() -> Result<(), String> {
    let (value, buildprop_hash) = canonical_system_prop()?;

    let mut parts = value.splitn(3, '.');

    let version = parts.next().ok_or("missing authorization version")?;
    let rom_id = parts.next().ok_or("missing ROM ID")?;
    let encoded_signature = parts.next().ok_or("missing signature")?;

    if version != "NEES4" {
        return Err("wrong authorization version".into());
    }

    if rom_id.len() != 64 || !rom_id.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err("invalid ROM ID".into());
    }

    let hal_hash = sha256_file(SENSOR_HAL)?;
    let aod_hash = sha256_file(AOD_DAEMON)?;
    let fod_hash = sha256_file(FOD_COMPAT)?;

    let manifest = format!(
        "NEES_RODIN_SHARED_V4\n\
ROM_ID={}\n\
SYSTEM_BUILD_PROP_SHA256={}\n\
SENSOR_HAL_SHA256={}\n\
AOD_DAEMON_SHA256={}\n\
FOD_COMPAT_SHA256={}",
        rom_id, buildprop_hash, hal_hash, aod_hash, fod_hash
    );

    let decoded = STANDARD
        .decode(encoded_signature)
        .map_err(|_| "bad signature base64")?;

    let sig_bytes: [u8; 64] = decoded
        .try_into()
        .map_err(|_| "wrong signature length")?;

    let key = VerifyingKey::from_bytes(&PUBLIC_KEY)
        .map_err(|_| "bad embedded public key")?;

    let signature = Signature::from_bytes(&sig_bytes);

    key.verify_strict(manifest.as_bytes(), &signature)
        .map_err(|_| "ROM signature mismatch")?;

    Ok(())
}

fn set_prop(name: &str, value: &str) {
    let _ = Command::new("/system/bin/setprop")
        .args([name, value])
        .status();
}

fn settings_put(name: &str, value: &str) {
    let _ = Command::new("/system/bin/settings")
        .args(["put", "secure", name, value])
        .status();
}

fn write_backlight(value: u32) {
    if let Ok(mut f) = OpenOptions::new().write(true).open(BL) {
        let _ = write!(f, "{value}");
    }
}

fn read_backlight() -> Option<u32> {
    fs::read_to_string(BL)
        .ok()?
        .trim()
        .parse::<u32>()
        .ok()
}

fn ensure_aod_backlight() {
    if read_backlight() == Some(0) {
        write_backlight(AOD_BL);
        thread::sleep(Duration::from_millis(20));
        let after = read_backlight().unwrap_or(9999);
        klog(&format!("AOD backlight 0 -> {after}"));
    }
}

fn display_really_awake() -> bool {
    let output = match Command::new("/system/bin/dumpsys")
        .arg("power")
        .output()
    {
        Ok(v) => v,
        Err(_) => return false,
    };

    let text = String::from_utf8_lossy(&output.stdout);

    text.contains("mWakefulness=Awake")
        || text.contains("mInteractive=true")
        || text.contains("Wakefulness: Awake")
}

fn main() {
    let verify_only = std::env::args().any(|arg| arg == "--verify-only");

    match authorize() {
        Ok(_) => {
            if verify_only {
                klog("V4 shared authorization OK (verify-only)");
                return;
            }
            klog("V4 shared authorization OK");
        }
        Err(e) => {
            klog(&format!("AUTH NOTICE: {e}"));
            if verify_only {
                std::process::exit(73);
            }
            klog("Continuing in shared daemon mode");
        }
    }

    set_prop("sys.nees4.authorized", "1");
    set_prop("ro.nees.fod.compat", "1");
    set_prop("persist.sys.rodin.aod_keep_doze", "1");

    while !std::path::Path::new(BL).exists() {
        thread::sleep(Duration::from_millis(500));
    }

    settings_put("Setting_AodSwitchEnable", "1");
    klog("started AOD=28 FP=100 (ready)");

    let mut pending_energy_hide = false;
    let mut fp_only_phase = false;
    let mut fp_panel_allowed = false;

    loop {
        let mut child = match Command::new("/system/bin/logcat")
            .args(["-b", "all", "-v", "brief", "-T", "1"])
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
        {
            Ok(c) => c,
            Err(e) => {
                klog(&format!("cannot start logcat: {e}"));
                thread::sleep(Duration::from_secs(1));
                continue;
            }
        };

        let stdout = match child.stdout.take() {
            Some(s) => s,
            None => {
                thread::sleep(Duration::from_secs(1));
                continue;
            }
        };

        for result in BufReader::new(stdout).lines() {
            let line = match result {
                Ok(v) => v,
                Err(_) => continue,
            };

            // 1. DOZE ENTRY (Seamless AOD, Classic AOD, or Full Screen AOD)
            if line.contains("setScreenState changed:")
                && line.contains("->DOZE")
                && !fp_only_phase
            {
                thread::sleep(Duration::from_millis(220));
                ensure_aod_backlight();

                // Ensure Xiaomi touchfeature is in FOD mode (mode 10, value 1)
                let _ = Command::new("/system/bin/service")
                    .args(["call", "vendor.xiaomi.hw.touchfeature.ITouchFeature/default", "9", "i32", "0", "i32", "10", "i32", "1"])
                    .status();
            }

            // 2. ALL-DAY AOD KEEPALIVE
            // ColorOS wakes into DOZE for time updates then requests DOZE_SUSPEND.
            // Pure string matching, no settings process calls.
            if !fp_only_phase {
                if line.contains("Final-state=DOZE_SUSPEND") || line.contains("performAodUpdate") {
                    if read_backlight() == Some(0) {
                        write_backlight(AOD_BL);
                        klog("AOD keepalive -> hold panel 28");
                    }
                }
            }

            // 3. POWER-SAVING (10s/seamless/classic) TIMEOUT HIDE
            if line.contains("onEnergySavingNotifyHide")
                && !fp_only_phase
            {
                pending_energy_hide = true;
                klog("power-saving AOD hide detected");
            }

            // 4. TRANSITION TO OFF (AOD hides / panel turns off)
            if line.contains("setScreenState changed:DOZE->OFF")
                && pending_energy_hide
                && !fp_only_phase
            {
                // CRITICAL: Telling ColorOS Setting_AodSwitchEnable=0 enables the
                // Screen-Off Fingerprint (SOFOD) icon on touch/pickup!
                settings_put("Setting_AodSwitchEnable", "0");

                pending_energy_hide = false;
                fp_only_phase = true;
                fp_panel_allowed = true;
                write_backlight(0);

                klog("entered FP-only phase (SOFOD enabled)");
            }

            // 5. FAST SOFOD PATH (Screen-Off Fingerprint)
            if line.contains("notifyWakeUpCallback type 1")
                && fp_only_phase
            {
                fp_panel_allowed = true;
                write_backlight(FP_BL);
                klog("SOFOD wake -> panel 100");
            }

            if line.contains("setScreenState changed:OFF->DOZE")
                && fp_only_phase
                && fp_panel_allowed
            {
                thread::sleep(Duration::from_millis(15));
                if read_backlight() != Some(FP_BL) {
                    write_backlight(FP_BL);
                    klog("SOFOD DOZE -> panel 100");
                }
            }

            if line.contains("OnScreenFingerprintIcon")
                && line.contains("setVisibility VISIBLE")
                && fp_only_phase
                && fp_panel_allowed
            {
                if read_backlight() != Some(FP_BL) {
                    write_backlight(FP_BL);
                    klog("SOFOD icon -> panel 100");
                }
            }

            if line.contains("OnScreenFingerprintIcon")
                && line.contains("setVisibility INVISIBLE")
                && fp_only_phase
                && fp_panel_allowed
            {
                write_backlight(0);
                klog("FP-only icon hidden -> panel 0");
            }

            // 6. SCREEN WAKE TO ON
            if line.contains("setScreenState changed:")
                && line.contains("->ON")
            {
                fp_panel_allowed = false;
                klog("SCREEN ON -> release FP brightness control");

                if fp_only_phase {
                    thread::sleep(Duration::from_millis(700));

                    if display_really_awake() {
                        settings_put("Setting_AodSwitchEnable", "1");
                        pending_energy_hide = false;
                        fp_only_phase = false;
                        klog("real wake -> AOD content restored");
                    }
                }
            }
        }

        klog("logcat stream ended, restarting in 500ms");
        thread::sleep(Duration::from_millis(500));
    }
}
