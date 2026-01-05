{ stdenv }:

stdenv.mkDerivation {
  pname = "userfault-test";
  version = "0.1.0";

  src = ./.;

  buildPhase = ''
    $CC userfault-test.c -o userfault-test -O3
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv userfault-test $out/bin/
  '';

  meta.mainProgram = "userfault-test";
}
