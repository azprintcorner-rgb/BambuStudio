# TwoTrees SK1 printer workflow

This page describes the first native Moonraker workflow in the dedicated **TwoTrees SK1** tab.

## Printer profiles

The tab starts with two independent profiles:

- TwoTrees SK1 (1): `http://192.168.68.54:7125`
- TwoTrees SK1 (2): `http://192.168.68.53:7125`

Use **Configure** on a printer card to change its display name or Moonraker URL. The two profiles are stored independently in the Bambu Studio application configuration. The endpoint must be a plain HTTP URL with a host and optional port. HTTPS and IPv6 are not supported by this initial transport.

## Print workflow

1. Slice the model using a compatible TwoTrees SK1 / Klipper printer preset.
2. Export the sliced G-code to a local file.
3. Open **TwoTrees SK1** in Bambu Studio.
4. Check that the intended printer card reports Moonraker connected and Klipper ready.
5. On that same card, choose **Upload G-code** and select the exported file.
6. Press **Start** only after the upload has completed and the selected printer is ready.
7. Use that card's **Pause**, **Resume**, or **Stop** controls to manage the job.

Uploading a file does not start a print. Stop asks for confirmation. Each card sends commands only to the URL stored in its own profile.

## Status and limitations

- Status is refreshed every two seconds while the tab is visible, and can also be refreshed manually.
- The card displays Klipper state, print state, progress and available nozzle/bed temperatures.
- This initial integration uploads an exported G-code file; it does not yet send the in-memory sliced plate directly from the Slice/Print button.
- The initial HTTP transport does not support Moonraker API keys, HTTPS, WebSocket events, file listing, thumbnails, ETA, or camera streaming.
- Keep Moonraker on a trusted local network. Do not expose its unauthenticated HTTP endpoint to the public internet.

The integration must be compiled and verified against both physical printers before it is considered release-ready.
