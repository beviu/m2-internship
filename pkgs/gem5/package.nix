# SPDX-License-Identifier: MIT
# This file is based on https://github.com/NixOS/nixpkgs/pull/375739.
# Copyright (c) 2003-2025 Eelco Dolstra and the Nixpkgs/NixOS contributors
{
  boost,
  capstone,
  fetchFromGitHub,
  gperftools,
  hdf5-cpp,
  isa ? "X86", # ARM, NULL, MIPS, POWER, RISCV, SPARC, or X86
  lib,
  libpng,
  m4,
  protobuf_21,
  python3,
  scons,
  stdenv,
  variant ? "opt", # debug, opt, or fast
  zlib,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "gem5-${isa}-${variant}";
  version = "25.1.0.0";

  src = fetchFromGitHub {
    owner = "gem5";
    repo = "gem5";
    tag = "v${finalAttrs.version}";
    hash = "sha256-0goJSUGR0PJe9DEbxhKUHSlkfc8Gqqnd8Pwn8cZigFw=";
  };

  patches = [
    ./patches/0001-arch-x86-Fix-indentation-in-two_byte_opcodes.isa.patch
    ./patches/0002-arch-x86-Implement-UIRET.patch
    ./patches/0003-arch-x86-implement-User-Fault-delivery.patch
    ./patches/0004-arch-x86-implement-STUF-and-CLUF.patch
    ./patches/0005-arch-x86-advertise-User-Fault-support-in-CPUID-exten.patch
  ];

  nativeBuildInputs = [
    m4
    python3
    scons
  ];

  buildInputs = [
    boost
    capstone
    gperftools # provides tcmalloc
    hdf5-cpp
    libpng
    protobuf_21
    zlib
  ];

  buildFlags = [ "build/${isa}/gem5.${variant}" ];

  enableParallelBuilding = true;

  preBuild = ''
    patchShebangs .
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp build/${isa}/gem5.${variant} $out/bin/
    runHook postInstall
  '';

  meta = {
    description = "A modular platform for computer-system architecture research";
    homepage = "https://www.gem5.org/";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
    mainProgram = "gem5.${variant}";
  };
})
