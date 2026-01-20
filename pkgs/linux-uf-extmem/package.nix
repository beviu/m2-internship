{
  linuxManualConfig,
  fetchurl,
  lib,
}:

linuxManualConfig rec {
  pname = "linux-uf-extmem";
  version = "6.0";

  src = fetchurl {
    url = "mirror://kernel/linux/kernel/v${lib.versions.major version}.x/linux-${version}.tar.xz";
    hash = "sha256-XCRDpVON5SaI77VcJ6sFOcH161jAz9FqK5+7CP2BeI4=";
  };

  kernelPatches = map (p: { patch = p; }) [
    ./patches/0001-x86-fpu-Add-helpers-for-modifying-supervisor-xstate.patch
    ./patches/0002-x86-uintr-man-page-Include-man-pages-draft-for-refer.patch
    ./patches/0003-Documentation-x86-Add-documentation-for-User-Interru.patch
    ./patches/0004-x86-cpu-Enumerate-User-Interrupts-support.patch
    ./patches/0005-x86-fpu-xstate-Enumerate-User-Interrupts-supervisor-.patch
    ./patches/0006-x86-irq-Reserve-a-user-IPI-notification-and-kernel-v.patch
    ./patches/0007-x86-uintr-Introduce-uintr-receiver-syscalls.patch
    ./patches/0008-x86-process-64-Add-uintr-task-context-switch-support.patch
    ./patches/0009-x86-process-64-Clean-up-uintr-task-fork-and-exit-pat.patch
    ./patches/0010-x86-uintr-Introduce-vector-registration-and-uvec_fd-.patch
    ./patches/0011-x86-uintr-Introduce-user-IPI-sender-syscalls.patch
    ./patches/0012-x86-uintr-Add-alternate-stack-support.patch
    ./patches/0013-x86-uintr-Add-support-for-interrupting-blocking-syst.patch
    ./patches/0014-x86-uintr-Introduce-uintr_wait-syscall.patch
    ./patches/0015-x86-uintr-Wire-up-the-user-interrupt-syscalls.patch
    ./patches/0016-x86-uintr-Add-kernel-to-user-event-signaling-support.patch
    ./patches/0017-io_uring-add-x86-uintr-support-to-io_uring.patch
    ./patches/0018-selftests-x86-Add-basic-tests-for-User-IPI.patch
    ./patches/0019-selftests-x86-uintr-Add-a-test-suite-for-UINTR.patch
    ./patches/0020-archive-repository.patch
    ./patches/0021-x86-cpu-Enumerate-User-Fault-support.patch
    ./patches/0022-x86-process-64-Add-User-Fault-task-context-switch-su.patch
    ./patches/0023-x86-ufault-Introduce-ufault_register_handler-syscall.patch
    ./patches/0024-x86-Add-Gem5-specific-changes.patch
    ./patches/0025-uffd-dax-patch.patch
    ./patches/0026-dma-request-chans-multi-process-simple.patch
    ./patches/0027-less-locking-when-copy.patch
    ./patches/0028-attempt-showing-upcall-path.patch
    ./patches/0029-attempt-at-showing-full-upcall-path.patch
  ];

  configfile = ./config;
}
