# TwoTrees SK1 Moonraker integration for Bambu Studio

## Goal

Add a third-party Klipper/Moonraker printer connection workflow for TwoTrees SK1 without changing Bambu Lab's existing device binding or networking plugin.

Target user environment: Windows 11 x64, Bambu Studio 2.8.2.61.

## Confirmed target devices

The user has two physically separate TwoTrees SK1 printers on the same LAN. They must be stored and displayed as two independent printer entries; identical model names must not cause the entries to overwrite or share state.

| Display name | Base URL | Device identity |
|---|---|---|
| TwoTrees SK1 (1) | http://192.168.68.54 | Separate Moonraker instance |
| TwoTrees SK1 (2) | http://192.168.68.53 | Separate Moonraker instance |

Both devices have been checked by the user through `GET /server/info`. The returned response from each reported:
- `klippy_connected: true`
- `klippy_state: "ready"`
- `failed_components: []`
- `moonraker_version: "v0.7.1-609-gbdd0222-dirty"`
- `api_version_string: "1.0.5"`

These are user-reported successful API checks, not a test performed by the build host. The application must re-test each configured endpoint at runtime and must not assume it remains reachable.

## User workflow

1. Select a compatible TwoTrees SK1 printer preset and slice as usual.
2. Open the Device workspace and choose a separate "Third-party / Moonraker" connection.
3. Add two independent saved printer entries, each with a user-editable display name and base URL.
4. Test each connection independently using `GET /server/info` and show its own Klipper/Moonraker state.
5. Upload the sliced G-code to the selected printer's Moonraker `server/files/upload` endpoint.
6. Only when the user explicitly presses Print, request print start (upload with `print=true`, or upload then call the print-start endpoint).
7. Poll Moonraker printer objects for state/progress; provide pause, resume and cancel actions.
8. Switching the selected printer must never redirect an upload or control command to the other printer.

## API contract

- Connection: `GET /server/info`
- Printer status: `GET /printer/objects/query?webhooks&print_stats&virtual_sdcard`
- Upload: multipart `POST /server/files/upload`, field `file`, optional `root=gcodes`
- Print start: `POST /printer/print/start` with JSON `{"filename":"<uploaded filename>" }`
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
3. Add a Device-workspace panel that is independent of Bambu Lab's DeviceWeb binding.
4. Wire upload/start/status/pause/resume/cancel to the selected printer only.
5. Build and test on Windows x64 against both real SK1 printers before distributing an installer.

## Repository architecture notes

The current source tree already has a DeviceWeb bridge and ViewModel registration mechanism in `src/slic3r/GUI/DeviceWeb`. The existing `DeviceManager` is Bambu-specific and uses Bambu access-code/network protocols; Moonraker devices must not be inserted into that Bambu machine model. A separate third-party ViewModel/service is the intended integration boundary. The existing DeviceWeb frontend build is driven by `src/slic3r/GUI/DeviceWeb/CMakeLists.txt`.

## Important scope note

This document records the confirmed target setup and integration design. It is not a claim that the Device UI, Moonraker client, print controls, or a tested Windows installer already exist. Native source changes must be implemented, compiled, and tested against the exact Bambu Studio source revision before a usable installer can be supplied.
