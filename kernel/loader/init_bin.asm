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
