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
  initScript = writeScript "extmem-benchmark-init" ''
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

    mount -t tmpfs -o size=19G tmpfs /tmp
    mount -t proc proc /proc

    export DRAMSIZE=8289934592 # less than 8GB to account for metadata

    m5 exit

    echo "Starting simple-mmap-test with ExtMem (User Faults)..."
    LD_PRELOAD=${extmem-ufault}/lib/libextmem-default.so simple-mmap-test
    echo "Exit status: $?"

    echo "Starting mmapbench in random write mode..."
    mmapbench /dev/null 1 1 0 0 1 | head -n 3

    echo "Starting mmapbench in random write mode with ExtMem (SIGBUS)..."
    LD_PRELOAD=${extmem}/lib/libextmem-default.so mmapbench /dev/null 1 1 0 0 1 | head -n 3

    echo "Starting mmapbench in random write mode with ExtMem (User Faults)..."
    LD_PRELOAD=${extmem-ufault}/lib/libextmem-default.so mmapbench /dev/null 1 1 0 0 1 | head -n 3

    m5 exit
  '';
  closure = writeClosure [
    initScript
  ];
  rootfs = runCommand "extmem-benchmark-rootfs" { } ''
    mkdir -p $out/${builtins.storeDir}
    xargs cp -r --target-directory $out/${builtins.storeDir} < ${closure}
    # /dev is created so that the kernel can mount a devtmpfs at /dev during boot.
    mkdir $out/{sbin,dev,proc,sys,tmp}
    ln -s ${initScript} $out/sbin/init
  '';
in
runCommand "extmem-benchmark-image.img" { } ''
  ${lib.getExe' e2fsprogs "mkfs.ext2"} -d ${rootfs} $out 256M
''
