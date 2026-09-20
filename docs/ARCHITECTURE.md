# Architecture

## FOD compatibility

`librodin_fp_compat.so` is loaded into Xiaomi `mfp-daemon`.

It keeps Xiaomi as owner of the real hardware path while adding Oplus-compatible behavior:

- fingerprint/session Binder hooks
- ColorOS FOD touch callbacks
- session lifecycle state
- physical touch tracking independent of listener readiness
- Xiaomi FOD condition re-arm
- Oplus DisplayPanelFeature service
- Local-HBM/AOD capability reporting

## Rapid-lock race fix

Old failure:

```text
DOWN while listener off -> event discarded
UP while listener off -> event discarded
stale physical-down state remains
next AUTH suppressed
FOD remains dead until another reset path
```

Current logic:

```text
DOWN always updates physical state
  -> listener/session not ready: mark pending
  -> ready: forward DOWN

UP always clears physical + pending state
  -> listener off: force Xiaomi state4 cleanup
  -> listener on: forward UP and re-arm

AUTH start
  -> if pending DOWN is still physically held:
     arm state4/state1
     replay DOWN
```

## Authorization

Do not fork/exec the AOD verifier from `mfp-daemon`.

Final flow:

```text
nees_aodd verifies NEES4 signature
-> publishes sys.nees4.authorized=1
-> FOD shim observes authorization
-> protected compatibility behavior activates
```

The property is volatile and must be recreated after every reboot.
