{
  bash,
  coreutils,
  e2fsprogs,
  extmem,
  extmem-ufault,
  lib,
  m5ops,
  mmapbench,
  runCommand,
  simple-mmap-test,
  util-linux,
  writeClosure,
  writeScript,
}:

let
  initScript = writeScript "gem5-init" ''
    #!${lib.getExe bash}

    export PATH=${
      lib.makeBinPath [
        coreutils
        m5ops
        mmapbench
        simple-mmap-test
        util-linux
      ]
    }

    echo "Starting simple-mmap-test..."
    SIMPLE_MMAP_TEST_M5_EXIT=1 simple-mmap-test
    echo "Exit status: $?"

    # Wait here for the m5 exit command to complete, instead of exiting the
    # process which would cause a kernel panic.
    sleep infinity
  '';
  closure = writeClosure [
    initScript
  ];
  rootfs = runCommand "gem5-rootfs" { } ''
    mkdir -p $out/${builtins.storeDir}
    xargs cp -r --target-directory $out/${builtins.storeDir} < ${closure}
    # /dev is created so that the kernel can mount a devtmpfs at /dev during boot.
    mkdir $out/{sbin,dev,proc,sys,tmp}
    ln -s ${initScript} $out/sbin/init
  '';
in
runCommand "gem5-disk-image.img" { } ''
  ${lib.getExe' e2fsprogs "mkfs.ext2"} -d ${rootfs} $out 256M
''
