# USB debug shell

A root shell on the running console over USB, for live debugging: the
console appears on a PC as a USB serial port (CDC ACM) while the menu and
games keep running.

- Turn it on in Settings > System > USB Debug Shell (`usb_debug=on` in
  `frogui/settings.txt`; off by default, since anything plugged in gets a root
  shell). It starts at once and on every boot.
- Windows 10/11: Device Manager > Ports (COM & LPT) shows "USB Serial
  Device (COMn)". Open it in PuTTY (Connection type Serial, that COM port,
  any speed) and press Enter. Linux: `/dev/ttyACM0`; macOS:
  `/dev/tty.usbmodem*`.
- In the shell: `logs` follows `log.txt` (create an empty `/mnt/sdcard/log.txt`
  and restart for the detailed log), `ls cubegm/logs`, `dmesg`, `top`.
- USB mode (MTP / storage) stops it first; it returns with the next boot.
- Log: `cubegm/logs/usb_debug.log`.

The modules (`cubegm/modules/4.4.186-release/u_serial.ko`, `usb_f_acm.ko`)
are built by `scripts/build_usb_serial_modules.sh` from kernel.org Linux
4.4.186 with the recipe TreeFrogUI used for `usb_f_mass_storage.ko`
(apps/usb_mode/README.md); the build checks their vermagic against that
module's.
