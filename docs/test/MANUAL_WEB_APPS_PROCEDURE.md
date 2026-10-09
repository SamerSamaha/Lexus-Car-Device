# Manual procedure: web apps in the system browser (REQ-018, LHU-023)

Run on the Pi, parked or at the desk, with the hub running (`docs/release/PI_BRINGUP_CHECKLIST.md` steps 3.5 and 3.6). Every run is recorded in `docs/test/results/<date>_web_apps.md` with the table at the end of this file, filled in; a failed or partial run is recorded too, never repeated until it passes.

The web apps are content, not product code: what this procedure measures is whether the platform starts them, tracks them and returns from them, and what they cost in memory.

## Preparation

1. Record the browser and its version: `chromium --version`.
2. Record whether the browser's DRM module (Widevine) is present: `ls -d /usr/lib/chromium*/WidevineCdm 2>/dev/null; dpkg -l | grep -i widevine`, and in the browser `chrome://components` (look for "Widevine Content Decryption Module" and its version). Write "present, version ..." or "absent".
3. Record whether `/tmp` is in RAM (the browser profile and cache live there): `findmnt /tmp` (expected: `tmpfs`; assumption A14).
4. The registry `deploy/hub.conf` lists the three test apps: `video` (a public video page), `drm_check` (a public Widevine test page) and `game` (a public web game). None needs an account; no account, password or cookie is ever written into this repository.

## Steps

| # | App | Do | Record |
|---|---|---|---|
| 1 | Video | Tap "Video" on the hub; start the video with sound | Plays: yes or no; sound heard through the configured output (the car stereo once LHU-024 works, otherwise the panel's absent speaker means "no output configured"); stutter seen: yes or no |
| 2 | Memory | While step 1 plays, in a terminal: `lexus-hub --send status` to read `app_pid`, then `python3 tools/measure/sample_process_memory.py --pid <app_pid> --label browser_video --interval-seconds 10 --duration-seconds 600` | The summary lines (process count, RSS and PSS minimum, median, maximum); the CSV goes to `docs/measurements/memory/` and is committed unchanged |
| 3 | Return | Tap the panel's Hub launcher (or `lexus-hub --send return`) | Hub in front within 1 s: yes or no; `lexus-hub --send status` shows `state idle` |
| 4 | Protected audio | Tap "DRM check"; start the protected stream | Plays: yes or no (the measured fact REQ-018 asks for), with the DRM module result from preparation step 2 |
| 5 | Game | Tap "Game"; play with touch | Accepts touch: yes or no; the game's moves follow swipes: yes or no |
| 6 | Tracking | During any app, `lexus-hub --send status` | `state running` while the browser is open (assumption A10: the browser stays the process the hub launched) |

## Result table

| Run | Date | Browser version | Widevine | `/tmp` | Video plays | Sound | Memory (PSS median, MB) | Return within 1 s | Protected audio plays | Game takes touch | Tracked as running |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | | | | | | | | | | | |

Results: not yet measured.
