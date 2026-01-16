{ stdenv }:

stdenv.mkDerivation {
  pname = "test-user-faults";
  version = "0.1.0";

  src = ./.;

  buildPhase = ''
    $CC test-user-faults.c -o test-user-faults -O3
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv test-user-faults $out/bin/
  '';

  meta.mainProgram = "test-user-faults";
}
