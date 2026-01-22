{
  bash,
  coreutils,
  e2fsprogs,
  extmem,
  extmem-ufault,
  gem5-bridge,
  kmod,
  linux-ufault,
  lib,
  m5ops,
  mmapbench,
  perl,
  runCommand,
  simple-mmap-test,
  test-user-faults,
  util-linux,
  writeClosure,
  writeScript,
}:

let
  initScript = writeScript "gem5-init" ''
    #!${lib.getExe' bash "sh"}

    export PATH=${
      lib.makeBinPath [
        bash
        coreutils
        kmod
        m5ops
        mmapbench
        perl
        simple-mmap-test
        test-user-faults
        util-linux
      ]
    }

    # Load the module that provides /dev/gem5_bridge. It will be used both by
    # the m5 binary and the libm5.a library.
    insmod ${gem5-bridge}/lib/modules/${linux-ufault.modDirVersion}/gem5/gem5_bridge.ko \
      gem5_bridge_baseaddr=0xffff0000 \
      gem5_bridge_rangesize=0x10000

    export EXTMEM_PATH=${extmem}
    export EXTMEM_UFAULT_PATH=${extmem-ufault}

    ${./init.sh}
  '';
  closure = writeClosure [
    initScript

    # For /usr/bin/env
    coreutils
  ];
  rootfs = runCommand "gem5-rootfs" { } ''
    mkdir -p $out/${builtins.storeDir}
    xargs cp -r --target-directory $out/${builtins.storeDir} < ${closure}
    # /dev is created so that the kernel can mount a devtmpfs at /dev during boot.
    mkdir $out/{dev,proc,sbin,sys,tmp,usr,usr/bin}
    ln -s ${initScript} $out/sbin/init
    ln -s ${lib.getExe' coreutils "env"} $out/usr/bin/env
  '';
in
runCommand "gem5-disk-image.img" { } ''
  ${lib.getExe' e2fsprogs "mkfs.ext2"} -d ${rootfs} $out 256M
''
