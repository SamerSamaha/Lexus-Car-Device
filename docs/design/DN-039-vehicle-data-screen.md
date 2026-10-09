# DN-039: Vehicle-data screen, 4 x 2 signal grid

| | |
|---|---|
| Ticket | LHU-039 |
| Requirements | REQ-012, REQ-006 |
| Author | implementer (build-out form, D-049) |
| Status | Approved |
| Form | Light (one page: problem, public interface, failure cases, test plan) |
| Draft written | 2026-10-08, about 10 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The home screen shows two values; the vehicle-data screen shows all eight signals of REQ-004 as a 4 x 2 grid of tiles, each with name, value, unit and status (REQ-012), with the same Stale and NeverReceived styling as the home screen (REQ-006), and a way back. The screens are switched by the application window, not by the screens themselves.

Sizes on the 110.4 x 62.1 mm panel: the status strip grows to 10 mm so that it can carry the Home touch target; four tiles across with 2 mm gutters are 25 mm wide; two rows below the strip are 23 mm tall. The plan's "about 27 x 27 mm" becomes 25 x 23 mm because the strip holds the navigation; still well above the 10 mm touch rule and large enough for a `mm(5.7)` value.

## 2. Public interface

| Element | Interface | Notes |
|---|---|---|
| `VehicleDataScreen.qml` | `required property var vehicleData`; `signal homeRequested()` | `Repeater` over `vehicleData.tiles` into a `Grid` with 4 columns; `objectName` `tile0` ... `tile7` in id order; the strip's navigation button says `Home` |
| `StatusStrip.qml` | new `property string navigationText` (empty hides the button) and `signal navigationRequested()` | Button `objectName` `navigationButton`, at least 10 mm on each side; strip height `Sizes.statusStripHeight` is now `mm(10)` |
| `HomeScreen.qml` | unchanged signal `vehicleDataRequested()`; the bottom button is kept | |
| `Screens.qml` | `required property var vehicleData`; `property bool vehicleDataShown`; shows `HomeScreen` or `VehicleDataScreen` | The switch lives here so that it is testable without a `Window`; `Main.qml` embeds it |
| `SignalTile.qml` | `property int valuePixelSize` already exists; the grid passes `Sizes.gridValuePixelSize` (`mm(5.7)`) | Same styling rules as DN-013 |

## 3. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | More or fewer than eight tiles in `vehicleData.tiles` | The `Grid` lays out whatever it gets; the test asserts exactly eight and the view-model test already pins the count |
| 2 | A tile narrower than its value text | The value scales with `fontSizeMode: Text.Fit` down to `Sizes.primaryValueMinimumPixelSize` (`mm(4)`), never below the 4 mm rule |
| 3 | Navigation signal with no handler | Nothing happens; `Screens.qml` is the only handler and `Main.qml` embeds it |

## 4. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `tst_vehicle_data_screen.qml`: eight tiles, one per signal in id order, each showing the definition name and unit; Valid shows the value in the live colour; Stale keeps the value, shows the badge, never the live colour; NeverReceived shows `--`; for all eight signals | REQ-012, REQ-006 |
| 2 | Same file: each tile at least `mm(22)` wide and `mm(20)` tall, the Home button at least `mm(10)` on each side; the value font at least `mm(4)` | REQ-012 |
| 3 | `tst_screens.qml`: Home is shown first; the Vehicle data button shows the grid; the Home button returns | REQ-012 |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Added after the code pull request is merged.
