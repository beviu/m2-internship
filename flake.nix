{
  description = "Development environment for my internship";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
  };

  outputs =
    {
      nixpkgs,
      self,
      treefmt-nix,
      ...
    }:
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
          gem5 = pkgs.callPackage ./pkgs/gem5/package.nix { };
          gem5-disk-image = pkgs.callPackage ./pkgs/gem5-disk-image/package.nix {
            inherit
              extmem
              extmem-ufault
              m5ops
              mmapbench
              simple-mmap-test
              ;
          };
          extmem = pkgs.callPackage ./pkgs/extmem/package.nix { inherit m5ops; };
          extmem-ufault = extmem.override {
            inherit m5ops;

            withUserFaults = true;
          };
          gapbs = pkgs.callPackage ./pkgs/gapbs/package.nix { };
          m5ops = pkgs.callPackage ./pkgs/m5ops/package.nix { };
          mmapbench = pkgs.callPackage ./pkgs/mmapbench/package.nix { };
          simple-mmap-test = pkgs.callPackage ./pkgs/simple-mmap-test/package.nix {
            inherit m5ops;
          };
          twitter-dataset = pkgs.callPackage ./pkgs/twitter-dataset/package.nix { };
          userfault-test = pkgs.callPackage ./pkgs/userfault-test/package.nix { };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            nativeBuildInputs = [
              # clang-tools needs to go before clang
              # (https://github.com/NixOS/nixpkgs/issues/308482#issuecomment-2090095873)
              pkgs.clang-tools

              (pkgs.python3.withPackages (ps: [ ps.pandas ]))
              pkgs.clang
              pkgs.glibc.static
              pkgs.m4
              pkgs.meson
              pkgs.pre-commit
              pkgs.protobuf
              pkgs.samurai
              pkgs.scons
            ];

            buildInputs = [
              pkgs.hdf5-cpp
              pkgs.libpng
              pkgs.zlib
              self.packages.${system}.m5ops
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
