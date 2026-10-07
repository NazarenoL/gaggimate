# Custom standby photos and fork OTA

## Using photos

After installing this build, open the machine's web UI → **Settings → General → Standby photo**. Choose a JPG, PNG or WebP, review the circular preview and press **Save photo**. The image updates without a reboot. **Remove photo** returns to a plain standby background; the GaggiMate logo is not used as a fallback. Enable the standby display and choose a nonzero brightness to see photos.

The clock, touch-to-wake gesture, connection indicators and error/update UI remain. No custom controller firmware is needed for this feature: it changes only the display firmware and its embedded web UI.

The browser resizes the image to fit within 480 × 480, pads with black, and converts it to little-endian RGB565. The circular panel clips the corners; keep the subject near the center. The browser accepts originals up to 20 MB. The display stores one photo at `/standby.rgb565` in LittleFS (460,800 bytes). A replacement also temporarily needs that much free filesystem space. Upload errors keep the previous photo. There is no slideshow or multi-photo library in this implementation.

The photo survives reboot and firmware OTA, which does not overwrite LittleFS in this revision. A filesystem flash or full flash erase will remove it, along with other data stored there. The included `docs/examples/corgi.jpg` is a test image; its attribution is in `docs/examples/README.md`. It is not automatically installed on the device.

If **Save photo** makes the display restart and the browser eventually reports **Failed to fetch**, install a display build containing the background photo-save fix. Earlier builds wrote the whole photo inside the AsyncTCP callback, where a slow flash write can exceed the network task watchdog timeout. Saves now run in a separate task with smaller writes that yield between chunks. Updating the controller alone does not change this code. A serial crash log or `/api/core-dump` download can help confirm the reset cause.

## Can stock firmware install a fork over OTA?

**The updater in this source revision cannot bootstrap custom display firmware through its existing web UI.** `WebUIPlugin.h` originally hardcoded `https://github.com/jniebuhr/gaggimate/releases/`. `handleOTASettings()` accepts only stable (`latest`) or `nightly`; other values are coerced to nightly. There is no user-supplied release URL, local firmware-upload HTTP endpoint, or ArduinoOTA service. The BLE DFU service is on the controller, not the display. Merely uploading a binary to this fork or enabling its Actions will not make stock display firmware download it.

For a machine using that stock updater, install this display firmware once over USB. Afterward, this fork uses **NazarenoL/gaggimate's own GitHub releases** for OTA. This requires neither an upstream PR nor publication to the official update servers. If your installed firmware differs from this revision and already exposes a custom binary/URL upload feature, check that version before assuming USB is mandatory.

`RELEASE_URL` in this fork is now `https://github.com/NazarenoL/gaggimate/releases/`. The existing Stable channel uses the fork's latest published release; Nightly uses the fork's `nightly` release. Existing channel settings are retained: select **Stable** to use releases from the new custom workflow.

## Build and first USB install

This configuration targets the **LilyGo-T-RGB, 16 MB flash / 8 MB PSRAM**, as specified in `boards/LilyGo-T-RGB.json`. Confirm your board and partition layout before flashing. Other displays need their corresponding build configuration; this binary is not a headless build.

Use Node 22 and PlatformIO. The fork has no inherited version tags, so add a local version tag before building (choose a new version):

```sh
git tag v1.9.1-photos1
scripts/build_webui.sh
pio run -e display
```

The web bundle must be built **before** the firmware, because it is embedded in the application. Connect the display by USB and run:

```sh
pio run -e display -t upload --upload-port /dev/ttyACM0
```

Replace the port with your actual display port. This uses PlatformIO's bootloader/partition/application upload for the configured board; it does not run `uploadfs` or intentionally erase LittleFS. If the existing partition layout differs, back up profiles/settings first: a layout migration can make existing filesystem data inaccessible. Do not flash the corgi or a fresh filesystem image using `uploadfs` for an ordinary upgrade. The controller can stay on its existing firmware only if it uses the same communication protocol (currently protocol 6). If the display reports **Version mismatch, update controller**, install the matching controller build through Settings → System → Stable → Save Channel & Refresh → Update Controller after publishing a fork release. OTA recovery remains available during a protocol mismatch.

## Publish subsequent OTA builds in your fork

The new **Build custom firmware** workflow (`.github/workflows/custom-firmware.yml`) builds automatically on pushes to `master` and pull requests targeting it, and can also run manually. Automatic builds upload downloadable firmware artifacts without publishing an OTA release. Manual runs with a version tag publish a release and need only the fork's standard `GITHUB_TOKEN`, with contents-write permission. It does not require `UPDATE_SERVER_HOST` or `UPDATE_SERVER_API_KEY`, unlike the inherited official deployment workflows.

1. Put the workflow and changes on your fork's default branch and enable GitHub Actions if necessary.
2. In Actions, select **Build custom firmware → Run workflow**. Enter a new tag such as `v1.9.2-custom1` to publish an OTA release (leave blank to build artifacts only), higher than the version installed on the machine. Use a distinct, unused tag each time. Avoid dots within the prerelease suffix because this firmware's version parser does not retain them.
3. The workflow builds the embedded web UI, display and unchanged controller sources, archives the binaries and publishes a release in **this fork**, marked latest.
4. In the machine's web UI, select **Settings → System → Stable** and check for updates. Choose **Update Display** for this feature. If the display reports a protocol version mismatch, choose **Update Controller** to install the matching controller build; the existing display supports OTA recovery in this state.

OTA downloads `display-firmware.bin` (or `board-firmware.bin` for controller updates). It installs only versions considered newer by `lib/OTA/src/common.cpp`; rebuilding under the same version will not offer an update. Nightly fallback additionally reads `version.txt`. GitHub-hosted assets are downloaded over HTTPS, so the machine needs internet access; private repository assets are not supported by the current unauthenticated downloader.

OTA cannot change the bootloader or partition table. USB is still needed for initial blank hardware, incompatible partition migrations and recovery when the current firmware cannot boot or reach the network.

## Verification

- Display firmware and `display-sim` compile.
- Production web UI builds; the changed web files pass ESLint.
- `node --test web/test/standbyPhoto.test.js` verifies RGB565 primary colors, byte order, buffer length and invalid input rejection.
- Real browser against the simulator: uploaded the included corgi through the file picker and Save photo control; checked preview after reload, exact stored bytes, photo replacement and removal.
- Empty, short and oversized API uploads were rejected without changing the saved photo.
- Restarted the simulator and captured the real LVGL standby screen showing the corgi, clock, status icons and wake indicator.

A physical display and OTA installation were not available for testing. GitHub build results and firmware artifacts are available under Actions → Build custom firmware.
