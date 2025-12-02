{
  description = "Development environment for my internship";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
  };

  outputs =
    { nixpkgs, treefmt-nix, ... }:
    let
      forAllSystems = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        rec {
          extmem-benchmark-image = pkgs.callPackage ./pkgs/extmem-benchmark-image/package.nix {
            inherit extmem extmem-ufault m5ops mmapbench simple-mmap-test;
          };
          extmem = pkgs.callPackage ./pkgs/extmem/package.nix { };
          extmem-ufault = extmem.override { withUserFaults = true; };
          gapbs = pkgs.callPackage ./pkgs/gapbs/package.nix { };
          m5ops = pkgs.callPackage ./pkgs/m5ops/package.nix { };
          mmapbench = pkgs.callPackage ./pkgs/mmapbench/package.nix { };
          simple-mmap-test = pkgs.callPackage ./pkgs/simple-mmap-test/package.nix { };
          twitter-dataset = pkgs.callPackage ./pkgs/twitter-dataset/package.nix { };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            packages = [
              pkgs.clang
              pkgs.clang-tools
              pkgs.glibc.static
              pkgs.meson
              (pkgs.python3.withPackages (ps: [ ps.pandas ]))
              pkgs.samurai
            ];
          };

          gem5 = pkgs.mkShell {
            nativeBuildInputs = [
              pkgs.m4
              pkgs.pre-commit
              pkgs.protobuf
              pkgs.scons
            ];

            buildInputs = [
              pkgs.hdf5-cpp
              pkgs.libpng
              pkgs.zlib
            ];
          };
        }
      );

      formatter = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          treefmtEval = treefmt-nix.lib.evalModule pkgs ./treefmt.nix;
        in
        treefmtEval.config.build.wrapper
      );
    };
}
