# Camping Mode (Offline Use)

SomnoTrace can record therapy data with **no Wi-Fi or internet connection at all** — useful for camping, travel, or during internet outages. Here's how it works and what you need to know.

## Prerequisites

Camping mode is **not available out of the box**. The following must be true:

1. **At least one previous online session completed.** SomnoTrace needs to have previously connected to Wi-Fi, synced its clock via NTP, and recorded at least one therapy session with the AirSense 11. This establishes a "clock drift" reference that allows accurate timekeeping without internet.
2. **The AirSense 11 must be paired.** The Bluetooth pairing happens during normal setup — once paired, the bond persists across reboots.

## How to Use It

1. **Turn on your AirSense 11 first**, and wait for it to be ready (screen on, not in a startup/error state).
2. **Then power on SomnoTrace** (plug in USB-C or hold the POWER button for 2 seconds).
3. SomnoTrace will detect that Wi-Fi is unavailable, connect to the AirSense 11 over Bluetooth, and estimate the current time using the stored clock drift.
4. The screen will show an **"Estimated time"** notice — this is normal. Recording proceeds as usual.
5. Therapy data is saved to the MicroSD card. If you have a local NAS/SMB share on a network without internet, uploads to that share will still work.
6. SleepHQ cloud uploads are queued and will upload automatically once internet connectivity is restored.

## Important Notes

- **Always power on the AirSense 11 before SomnoTrace.** SomnoTrace waits up to 30 seconds for the AirSense 11 to connect over Bluetooth at boot. If the AS11 isn't ready in time, the device will retry a few times and may eventually enter Wi-Fi setup mode.
- **The estimated time may drift slightly** over long offline periods, since it's based on the AS11's internal clock plus a previously measured offset. The longer since the last NTP sync, the less precise the timestamp.
- **No data is lost.** Everything recorded during camping mode is stored on the MicroSD card and will upload to SleepHQ once you're back online.
