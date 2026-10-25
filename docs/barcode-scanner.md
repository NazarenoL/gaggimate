# Waveshare Barcode Scanner Module (D)

The round **Module (D)** works with this firmware using its UART output. It reads
QR and common retail barcodes; additional supported formats can be enabled on
the scanner. Waveshare specifies **3.3 V power, about 120 mA**, and a six-pin
connector. See the [official Module D pinout and specifications](https://www.waveshare.com/wiki/Barcode_Scanner_Module_%28D%29).

## Connection to the controller

The scanner connects to the **controller board's expansion header**, leaving
the LilyGo display wiring alone. The controller forwards decoded text over its
existing BLE link to the display, which stores the bean library and shows the
picker. No Bluetooth capability or USB adapter is needed on the scanner.

Unplug the espresso machine from mains and disconnect USB power before accessing
the controller and wiring the scanner. Use the module connector's documented pin numbering, not
wire colors or an assumed left/right orientation.

| Module D pin | Connect to controller | Purpose                                 |
| ------------ | --------------------- | --------------------------------------- |
| 1 — GND      | Expansion GND         | Shared ground                           |
| 2 — RX       | Leave disconnected    | Firmware does not send scanner commands |
| 3 — TX       | Expansion EXT4        | Decoded barcode data                    |
| 4 — VCC      | Expansion **3.3 V**   | Power; do not use the adjacent 5 V pins |
| 5, 6 — NC    | Leave disconnected    | Reserved                                |

On the **Standard/Pro PCB drawing in this repository**, the ten-pin expansion
header is **J6**: pin **3 = 3.3 V**, pin **5 = GPIO12 / EXT4**, and pins **9/10 =
GND**. Pins **1/2 are 5 V**. Count from the PCB's pin-1 marker and check against
the connector labels/schematic for your actual board revision.

The firmware uses the detected controller board's EXT4 mapping automatically:
Standard/Pro uses GPIO12; Max uses GPIO43. The J6 numbering above is for the
Standard/Pro drawing, not a Max pinout. EXT4 must be unused by other add-ons.
The 3.3 V rail must have capacity for the scanner's additional current.

## Firmware

Install **both controller and display firmware from this source revision**.
Controller firmware now reads Module D on EXT4 at 9600 baud, 8N1, receive-only;
the matching display firmware accepts controller scans. There is no scanner
GPIO build flag to set for this connection. UART1 is reserved on the controller;
USB debug serial is unchanged.

Build the embedded web bundle first, then both firmware targets:

```sh
scripts/build_webui.sh
pio run -e controller -e display
```

Use the existing installation/update procedure for each board; the scanner does
not require a filesystem erase or `uploadfs`. For a first USB installation,
connect each board individually and use its actual serial port:

```sh
pio run -e controller -t upload --upload-port <controller-port>
pio run -e display -t upload --upload-port <display-port>
```

The display target is the repository's LilyGo-T-RGB configuration. A different
LilyGo panel requires its matching build configuration.

## Scanner configuration

A factory-configured Module D already sends plain data at **9600 baud, 8N1**,
with a carriage-return terminator. Unlike some other Waveshare models, Module D
is UART-only. Keep those serial settings.

Use the QR configuration codes in the [Module D setup manual](https://files.waveshare.com/wiki/Barcode-%20Scanner-Module-%28D%29/barcode-scanner-module-%28D%29-setup-manual.pdf):

1. Keep the machine in standby while configuring the scanner. Enable its
   configuration function if previously disabled (printed page 5).
2. Enable **all supported code types** if you want the widest format coverage
   (printed page 21). This cannot add formats the hardware does not support.
3. For automatic bag scanning, select sensing mode (printed page 11). Factory
   continuous mode also works; the firmware filters repeated reads of a held bag.
4. Preserve the complete payload: no custom prefix/suffix or Code ID, no data
   truncation, no read-failure output, plain data output, and a CR tail. The
   relevant configuration codes are on printed pages 15–21.
5. Disable the configuration function when finished (printed page 5).

The repeat filter requires **1.5 seconds with no read of the same code** before
that code is accepted again. Different codes are accepted immediately. This
works with Module D's factory one-second repeat interval; if you change that
interval, use sensing mode to avoid repeated selections. Remove the bag briefly
before intentionally scanning its same code again.

## First use

1. Wake the machine and add your bean to the library in the web UI.
2. On the display, open **Beans**, choose that bean and tap **Scan new code**.
3. Present the bag's barcode or QR code. **Code saved** confirms registration.
4. Tap the single check to finish. Later scans of that code select the bean
   immediately, including beans outside the recent ten.

To link another code to the same bean, tap **Scan new code** again. If an unknown
code opened the picker before registration was armed, remove the bag for at
least 1.5 seconds, choose the bean, arm registration and present it again.

If the scanner lights up but nothing changes, check that the machine is awake
and idle, both firmware versions include scanner support, TX is connected to
EXT4, the ground is shared, and output is plain data with a CR terminator. Its
LED indicates optical reading, not that the bean code is registered.

## Matching and limits

- A bean can have **16 codes**, each **1–512 bytes**. Leading zeros, case, spaces,
  UTF-8 text and QR URLs are preserved. A QR URL is an identifier; it is not
  opened or fetched.
- Embedded CR, LF and NUL are unsupported by the line-based transport. Use a
  single-line payload or your own sticker for such QR codes. Frames exceeding
  512 bytes or interrupted by a gap over 500 ms are discarded through the next
  line terminator, so a truncated suffix cannot select a bean.
- A code belongs to one bean. Retail EAN/UPC codes often identify a product
  rather than an individual bag; use your own unique stickers if you keep
  separate entries for bags sharing the same retail code.
- The controller discards scans while the display has scanning disabled. Replies
  carry the current scanning session; the display rejects delayed replies from an
  earlier session after standby, a brew, an update or a reconnect. Mode changes
  also cancel pending registration. The scanner remains powered and its LEDs
  may stay on in standby; it cannot wake the machine.
- Manage linked codes under **Beans** in the web UI. Older clients that edit
  only the name and roaster preserve the stored codes.

## Validation

`pio test -e native -f test_barcode_reader` checks framing, EAN and QR payloads,
UTF-8, size limits, invalid frames, timeouts and timer wraparound.

After building `display-sim`, run `scripts/test_display_beans.sh` for the real
library and picker checks, including registration, multiple codes, duplicates,
legacy edits, scanning an older bean, standby and active-process rejection.
The checks cover both injected scans and the controller-to-display callback,
including replies delayed across standby. Native tests also check continuous
scan filtering and reassembly of a full 512-byte code from BLE fragments.
Physical wiring and optical scanning still need testing on the actual machine.
