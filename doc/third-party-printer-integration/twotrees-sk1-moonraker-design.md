# TwoTrees SK1 Moonraker integration for Bambu Studio

## Goal

Add a third-party Klipper/Moonraker printer connection workflow for TwoTrees SK1 without changing Bambu Lab's existing device binding or networking plugin.

Target user environment: Windows 11 x64, Bambu Studio 2.8.2.61.

## User workflow

1. Select a compatible TwoTrees SK1 printer preset and slice as usual.
2. Open the Device workspace and choose a separate "Third-party / Moonraker" connection.
3. Enter the printer base URL (for example, `http://192.168.1.50`) and optional Moonraker API key.
4. Test connection and show printer state.
5. Upload the sliced G-code to Moonraker's `server/files/upload` endpoint.
6. Only when the user explicitly presses Print, request print start (Moonraker upload parameter `print=true`, or upload then call the print-start endpoint).
7. Poll Moonraker printer objects for state/progress; provide pause, resume and cancel actions.

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

## Implementation plan

1. Implement a reusable Moonraker HTTP client and unit tests.
2. Add a dedicated third-party printer connection model and settings persistence.
3. Add a Device-workspace panel that is independent of Bambu Lab's DeviceWeb binding.
4. Wire upload/start/status/pause/resume/cancel to the client.
5. Build and test on Windows x64 against a real SK1 before distributing an installer.

## Important scope note

This document is the integration design and implementation checklist, not a claim that the Device UI or a tested Windows installer already exists. The native Device-workspace work must be implemented and built against the exact Bambu Studio source revision corresponding to 2.8.2.61.
