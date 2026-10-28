#!/usr/bin/env python3
"""Run commands on a ShallotsOS handheld through its USB debug shell.

    python tools/shallots_serial.py COM5 "free" "dmesg | tail -30"
    python tools/shallots_serial.py /dev/ttyACM0 --file cmds.txt
    python tools/shallots_serial.py COM5 --interactive

Turn on Settings > System > USB Debug Shell on the console and connect it by
USB-C; Windows shows "USB Serial Device (COMn)" in Device Manager > Ports.
Each command's output is printed as it finishes (a marker echoed after the
command tells where it ends), so a script or an agent can read results
without a terminal program. Needs pyserial (pip install pyserial).
"""
import argparse
import sys
import time
import uuid

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: pip install pyserial")

PROMPT_WAIT = 3.0


def read_until(port, token, timeout):
    """Read until `token` appears or `timeout` seconds pass; returns the text."""
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        chunk = port.read(4096)
        if chunk:
            buf += chunk
            if token.encode() in buf:
                break
        else:
            time.sleep(0.02)
    return buf.decode("utf-8", "replace")


def run(port, command, timeout):
    """Run one command; return its output (without the echo or the marker)."""
    marker = "__SHALLOTS_DONE_%s__" % uuid.uuid4().hex[:8]
    # Splitting the marker keeps the echoed command line from matching it.
    half = len(marker) // 2
    line = "%s; echo; echo '%s''%s' $?\n" % (command, marker[:half], marker[half:])
    port.reset_input_buffer()
    port.write(line.encode())
    out = read_until(port, marker, timeout)
    lines = out.replace("\r", "").split("\n")
    body, status = [], None
    for i, l in enumerate(lines):
        if marker in l:
            parts = l.split()
            status = parts[-1] if parts and parts[-1].isdigit() else None
            break
        if i == 0 and command[:20] in l:   # the shell's echo of the command
            continue
        body.append(l)
    while body and not body[-1].strip():
        body.pop()
    if status is None:
        body.append("[no end marker within %ss; output may be incomplete]" % timeout)
    return "\n".join(body), status


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="COM5 on Windows, /dev/ttyACM0 on Linux")
    ap.add_argument("commands", nargs="*")
    ap.add_argument("--file", help="read commands from a file, one per line")
    ap.add_argument("--timeout", type=float, default=30.0, help="seconds per command")
    ap.add_argument("--interactive", action="store_true", help="read commands from stdin")
    a = ap.parse_args()

    port = serial.Serial(a.port, 115200, timeout=0.1)
    port.write(b"\n")                 # wake the shell; the banner and prompt follow
    read_until(port, "# ", PROMPT_WAIT)
    port.write(b"stty -echo 2>/dev/null; export PS1='# '\n")
    read_until(port, "# ", PROMPT_WAIT)

    cmds = list(a.commands)
    if a.file:
        with open(a.file, encoding="utf-8") as f:
            cmds += [l.rstrip("\n") for l in f if l.strip() and not l.startswith("#")]
    if a.interactive:
        print("Connected to %s. Type commands; Ctrl+D or 'quit' ends." % a.port, file=sys.stderr)
        for l in sys.stdin:
            l = l.strip()
            if l in ("quit", "exit"):
                break
            if l:
                out, st = run(port, l, a.timeout)
                print(out)
                if st not in (None, "0"):
                    print("[exit status %s]" % st, file=sys.stderr)
    for c in cmds:
        out, st = run(port, c, a.timeout)
        print("$ %s" % c)
        print(out)
        if st not in (None, "0"):
            print("[exit status %s]" % st)
    port.write(b"stty echo 2>/dev/null\n")
    port.close()


if __name__ == "__main__":
    main()
