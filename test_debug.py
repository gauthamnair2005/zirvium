#!/usr/bin/env python3
"""Debug serial test — minimal interaction, see what's happening."""

import pexpect
import sys
import os
import subprocess
from pathlib import Path

BUILD_DIR = Path(__file__).parent / "build"
KERNEL_ISO = BUILD_DIR / "zirvium.iso"
BLANK_IMG = BUILD_DIR / "blank.img"
TIMEOUT = 10
BOOT_TIMEOUT = 30
PROMPT = '\u20b9 '

def build():
    subprocess.run(["make", "iso"], cwd=os.path.dirname(__file__),
                   capture_output=True)

def spawn():
    if not BLANK_IMG.exists():
        subprocess.run(["dd", "if=/dev/zero", f"of={BLANK_IMG}", "bs=1M", "count=10"])
    cmd = [
        "qemu-system-x86_64",
        "-cdrom", str(KERNEL_ISO),
        "-drive", f"file={BLANK_IMG},format=raw,if=ide",
        "-drive", "file=/dev/zero,format=raw,if=none,id=vdisk0",
        "-device", "virtio-blk-pci,drive=vdisk0",
        "-netdev", "user,id=net0", "-device", "e1000,netdev=net0",
        "-netdev", "user,id=net1", "-device", "virtio-net-pci,netdev=net1",
        "-netdev", "user,id=net2", "-device", "rtl8139,netdev=net2",
        "-boot", "d", "-m", "512M", "-no-reboot", "-nographic",
    ]
    c = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace',
                     timeout=TIMEOUT)
    return c

c = spawn()
print("Spawned PID", c.pid, flush=True)

# Wait for first prompt (boot+banner)
print("Waiting for boot...", flush=True)
idx = c.expect_exact([PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=BOOT_TIMEOUT)
print("Boot index:", idx, flush=True)
if idx != 0:
    print("BEFORE:", repr(c.before[-200:] if len(c.before)>200 else c.before), flush=True)
    print("BUFFER:", repr(c.buffer[-200:] if len(str(c.buffer))>200 else c.buffer), flush=True)
    c.close()
    sys.exit(1)

print("Ready! Sending 'hello'...", flush=True)
c.sendline("hello")
idx = c.expect_exact([PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=TIMEOUT)
print("After hello, index:", idx, flush=True)
print("BEFORE:", repr(c.before), flush=True)
print("AFTER:", repr(c.after), flush=True)

if idx == 0:
    print("Hello PASSED!", flush=True)
    print("Sending 'uname'...", flush=True)
    c.sendline("uname")
    idx = c.expect_exact([PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=TIMEOUT)
    print("After uname, index:", idx, flush=True)
    print("BEFORE:", repr(c.before), flush=True)

c.sendline("shutdown")
try:
    c.expect(pexpect.EOF, timeout=5)
except:
    pass
c.close()
