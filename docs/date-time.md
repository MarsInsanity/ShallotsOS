# Date and Time (Emulation)

TreeFrogUI provides date and time controls for handhelds that do not have a
battery-backed real-time clock (RTC). The clock runs while the handheld is on,
and TreeFrogUI saves its last known time to the SD card.

**Hardware limitation:** these devices have no RTC hardware to keep time while
powered off. The saved clock cannot tell how long the handheld was off. A boot
bump can estimate that lost time, or you can sync from a USB drive when you
know the correct time.

## Set the date and time

1. Open **Settings** and expand **DATE/TIME (EMULATION)**. It is above
   **SYSTEM** and collapsed by default.
2. Select **Set Date & Time** and press **A**.
3. Use **Left/Right** to select a field and **Up/Down** to change it.
4. Press **A** to save or **B** to cancel.

The clock is saved in `cubegm/time_save.txt` on the SD card. TreeFrogUI updates
that saved time as it returns from the menu and games.

## Sync from USB

Put a plain text file named `time_save.txt` in the root of a USB drive. The
handheld must mount the drive at `/media/hdd`; see the
[USB-host storage guide](dev/usb-host-storage.md). The file must contain one
date and time in this format:

```text
2026-10-29 14:30:00
```

Connect the drive, open **Settings → DATE/TIME (EMULATION)**, and select
**Sync Time from USB**. This applies the timestamp in the file and copies it to
the SD card's saved clock file. Sync is manual; TreeFrogUI does not poll USB for
time in the background.

## Clock Bump per Reboot

**Clock Bump per Reboot (min)** adds the selected number of minutes each time
TreeFrogUI starts. It restores the saved time, advances it by this amount, and
saves the advanced time. **0** disables the bump. Values are set in 5-minute
steps, up to 1440 minutes.

Choose an estimate for how long the handheld is usually powered off between
boots. This is only an approximation: the device cannot measure the actual
time while powered off without RTC hardware. Sync from USB or set the clock
manually whenever you need the correct time.
