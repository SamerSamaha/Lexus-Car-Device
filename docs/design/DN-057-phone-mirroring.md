# DN-057: iPhone screen mirroring (light note)

| | |
|---|---|
| Ticket | LHU-057 |
| Requirements | REQ-033 (new); uses REQ-031 (the hub's Close control) |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 10 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

The owner wants Waze and other phone apps on the touchscreen for the demo. Phone projection (CarPlay) is proprietary and cut (D-042). AirPlay screen mirroring is built into the iPhone, and Debian 13 has a receiver, uxplay 1.71.1. It is a mirror: the touchscreen does not control the phone, and the tile says so.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `lexus-mirroring` (`deploy/car/lexus-mirroring`) | `exec uxplay -n "Lexus Head Unit" -nh -fs -vs waylandsink -avdec`; `--print`; `LEXUS_MIRRORING_OPTIONS` replaces the options | `exec`: the hub tracks the receiver. `-avdec`: the Pi 5 has no hardware H.264 decoder |
| Hub tile "Phone mirroring" | `lexus-mirroring`, `restart = never` | Labelled as mirroring; the hub's strip reads "Phone mirroring running" while it runs |
| The way back | When the phone stops, uxplay closes its window and waits; the hub is visible again; its Close control (REQ-031) stops the receiver | uxplay has no option to exit after one session |
| Installer | `uxplay`, `gstreamer1.0-plugins-bad` (waylandsink), `gstreamer1.0-plugins-good`, `gstreamer1.0-libav` (software H.264), `avahi-daemon` (the iPhone finds the receiver by mDNS) | All Debian 13 packages |

## Failure cases

| Case | Handling |
|---|---|
| The iPhone does not list the receiver | avahi-daemon not running, or the phone and the Pi on different networks: the checklist checks both |
| The picture stutters (software decoding) | Recorded in step 3.22 (A35); lower the phone's quality is not possible from the receiver, so the fallback is accepting it for the video |
| Notifications on the mirrored screen | An iPhone setting (Screen Sharing, Allow Notifications off, the iOS default); the checklist sets it before recording (A36). The Pi cannot filter what the phone sends |
| A mirrored map shows the area around home | Not detectable from the Pi (the phone link pauses when Safari is in the background); an operating rule in step 3.22: do not record mirroring near home |

## Test plan

| Requirement | Test |
|---|---|
| REQ-033 | `tools/test_phone_mirroring.py`: the command (name, full screen, Wayland sink, software decoding), the option override, `exec`, the tile and its label, the installer's packages and link |
| REQ-033, REQ-031 | `tests/hmi_hub/tst_hub_screen.qml`: the Close control |
| REQ-033 | On the Pi with the iPhone: checklist step 3.22 |

## Alternative considered

RPiPlay was rejected: it targets the older Raspberry Pi video stack and is not in Debian 13. Mirroring through a web page is not possible from iOS.
