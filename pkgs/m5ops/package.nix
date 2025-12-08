# SPDX-License-Identifier: MIT
# This file is based on https://github.com/NixOS/nixpkgs/pull/375739.
# Copyright (c) 2003-2025 Eelco Dolstra and the Nixpkgs/NixOS contributors

{
  fetchFromGitHub,
  glibc,
  isa ? "x86", # x86, arm, thumb, sparc, arm64, or riscv
  lib,
  scons,
  stdenv,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "m5ops-${isa}";
  version = "24.1.0.3";

  src = fetchFromGitHub {
    owner = "gem5";
    repo = "gem5";
    tag = "v${finalAttrs.version}";
    hash = "sha256-MLceCx4Sv+LHDXdmc4wuIArDZjelh7dDqmnPGNVJ2zo=";
  };

  nativeBuildInputs = [ scons ];

  buildInputs = [ glibc.static ];

  sourceRoot = "source/util/m5";

  buildFlags = [ "build/${isa}/out/m5" ];

  postPatch = ''
    # Needed so the build script doesn't hide all Nix environment variables.
    substituteInPlace SConstruct \
      --replace-fail "Environment()" "Environment(ENV=os.environ)"

    # Don't force -no-pie as it breaks using the static library in a PIE
    # executable, which is the default.
    sed -i "s/static_env\.Append(LINKFLAGS=\[ '-no-pie', '-static' \])/static_env.Append(LINKFLAGS='-static', ASFLAGS='-DM5OP_PIC')/g" src/SConscript
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/{bin,include,lib}
    cp build/${isa}/out/m5 $out/bin/
    cp build/${isa}/out/libm5.a $out/lib/
    cp -r ../../include/* src/m5_mmap.h $out/include/
    runHook postInstall
  '';

  meta = {
    description = "Special instructions for gem5";
    homepage = "https://www.gem5.org/";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
    mainProgram = "m5";
  };
})
