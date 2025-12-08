{ m5ops, stdenv }:

stdenv.mkDerivation {
  pname = "simple-mmap-test";
  version = "0.1.0";

  src = ./.;

  buildInputs = [ m5ops ];

  buildPhase = ''
    $CC simple-mmap-test.c -o simple-mmap-test -O3 -lm5
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv simple-mmap-test $out/bin/
  '';

  meta.mainProgram = "simple-mmap-test";
}
