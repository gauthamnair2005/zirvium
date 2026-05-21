%ifndef VMZIRV

section .rodata
global zirvinit_bin_start
global zirvinit_bin_end
global zirvshell_bin_start
global zirvshell_bin_end
global zirvutil_hello_bin_start
global zirvutil_hello_bin_end
global zirvutil_cat_bin_start
global zirvutil_cat_bin_end
global zirvutil_sysinfo_bin_start
global zirvutil_sysinfo_bin_end
global zirvutil_clear_bin_start
global zirvutil_clear_bin_end
global zirvutil_echo_bin_start
global zirvutil_echo_bin_end
global zirvutil_reboot_bin_start
global zirvutil_reboot_bin_end
global zirvutil_shutdown_bin_start
global zirvutil_shutdown_bin_end
global zirvutil_suspend_bin_start
global zirvutil_suspend_bin_end
global zirvutil_poweroff_bin_start
global zirvutil_poweroff_bin_end
global zirvutil_ping_bin_start
global zirvutil_ping_bin_end
global zirvutil_sleep_bin_start
global zirvutil_sleep_bin_end
global zirvutil_true_bin_start
global zirvutil_true_bin_end
global zirvutil_false_bin_start
global zirvutil_false_bin_end
global zirvutil_yes_bin_start
global zirvutil_yes_bin_end
global zirvutil_uname_bin_start
global zirvutil_uname_bin_end
global zirvutil_hostname_bin_start
global zirvutil_hostname_bin_end
global zirvui_bin_start
global zirvui_bin_end

align 4096
zirvinit_bin_start:
    incbin "zirvinit/zirvinit.elf"
align 4096
zirvinit_bin_end:

align 4096
zirvshell_bin_start:
    incbin "zirvshell/zirvshell.elf"
align 4096
zirvshell_bin_end:

align 4096
zirvutil_hello_bin_start:
    incbin "zirvutils/hello.elf"
align 4096
zirvutil_hello_bin_end:

align 4096
zirvutil_cat_bin_start:
    incbin "zirvutils/cat.elf"
align 4096
zirvutil_cat_bin_end:

align 4096
zirvutil_sysinfo_bin_start:
    incbin "zirvutils/sysinfo.elf"
align 4096
zirvutil_sysinfo_bin_end:

align 4096
zirvutil_clear_bin_start:
    incbin "zirvutils/clear.elf"
align 4096
zirvutil_clear_bin_end:

align 4096
zirvutil_echo_bin_start:
    incbin "zirvutils/echo.elf"
align 4096
zirvutil_echo_bin_end:

align 4096
zirvutil_reboot_bin_start:
    incbin "zirvutils/reboot.elf"
align 4096
zirvutil_reboot_bin_end:

align 4096
zirvutil_shutdown_bin_start:
    incbin "zirvutils/shutdown.elf"
align 4096
zirvutil_shutdown_bin_end:

align 4096
zirvutil_suspend_bin_start:
    incbin "zirvutils/suspend.elf"
align 4096
zirvutil_suspend_bin_end:

align 4096
zirvutil_poweroff_bin_start:
    incbin "zirvutils/poweroff.elf"
align 4096
zirvutil_poweroff_bin_end:

align 4096
zirvutil_ping_bin_start:
    incbin "zirvutils/ping.elf"
align 4096
zirvutil_ping_bin_end:

align 4096
zirvutil_sleep_bin_start:
    incbin "zirvutils/sleep.elf"
align 4096
zirvutil_sleep_bin_end:

align 4096
zirvutil_true_bin_start:
    incbin "zirvutils/true.elf"
align 4096
zirvutil_true_bin_end:

align 4096
zirvutil_false_bin_start:
    incbin "zirvutils/false.elf"
align 4096
zirvutil_false_bin_end:

align 4096
zirvutil_yes_bin_start:
    incbin "zirvutils/yes.elf"
align 4096
zirvutil_yes_bin_end:

align 4096
zirvutil_uname_bin_start:
    incbin "zirvutils/uname.elf"
align 4096
zirvutil_uname_bin_end:

align 4096
zirvutil_hostname_bin_start:
    incbin "zirvutils/hostname.elf"
align 4096
zirvutil_hostname_bin_end:

align 4096
zirvui_bin_start:
    incbin "zirvui/zirvui.elf"
align 4096
zirvui_bin_end:

%else
; VMZIRV mode: define all symbols as empty stubs (start == end)
; so the linker can resolve references from embedded.c without
; embedding any user-space binaries.

%macro vmzirv_bin_stub 2
section .rodata
global %1
global %2
%1:
%2:
%endmacro

vmzirv_bin_stub zirvinit_bin_start, zirvinit_bin_end
vmzirv_bin_stub zirvshell_bin_start, zirvshell_bin_end
vmzirv_bin_stub zirvutil_hello_bin_start, zirvutil_hello_bin_end
vmzirv_bin_stub zirvutil_cat_bin_start, zirvutil_cat_bin_end
vmzirv_bin_stub zirvutil_sysinfo_bin_start, zirvutil_sysinfo_bin_end
vmzirv_bin_stub zirvutil_clear_bin_start, zirvutil_clear_bin_end
vmzirv_bin_stub zirvutil_echo_bin_start, zirvutil_echo_bin_end
vmzirv_bin_stub zirvutil_reboot_bin_start, zirvutil_reboot_bin_end
vmzirv_bin_stub zirvutil_shutdown_bin_start, zirvutil_shutdown_bin_end
vmzirv_bin_stub zirvutil_suspend_bin_start, zirvutil_suspend_bin_end
vmzirv_bin_stub zirvutil_poweroff_bin_start, zirvutil_poweroff_bin_end
vmzirv_bin_stub zirvutil_ping_bin_start, zirvutil_ping_bin_end
vmzirv_bin_stub zirvutil_sleep_bin_start, zirvutil_sleep_bin_end
vmzirv_bin_stub zirvutil_true_bin_start, zirvutil_true_bin_end
vmzirv_bin_stub zirvutil_false_bin_start, zirvutil_false_bin_end
vmzirv_bin_stub zirvutil_yes_bin_start, zirvutil_yes_bin_end
vmzirv_bin_stub zirvutil_uname_bin_start, zirvutil_uname_bin_end
vmzirv_bin_stub zirvutil_hostname_bin_start, zirvutil_hostname_bin_end
vmzirv_bin_stub zirvui_bin_start, zirvui_bin_end

%endif
