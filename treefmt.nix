{ ... }:

{
  programs = {
    clang-format.enable = true;
    latexindent.enable = true;
    nixfmt.enable = true;
    ruff.enable = true;
    typstyle.enable = true;
  };

  settings = {
    global.excludes = [
      # Keep the original formatting for third party code.
      "page-fault-timings/variants/*"
    ];

    formatter.typstyle.options = [ "--wrap-text" ];
  };
}
