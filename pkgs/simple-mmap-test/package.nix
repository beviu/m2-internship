{ stdenv }:

stdenv.mkDerivation {
  pname = "simple-mmap-test";
  version = "0.1.0";

  src = ./.;

  buildPhase = ''
    $CC simple-mmap-test.c -O3 -o simple-mmap-test
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv simple-mmap-test $out/bin/
  '';

  meta.mainProgram = "simple-mmap-test";
}
