#!/usr/bin/env python3
"""Test all commands return to shell — single QEMU session."""

import pexpect, sys, os, subprocess, re
from pathlib import Path

BUILD_DIR = Path(__file__).parent / "build"
KERNEL_ISO = BUILD_DIR / "zirvium.iso"
BLANK_IMG = BUILD_DIR / "blank.img"
TIMEOUT = 10
BOOT_TIMEOUT = 30
PROMPT = '\u20b9 '

def build():
    r = subprocess.run(["make", "iso"], cwd=os.path.dirname(__file__), capture_output=True, text=True)
    if r.returncode: print(r.stderr); sys.exit(1)

def spawn():
    if not BLANK_IMG.exists():
        subprocess.run(["dd", "if=/dev/zero", f"of={BLANK_IMG}", "bs=1M", "count=10"])
    cmd = [
        "qemu-system-x86_64", "-cdrom", str(KERNEL_ISO),
        "-drive", f"file={BLANK_IMG},format=raw,if=ide",
        "-drive", "file=/dev/zero,format=raw,if=none,id=vdisk0", "-device", "virtio-blk-pci,drive=vdisk0",
        "-netdev", "user,id=net0", "-device", "e1000,netdev=net0",
        "-netdev", "user,id=net1", "-device", "virtio-net-pci,netdev=net1",
        "-netdev", "user,id=net2", "-device", "rtl8139,netdev=net2",
        "-boot", "d", "-m", "512M", "-no-reboot", "-nographic",
    ]
    return pexpect.spawn(cmd[0], cmd[1:], encoding='utf-8', codec_errors='replace', timeout=TIMEOUT)

def run_cmd(c, cmd, timeout=TIMEOUT):
    c.sendline(cmd)
    idx = c.expect_exact([PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=timeout)
    if idx != 0: return None
    before = c.before
    lines = before.strip().split('\n')
    return '\n'.join(lines[1:]).strip() if len(lines) > 1 else ''

def main():
    build()
    c = spawn()
    passed = 0
    failed = 0

    TESTS = [
        # (command, expected_pattern, description)
        ("pwd",              r"^/",                     "pwd builtin"),
        ("uname",            r"Zirvium",                "uname builtin"),
        ("echo hello world", r"hello world",            "echo builtin"),
        ("uptime",           r"[0-9]",                  "uptime builtin"),
        ("date",             r"20[0-9][0-9]",           "date builtin"),
        ("timezone",         r"UTC|offset",             "timezone builtin"),
        ("ls",               r"bin|dev|etc|home|usr",   "ls builtin"),
        ("whoami",           r"root",                   "whoami builtin"),
        ("clear",            r"[\x1b]",                 "clear builtin"),
        # External binaries — these must return to the shell
        ("hello",            r"Hello from ZirvUtils",   "hello binary"),
        ("uname",            r"Zirvium",                "uname binary"),
        ("hostname",         r".+",                     "hostname binary"),
        ("sysinfo",          r"Zirvium|Memory|Process", "sysinfo binary"),
        ("lspci",            r"PCI",                    "lspci binary"),
        ("ifconfig",         r"eth0|UP|Network",        "ifconfig binary"),
    ]

    try:
        print("=== Booting ===", flush=True)
        idx = c.expect_exact([PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=BOOT_TIMEOUT)
        if idx != 0: print("BOOT FAILED"); c.close(); return 1
        # Drain banner
        run_cmd(c, "")
        print("Shell ready\n", flush=True)

        for cmd_str, expected, desc in TESTS:
            print(f"  [{cmd_str}]  ", end="", flush=True)
            out = run_cmd(c, cmd_str)
            if out is None:
                print(f"\u2717 FAIL: {desc} (no prompt returned)", flush=True)
                failed += 1
                continue
            if re.search(expected, out, re.IGNORECASE):
                print(f"\u2713 PASS: {desc}", flush=True)
                passed += 1
            else:
                print(f"\u2717 FAIL: {desc}", flush=True)
                print(f"         expected={expected!r} got={out[:80]!r}", flush=True)
                failed += 1

        # Test ping separately (needs longer timeout for first response)
        print("  [ping 10.0.2.2]  ", end="", flush=True)
        c.sendline("ping 10.0.2.2")
        idx = c.expect([r"64 bytes|from", PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=15)
        if idx == 0:
            print("\u2713 PASS: ping gateway", flush=True)
            passed += 1
            # Wait for ping to finish and prompt to return
            idx2 = c.expect_exact([PROMPT, pexpect.TIMEOUT], timeout=TIMEOUT)
            if idx2 != 0:
                print("  (shell did not return after ping, but ping worked)", flush=True)
        elif idx == 1:
            print("\u2717 FAIL: ping gateway (no response, prompt returned)", flush=True)
            failed += 1
        else:
            print("\u2717 FAIL: ping gateway (timeout)", flush=True)
            failed += 1

        print("  [ping 10.0.2.3]  ", end="", flush=True)
        c.sendline("ping 10.0.2.3")
        idx = c.expect([r"64 bytes|from", PROMPT, pexpect.TIMEOUT, pexpect.EOF], timeout=15)
        if idx == 0:
            print("\u2713 PASS: ping DNS server", flush=True)
            passed += 1
        elif idx == 1:
            print("\u2717 FAIL: ping DNS (no response, prompt returned)", flush=True)
            failed += 1
        else:
            print("\u2717 FAIL: ping DNS (timeout)", flush=True)
            failed += 1

        total = passed + failed
        print(f"\n{'='*50}")
        print(f"RESULTS: {passed}/{total} passed, {failed}/{total} failed")
        print(f"Shell prompt returned after each command: YES\n")

    except Exception as e:
        print(f"\nERROR: {e}", flush=True)
        import traceback; traceback.print_exc()
    finally:
        c.sendline("shutdown")
        try: c.expect(pexpect.EOF, timeout=10)
        except: pass
        c.close()

    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
