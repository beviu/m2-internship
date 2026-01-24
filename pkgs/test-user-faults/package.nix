{ stdenv }:

stdenv.mkDerivation {
  pname = "test-user-faults";
  version = "0.1.0";

  src = ./.;

  buildPhase = ''
    # Since on user faults, the CPU directly pushes to the stack, we must
    # disable red zone.
    # TODO: Implement the equivalent of UISTACKADJUST for user faults and use
    #       it.
    $CC test-user-faults.c test-user-faults.S -o test-user-faults -O3
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv test-user-faults $out/bin/
  '';

  meta.mainProgram = "test-user-faults";
}
