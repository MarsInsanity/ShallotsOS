# Game Boy link over USB: feasibility note

## Result

Gambatte is already ready for Game Boy/Game Boy Color link play in the shipped
TreeFrogUI build. PicoArch already exposes the core's **Emulator options**
screen, including `Game Link Mode`, the TCP port, and the server address.
There is no RetroArch netplay port to add for this: Gambatte implements the
Game Boy link cable itself over a TCP socket.

The missing part is a network connection between consoles. The stock
4.4.186-release kernel cannot provide one over USB-C/OTG without a kernel
change or matching, loadable kernel modules.

## What was verified locally

```mermaid
flowchart LR
    GB1[Gambatte: Game Link server] <-- TCP port 56400 --> NET[IP network]
    NET <-- USB Ethernet/RNDIS --> GB2[Gambatte: Game Link client]
```

- `cores/libretro-gambatte/gambatte_libretro.so` contains the Network Server,
  Network Client, Game Link Mode, and port `56400` strings.
- The source includes `NetSerial`, which uses TCP sockets (`socket`, `bind`,
  `listen`, `accept`, `connect`) for link transfers.
- PicoArch supports Libretro core options and exposes them as **Emulator
  options**, so it needs no Gambatte-specific frontend port.
- The exact stock kernel configuration has IPv4 enabled but deliberately lacks
  both halves needed for USB Ethernet:
  - host side: `CONFIG_USB_USBNET` is disabled;
  - gadget side: `CONFIG_USB_ETH`, `CONFIG_USB_CONFIGFS_ECM`, and
    `CONFIG_USB_CONFIGFS_RNDIS` are disabled.
- Every locally available stock/rootfs backup was checked. It supplies MTP,
  PTP, mass-storage and vendor iUM gadget modules, but no RNDIS/ECM/USB-net
  module that can be safely loaded into this kernel.

## Why MTP/OTG storage cannot be reused

MTP and USB storage are device-to-host transfer protocols, not an IP transport.
Two consoles connected directly cannot both be USB devices; one must host the
other. Even if that role issue were solved, the host has no USB-network driver
and the device has no Ethernet gadget function. Reusing the existing MTP module
would require inventing a paired host protocol and a relay process; it would not
be networked Gambatte link play and would be less reliable than adding the
proper kernel support.

## What a future implementation needs

1. A compatible stock-kernel module set or a kernel build with `usbnet`/
   `rndis_host` (host) and `usb_f_rndis` or ECM (gadget).
2. A proven USB-C OTG topology: one console host, one gadget, with an OTG
   adapter/cable that supplies the required role negotiation and power.
3. Small userspace setup scripts to assign fixed link-local addresses, for
   example server `192.168.7.1` and client `192.168.7.2`.
4. In PicoArch: set Gambatte's Game Link Mode to Network Server on one console;
   set Network Client and server address `192.168.7.1` on the other.

Until step 1 is available, exposing a TreeFrogUI menu entry would be misleading:
it could not create an interface or reach the other console on the unmodified
stock kernel.

## Wi-Fi-module hunt (2026-09-02)

HCSEMI's public sibling SDKs confirm that the vendor supports these USB Wi-Fi
chip families in its other products: **RTL8188EU**, **RTL8188FU**, **RTL8733BU**,
and **RTL8811CU**. The first two are common, inexpensive 2.4 GHz 802.11n nano
dongles and would be the sensible first target if a matching Linux driver is
found.

This is evidence for the hardware route, not a compatibility claim: those
public SDKs use HC's RTOS driver libraries, while TreeFrogUI boots a different
Linux 4.4.186 kernel. The stock images and all available local backups contain
none of the corresponding Linux `.ko` modules. Do not recommend or buy a
specific dongle until an ABI-matched `8188eu`/`8188fu` (or equivalent) Linux
module is recovered and loaded successfully.
