# Battery & Power Guidelines

> SomnoTrace is designed to run on **stable USB-C power**. The internal battery is a safety net, not a primary power source.

**Always connect USB-C power** during overnight therapy recording. The battery exists for **power outage protection only** — if your electricity drops mid-session, the battery keeps the device alive long enough to finish writing data and shut down safely.

**Running an entire night on battery is strongly discouraged** and may result in data loss. If the battery dies mid-session, the current recording may be incomplete or corrupted. SomnoTrace does its best to flush and close files on low battery, but a sudden power loss during active Bluetooth streaming can still lose the last few seconds of data.

## Battery life (emergency use only)

| Screen Brightness | Approximate Runtime |
|---|---|
| Medium brightness | 2–3 hours |
| LCD screen off | Up to 11 hours |

These figures are **not** a recommendation to run unplugged overnight.

## Battery Calibration & Why You Might See `--% ⚡` ("Calibrating")

If your screen displays **`--% ⚡`** (or the Web Portal Status shows **`--% ⚡ (Calibrating)`**):

- **If you do NOT have a battery installed (running on USB-C power only):**
  The board's charging pins float at ~4.2 V, causing the firmware to assume an uncalibrated battery is present.
  👉 **Turn off the indicator:** Open the Web Portal, navigate to **Settings → Device Settings**, and toggle **Battery Indicator** to **OFF**. This cleanly hides the battery gauge from the LCD and sets the status to `USB Power (No battery)`.

- **If you DO have a battery installed:**
  SomnoTrace estimates battery percentage from cell voltage rather than a dedicated fuel-gauge chip. When booting or flashing firmware while plugged into USB-C with a high battery charge ($\ge 4.10\text{ V}$), the charger chip actively floats the cell at ~4.2 V. The firmware honestly indicates `--%` rather than guessing a false percentage.
  - **How to calibrate (takes 10 seconds):**
    1. **Unplug the USB-C cable** for 10–15 seconds.
    2. Operating briefly on battery allows the electrochemical surface charge to relax to genuine open-circuit voltage; SomnoTrace will immediately snap to the true percentage.
    3. Once calibrated, the percentage is stored in RTC memory across reboots, and you can plug USB-C back in.
    - *(Alternatively, simply leave it plugged in until the battery reaches 100% full charge; calibration engages automatically upon charge completion).*
