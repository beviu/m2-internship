{
  capstone_4,
  cmake,
  fetchFromGitHub,
  lib,
  liburing,
  pkg-config,
  stdenv,
  withUserFaults ? false,
}:

let
  linuxHeaders = stdenv.mkDerivation {
    pname = "linux-headers";
    version = "6.4.0-rc4";

    src = fetchFromGitHub {
      owner = "SepehrDV2";
      repo = "linux";
      rev = "0fa2007a9aa3f86902f44a7550ea46504e103b41";
      hash = "sha256-jACRCB8ZaCciOw8+OFdxnw+h+epjutcrf6mhM99hQgE=";
    };

    buildPhase = ''
      make headers $makeFlags
    '';

    doCheck = false;

    installPhase = ''
      cp -r usr/include/. $out/
      find $out -type f ! -name '*.h' -delete
    '';
  };

  syscall_intercept = stdenv.mkDerivation {
    pname = "syscall_intercept";
    version = "2025-01-06";

    src = fetchFromGitHub {
      owner = "pmem";
      repo = "syscall_intercept";
      rev = "b1b9bedcc8cf7d711cd3e74f08d860722e7c301d";
      hash = "sha256-bvftVo84pIxb+rxtHhxnX0cX4p8TVSuP56qSZwVbLFc=";
    };

    nativeBuildInputs = [
      cmake
      pkg-config
    ];
    buildInputs = [ capstone_4 ];

    cmakeFlags = [
      # Perl is required for checking coding style.
      "-DPERFORM_STYLE_CHECKS=OFF"
    ];
  };

in
stdenv.mkDerivation {
  pname = "extmem${lib.optionalString withUserFaults "-ufault"}";
  version = "2024-10-15";

  src = fetchFromGitHub {
    owner = "SepehrDV2";
    repo = "ExtMem";
    rev = "3da53db310401703d5485f2ab40edd35aec69e83";
    hash = "sha256-YDDyXg2zljVtBBsYOvz34ixJojxWxXIHDwwhvOibJ90=";
  };

  patches = [
    ./patches/0001-Remove-main_mmap-variable-that-is-never-read.patch
    ./patches/0002-Remove-call-to-undefined-function-lrudisk_ack_vma.patch
    ./patches/0003-Add-integer-to-pointer-casts-where-needed.patch
    ./patches/0004-Add-missing-declarations-of-ioring_-functions.patch
    ./patches/0005-Add-missing-declarations-of-lrudisk_-functions.patch
    ./patches/0006-Add-missing-declaration-of-core_try_prefetch-functio.patch
    ./patches/0007-Add-missing-definition-of-core_migrate_up_async_-fun.patch
    ./patches/0008-Fix-direct_allocate_page_asynch-typos.patch
    ./patches/0009-Add-missing-declaration-of-extmem_migration_downdisk.patch
    ./patches/0010-Remove-unused-functions-lrudisk_allocate_page_critic.patch
    ./patches/0011-Remove-call-to-undefined-function-lrudisk_stats.patch
  ]
  ++ lib.optional withUserFaults ./patches/0012-Implement-User-Fault-support.patch;

  buildInputs = [
    liburing
    syscall_intercept
  ];

  postPatch = ''
    # The next phases will also need to run in the src directory.
    cd src

    sed -i 's@../linux/usr/include/@${linuxHeaders}@g' Makefile
  '';

  installPhase = ''
    mkdir -p $out/lib
    cp libextmem-default.so $out/lib/
  '';
}
