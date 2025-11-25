{
  boost,
  fetchFromGitHub,
  stdenv,
  tbb,
}:

stdenv.mkDerivation {
  pname = "mmapbench";
  version = "2024-05-10";

  src = fetchFromGitHub {
    owner = "SepehrDV2";
    repo = "mmap-anon-benchmarks";
    rev = "d01bff915a854a44f307734d4348cac3f4e4e3c1";
    hash = "sha256-/xJHhAPB1wWL+/QenX1nosLbEuJIuKwo8DN19ejPBgE=";
  };

  buildInputs = [
    boost
    tbb
  ];

  buildPhase = ''
    $CXX -O3 -g mmapbench.cpp -o mmapbench -ltbb -pthread
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp mmapbench $out/bin/
  '';
}
