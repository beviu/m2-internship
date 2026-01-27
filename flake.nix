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
      apps = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          lib = pkgs.lib;
        in
        {
          gem5 = {
            type = "app";
            program =
              let
                gem5-config = pkgs.replaceVars ./gem5-config.py {
                  defaultDiskImage = self.packages.${system}.gem5-disk-image;
                  defaultKernel = "${self.packages.${system}.linux-ufault.dev}/vmlinux";
                };
                script = pkgs.writeShellScript "gem5.sh" ''
                  exec ${lib.getExe self.packages.${system}.gem5} ${gem5-config} "$@"
                '';
              in
              script.outPath;
          };
        }
      );

      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          packageNames = map ({ name, ... }: name) (
            builtins.filter ({ value, ... }: value == "directory") (
              nixpkgs.lib.attrsToList (builtins.readDir ./pkgs)
            )
          );
          packages =
            pkgs:
            nixpkgs.lib.genAttrs packageNames (name: pkgs.callPackage ./pkgs/${name}/package.nix { })
            // {
              gem5-bridge = pkgs.callPackage ./pkgs/gem5-bridge/package.nix {
                kernel = pkgs.linux-ufault;
                kernelModuleMakeFlags = pkgs.linux-ufault.commonMakeFlags ++ [
                  "KBUILD_OUTPUT=${pkgs.linux-ufault.dev}/lib/modules/${pkgs.linux-ufault.modDirVersion}/build"
                ];
              };
              extmem-ufault = pkgs.extmem.override {
                withUserFaults = true;
              };
            };
          scope = nixpkgs.lib.makeScope pkgs.newScope packages;
        in
        removeAttrs scope [
          "callPackage"
          "newScope"
          "overrideScope"
          "packages"
        ]
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
