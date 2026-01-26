# Factory Provisioning Checklist (ESP32-C6 Secure Boot v2 + Flash Encryption Release)

WARNING: eFuse operations are irreversible.

A) Inputs
- Private signing key stored securely (offline/HSM)
- Pinned certificate (or CA) finalized + rotation plan
- Production build reproducible + traceable

B) Per-device steps
1) Archive baseline:
- idf.py -p PORT efuse-summary

2) Flash signed validation build and run QA:
- Secure Boot state log
- Wi-Fi connect check
- OTA dry-run on sample devices

3) Burn Secure Boot key digest into eFuses (root-of-trust).

4) Enable Flash Encryption in Release mode.

5) Final validation:
- boots signed images only
- flash encryption enabled
- OTA works

6) Archive post state:
- idf.py -p PORT efuse-summary
- firmware version + secure_version + key id
