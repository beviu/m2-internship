{ fetchFromGitHub, stdenv }:

stdenv.mkDerivation {
  pname = "gapbs";
  version = "2024-05-10";

  src = fetchFromGitHub {
    owner = "SepehrDV2";
    repo = "gapbs";
    rev = "ad964d9d422d8967b822fd30d0a40bdaf5aaa21e";
    hash = "sha256-6xqetNfX0JpVuwta06q1MDe93vustt2l3aYCZJhZl1g=";
  };

  installPhase = ''
    mkdir -p $out/bin
    cp bc bfs cc cc_sv pr pr_spmv sssp tc $out/bin
  '';
}
