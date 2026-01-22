{
  linuxManualConfig,
  fetchzip,
}:

let
  version = "6.19-rc6";
in
linuxManualConfig {
  pname = "linux-ufault";
  inherit version;

  src = fetchzip {
    url = "https://git.kernel.org/torvalds/t/linux-${version}.tar.gz";
    hash = "sha256-qd1Dltzj8BRSSTqm4AzKm42QcZvFfJOCIPFIofd97TI=";
  };

  kernelPatches = map (p: { patch = p; }) [
    ./patches/0001-x86-uintr-man-page-Include-man-pages-draft-for-refer.patch
    ./patches/0002-Documentation-x86-Add-documentation-for-User-Interru.patch
    ./patches/0003-x86-cpu-Enumerate-User-Interrupts-support.patch
    ./patches/0004-x86-fpu-xstate-Enumerate-User-Interrupts-supervisor-.patch
    ./patches/0005-x86-irq-Reserve-a-user-IPI-notification-and-kernel-v.patch
    ./patches/0006-x86-uintr-Introduce-uintr-receiver-syscalls.patch
    ./patches/0007-x86-process-64-Add-uintr-task-context-switch-support.patch
    ./patches/0008-x86-process-64-Clean-up-uintr-task-fork-and-exit-pat.patch
    ./patches/0009-x86-uintr-Introduce-vector-registration-and-uvec_fd-.patch
    ./patches/0010-x86-uintr-Introduce-user-IPI-sender-syscalls.patch
    ./patches/0011-x86-uintr-Add-alternate-stack-support.patch
    ./patches/0012-x86-uintr-Add-support-for-interrupting-blocking-syst.patch
    ./patches/0013-x86-uintr-Introduce-uintr_wait-syscall.patch
    ./patches/0014-x86-uintr-Wire-up-the-user-interrupt-syscalls.patch
    ./patches/0015-x86-uintr-Add-kernel-to-user-event-signaling-support.patch
    ./patches/0016-io_uring-add-x86-uintr-support-to-io_uring.patch
    ./patches/0017-selftests-x86-Add-basic-tests-for-User-IPI.patch
    ./patches/0018-selftests-x86-uintr-Add-a-test-suite-for-UINTR.patch
    ./patches/0019-x86-ufault-add-user-fault-defines-and-instructions.patch
    ./patches/0020-x86-ufault-enumerate-user-fault-support.patch
    ./patches/0021-x86-ufault-add-noufault-kernel-param-to-force-disabl.patch
    ./patches/0022-x86-process-add-lockdep-assert-to-cr4_toggle_bits_ir.patch
    ./patches/0023-x86-cpu-move-cr4_toggle_bits_irqoffs-to-asm-tlbflush.patch
    ./patches/0024-x86-process-add-TIF_UFAULT-to-track-task-user-fault-.patch
    ./patches/0025-x86-process-save-and-restore-UFF-during-context-swit.patch
    ./patches/0026-x86-process-add-thread_info-ufault_handler-and-resto.patch
    ./patches/0027-prctl-add-prctl-PR_UFAULT-to-enable-disable-user-fau.patch
    ./patches/0028-x86-cpu-add-Gem5-build-option.patch
  ];

  configfile = ./config;
}
