# Testing

## Post-boot verification

```bash
adb shell 'su -M -c "
echo AOD_state=$(getprop init.svc.nees_aodd)
echo AOD_pid=$(pidof nees_aodd)
echo runtime_auth=$(getprop sys.nees4.authorized)

/system/bin/nees_aodd --verify-only
echo verify_rc=$?

service list | grep -i displaypanelfeature || true

logcat -b all -d -v time | grep -Ei \
"NEES4 runtime authorization OK|OplusPanel registered|AUTH REFUSED|authorization timeout" \
| tail -80
"'
```

Expected:

```text
AOD_state=running
runtime_auth=1
verify_rc=0
NEES4 runtime authorization OK
OplusPanel registered: vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default
```

## Rapid-lock torture test

Repeat 8-10 times:

```text
unlock -> lock -> immediately press FOD -> unlock -> repeat
```

Useful filter:

```bash
adb shell 'su -M -c "
logcat -c
timeout 120 logcat -b all -v time | grep --line-buffered -Ei \
"RodinFpCompat|AUTH start|PHYSICAL FOD DOWN|PHYSICAL FOD UP|pending|replay pending|FOD SESSION REARM|terminal contact|AUTH terminal|onAuthenticationSucceeded|onAuthenticationFailed|onError|state4=|state1="
"'
```

Healthy cycle:

```text
AUTH start state4=1 state1=1 armed=1
PHYSICAL FOD DOWN
onAuthenticationSucceeded
PHYSICAL FOD UP
FOD SESSION REARM state4=1 state1=1 armed=1
```

## AOD modes

Verify:

- Seamless + All-day
- Classic + All-day
- Full-screen/Panoramic + All-day
- Power Saving retains ColorOS policy

Do not permanently force SystemUI debug `keep_aod_on=1`.

## Brightness regression

After FOD unlock, verify the home screen immediately returns to normal brightness.
