# DN-055: Web apps in the car with a way back to the hub (light note)

| | |
|---|---|
| Ticket | LHU-055 |
| Requirements | REQ-031 (new); touches REQ-016, REQ-018 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

The demo needs YouTube and Spotify on the touchscreen in the car. In car mode the hub is full screen and there is no desktop panel, so a browser window had no way back to the hub (OQ-29), which is why DN-044 kept web apps off the car registry. Wayland does not let the hub raise itself over another window, and the panel launcher of DN-021 is not there.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `lexus-browser <url>` (`deploy/car/lexus-browser`) | `exec chromium --ozone-platform=wayland --user-data-dir=~/.cache/lexus-car-browser --load-extension=/usr/local/share/lexus-head-unit/browser-return --disable-features=DisableLoadExtensionCommandLineSwitch --start-fullscreen --autoplay-policy=no-user-gesture-required --no-first-run --noerrdialogs --password-store=basic --app=<url>`; `--print` lists the command | `exec`: the browser is the process the hub started (A10). A profile of its own, kept across reboots, so a YouTube or Spotify login stays |
| The Hub button extension (`deploy/car/browser_return/`, Manifest V3) | A 10 mm (116 px) round "Hub" button in the top-left corner of every page, in a closed shadow root, moved into the full-screen element when a video goes full screen; a tap asks the service worker, which closes the window | One window and its own profile, so the browser exits; the hub sees its app exit and is in front again (REQ-016). No permission is requested |
| `deploy/car/hub_car.conf` | `browser_command = lexus-browser {url}`; YouTube (`https://www.youtube.com/`), Spotify (`https://open.spotify.com/`) | The registry splits the command and puts the URL into one argument (DN-021) |
| Hub strip | A "Close" button (10 mm) while an app runs: `requestReturn()` | The fallback for any app: also for the mirroring receiver once its window has gone |
| Installer | `chromium`; the extension into `/usr/local/share/lexus-head-unit/browser-return/`; `lexus-browser` linked into `/usr/local/bin` | |

## Failure cases

| Case | Handling |
|---|---|
| This Chromium ignores `--load-extension` (Google turned it off in its branded builds in 2025; the Debian and Raspberry Pi builds are unbranded, and the feature switch turns it back on) | No Hub button on the page (A32). The way back is then the hub's own Close button, which the hub shows only when it is visible; the fallback after that is a labwc touch or key binding running `lexus-hub --send return`, decided on the Pi |
| A page puts something over the corner | The button is on the top layer (largest z-index) of the page; a full-screen video takes the button into its own element |
| Spotify's web player needs Widevine (protected audio) | If Chromium on the Pi has no Widevine module (OQ-27), the page says the browser is not supported. The fallback is a Spotify Connect receiver, `librespot`, which is not in Debian 13: it would come from the raspotify project's apt repository (outside Debian), so it is not installed without the owner's decision (see below) |
| The browser starts but shows a login | The owner logs in once, parked; the profile keeps it |
| The hub's app list is longer than eight | The grid scrolls (DN-021); the most used apps come first |

**Spotify Connect, if it is needed (not done).** Install, outside Debian: `curl -sL https://dtcooper.github.io/raspotify/install.sh | sh`, which adds the raspotify apt repository and installs `raspotify` (librespot as a systemd service); set `LIBRESPOT_NAME="Lexus Head Unit"` in `/etc/raspotify/conf`. The phone's Spotify app then plays to the Pi, the Pi plays to its default output (the car over Bluetooth, LHU-056), and the screen recording captures the sound. Spotify Premium is needed, which the owner has. This adds a third-party package source to the Pi, so it waits for the owner.

## Test plan

| Requirement | Test |
|---|---|
| REQ-031 | `tools/test_browser_apps.py`: the wrapper's command (Wayland, its own profile under the home folder, the extension path, the switch, full screen, the URL as one argument, even with spaces and quotes), `exec`, nothing created by `--print`; the manifest (version 3, a content script on every page, no permission); the button (116 px, top layer, shadow root, follows full screen, sends the close message) and the service worker (closes the sender's window); the registry entries and the installer paths |
| REQ-031 | `tests/hmi_hub/tst_hub_screen.qml`: the Close button appears while an app runs, is at least 10 mm, and stops the app |
| REQ-031 | On the Pi: checklist step 3.20 (YouTube with sound, the Hub button in a page and over a full-screen video, Spotify plays or not, OQ-27) |

## Alternative considered

A layer-shell overlay bar from Qt (`layer-shell-qt`) would sit above every window, including the mirroring receiver, and was the fallback named in DN-021. It needs a package that is not installed on the desk and a second always-running process; the extension needs neither, and the hub's Close button covers the cases a page button cannot.
