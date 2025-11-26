{
  bash,
  e2fsprogs,
  lib,
  m5ops,
  runCommand,
  writeClosure,
  writeScript,
}:

let
  initScript = writeScript "extmem-benchmark-init" ''
    #!${lib.getExe bash}
    echo Hello, world!
    ${lib.getExe m5ops} exit
    ${lib.getExe m5ops} exit
  '';
  closure = writeClosure [
    initScript
  ];
  rootfs = runCommand "extmem-benchmark-rootfs" { } ''
    mkdir -p $out/${builtins.storeDir}
    xargs cp -r --target-directory $out/${builtins.storeDir} < ${closure}

    mkdir -p $out/sbin
    ln -s ${initScript} $out/sbin/init

    # Let the kernel mount a devtmpfs at /dev during boot.
    mkdir $out/dev
  '';
in
runCommand "extmem-benchmark-image.img" { } ''
  ${lib.getExe' e2fsprogs "mkfs.ext2"} -d ${rootfs} $out 256M
''
