{
  fetchFromGitHub,
  lib,
  stdenv,
  kernel,
  kernelModuleMakeFlags,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "gem5_bridge";
  version = "25.1.0.0";

  src = fetchFromGitHub {
    owner = "gem5";
    repo = "gem5";
    tag = "v${finalAttrs.version}";
    hash = "sha256-0goJSUGR0PJe9DEbxhKUHSlkfc8Gqqnd8Pwn8cZigFw=";
  };

  nativeBuildInputs = kernel.moduleBuildDependencies;

  sourceRoot = "source/util/gem5_bridge";

  makeFlags = kernelModuleMakeFlags ++ [
    "KMAKEDIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
    "INSTALL_MOD_PATH=${placeholder "out"}"
  ];

  meta = {
    description = "Driver that provides user-accessible mapping to gem5ops MMIO range";
    homepage = "https://www.gem5.org/";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
  };
})
