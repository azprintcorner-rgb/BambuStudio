# TwoTrees SK1 Moonraker integration for Bambu Studio

## Goal

Add a third-party Klipper/Moonraker printer connection workflow for TwoTrees SK1 without changing Bambu Lab's existing device binding or networking plugin.

Target user environment: Windows 11 x64, Bambu Studio 2.8.2.61.

## Confirmed target devices

The user has two physically separate TwoTrees SK1 printers on the same LAN. They must be stored and displayed as two independent printer entries; identical model names must not cause the entries to overwrite or share state.

| Display name | Base URL | Device identity |
|---|---|---|
| TwoTrees SK1 (1) | http://192.168.68.54:7125 | Separate Moonraker instance |
| TwoTrees SK1 (2) | http://192.168.68.53:7125 | Separate Moonraker instance |

Both devices have been checked by the user through `GET /server/info`. The returned response from each reported:
- `klippy_connected: true`
- `klippy_state: "ready"`
- `failed_components: []`
- `moonraker_version: "v0.7.1-609-gbdd0222-dirty"`
- `api_version_string: "1.0.5"`

These are user-reported successful API checks, not a test performed by the build host. The application must re-test each configured endpoint at runtime and must not assume it remains reachable.

## User workflow

1. Select a compatible TwoTrees SK1 printer preset and slice as usual.
2. Open the dedicated "TwoTrees SK1" workspace tab.
3. Keep both configured printer entries visible with independent status and controls.
4. Test each connection independently using `GET /server/info` and query printer status.
5. Upload a G-code file to the chosen printer's Moonraker `server/files/upload` endpoint.
6. Only when the user explicitly presses Start, request print start through `POST /printer/print/start?filename=<URL-encoded filename>`.
7. Refresh printer state on demand and provide pause, resume and stop actions.
8. Every operation is bound to the card's own immutable printer profile; the other printer's address is never used.

## API contract

- Connection: `GET /server/info`
- Printer status: `GET /printer/objects/query?webhooks&print_stats&virtual_sdcard&heater_bed&extruder`
- Upload: multipart `POST /server/files/upload`, field `file`, optional `root=gcodes`
- Print start: `POST /printer/print/start?filename=<URL-encoded uploaded filename>`
- Pause/resume/cancel: `POST /printer/print/pause`, `/printer/print/resume`, `/printer/print/cancel`

Moonraker API reference: https://moonraker.readthedocs.io/en/latest/web_api/

## Security and UX requirements

- Never assume a printer is reachable just because an IP was entered; call `/server/info`.
- Keep API keys in the OS credential store or the application's secure settings facility; do not write secrets to logs or project files.
- Warn that the printer and workstation should be on a trusted LAN. Do not expose Moonraker directly to the public internet.
- Validate URL scheme and reject arbitrary local-file paths.
- Upload and print are separate user-visible actions; never auto-start a print after merely testing a connection.
- Do not send Bambu-specific commands or use Bambu cloud/LAN access-code authentication for Moonraker devices.
- Store each printer's URL, display name, credentials, connection state, and selected-device ID independently.
- Treat the two IP addresses as separate devices even though both report the same printer model and Moonraker version.

## Implementation plan

1. Implement a reusable Moonraker HTTP client and unit tests.
2. Add a dedicated third-party printer connection model and settings persistence, supporting multiple entries.
3. Add a native TwoTrees SK1 workspace panel independent of Bambu Lab's DeviceManager and DeviceWeb bindings.
4. Wire upload/start/status/pause/resume/cancel to each printer card independently.
5. Build and test on Windows x64 against both real SK1 printers before distributing an installer.

## Repository architecture notes

The current source tree already has a DeviceWeb bridge and ViewModel registration mechanism in `src/slic3r/GUI/DeviceWeb`, but the first implementation uses a separate native wxWidgets panel and service instead. The existing `DeviceManager` is Bambu-specific and uses Bambu access-code/network protocols; Moonraker devices are not inserted into that Bambu machine model. The panel is registered as a separate MainFrame tab and owns two independent printer profiles.

## Important scope note

This document records the confirmed target setup and integration design. It is not a claim that the Device UI, Moonraker client, print controls, or a tested Windows installer already exist. Native source changes must be implemented, compiled, and tested against the exact Bambu Studio source revision before a usable installer can be supplied.


## Implementation progress (2026-11-13)

- Added `MoonrakerPrinterConfig` with two default TwoTrees SK1 profiles, host/port validation, JSON serialization and persistent settings in the application config. Each card can independently edit its display name and Moonraker URL.
- Added a reusable synchronous Boost.Beast HTTP client for server info, printer status, G-code upload, print start, pause, resume and cancel. Callers must execute it off the GUI thread. Current transport supports plain HTTP only on Moonraker default port 7125 (or an explicit custom port); HTTPS is rejected explicitly.
- Added a native "TwoTrees SK1" tab with two independent printer cards, startup status checks, 2-second status polling while the tab is visible, manual refresh, per-printer configuration, G-code upload, print state/progress/temperature display and explicit print controls. Upload does not start a print; Stop asks for confirmation. Upload/Start/Pause/Resume/Stop are enabled according to Klipper readiness and the reported print state, and a printer profile cannot be edited while its print is active.
- Registered the new sources in the DeviceWeb CMake source list and added the tab to MainFrame.
- The native source has not yet been compiled, unit-tested, or run against either physical printer. Native compilation, runtime verification against both printers, and a Windows installer remain outstanding.
