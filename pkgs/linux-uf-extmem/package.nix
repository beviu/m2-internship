{
  bc,
  bison,
  elfutils,
  fetchFromGitHub,
  flex,
  stdenv,
}:

stdenv.mkDerivation {
  pname = "linux-uf-extmem";
  version = "6.0.0";

  nativeBuildInputs = [
    bc
    bison
    elfutils
    flex
  ];

  src = fetchFromGitHub {
    owner = "intel";
    repo = "uintr-linux-kernel";
    rev = "0ee776bd38532358159013ed0188693b34c46cf5";
    hash = "sha256-BYPsu2wjnq0uUgCT4lsGF+K0bOvp+fAmKoKbfNzI6SQ=";
  };

  patches = [
    ./patches/0001-x86-Add-X86_FEATURE_UFAULT.patch
    ./patches/0002-x86-Add-User-Fault-MSR-definitions.patch
    ./patches/0003-x86-Add-CONFIG_X86_USER_FAULTS.patch
    ./patches/0004-x86-Add-ufault_register_handler-system-call.patch
    ./patches/0005-uffd-dax-patch.patch
    ./patches/0006-dma-request-chans-multi-process-simple.patch
    ./patches/0007-less-locking-when-copy.patch
    ./patches/0008-attempt-showing-upcall-path.patch
    ./patches/0009-attempt-at-showing-full-upcall-path.patch
  ];

  postPatch = ''
    patchShebangs scripts
  '';

  configurePhase = ''
    runHook preConfigure
    cp ${./config} .config
    runHook postConfigure
  '';

  buildFlags = [ "vmlinux" ];

  installPhase = ''
    cp vmlinux $out
  '';
}
