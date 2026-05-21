#!/usr/bin/env python3
"""Quick boot test: does the kernel boot and show a prompt?"""

import pexpect, sys, subprocess
from pathlib import Path

BUILD_DIR = Path(__file__).parent / "build"
KERNEL_ISO = BUILD_DIR / "zirvium.iso"
BLANK_IMG = BUILD_DIR / "blank.img"

subprocess.run(["make", "iso"], cwd=Path(__file__).parent, capture_output=True)

if not BLANK_IMG.exists():
    subprocess.run(["dd", "if=/dev/zero", f"of={BLANK_IMG}", "bs=1M", "count=10"])

cmd = [
    "qemu-system-x86_64", "-cdrom", str(KERNEL_ISO),
    "-drive", f"file={BLANK_IMG},format=raw,if=ide",
    "-boot", "d", "-m", "512M", "-no-reboot", "-nographic",
]
c = pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=30)
idx = c.expect_exact(['\u20b9 ', pexpect.TIMEOUT, pexpect.EOF], timeout=30)
print(f"Index: {idx}")
print(f"BEFORE: {c.before[-500:]}")
if idx == 0:
    print("BOOT OK")
    c.sendline("shutdown")
    c.expect(pexpect.EOF, timeout=10)
else:
    print("BOOT FAILED")
c.close()
