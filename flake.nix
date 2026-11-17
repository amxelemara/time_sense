{
  description = "Time Sense: Pebble Time 2 interval buzzer (Pebble SDK dev shell)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      # pebble-tool (installed with `uv tool install pebble-tool`) downloads a prebuilt
      # arm-none-eabi toolchain and a QEMU emulator that expect a standard Linux layout
      # (/lib64/ld-linux-x86-64.so.2 and libraries under /usr/lib). An FHS env provides that.
      fhs = pkgs.buildFHSEnv {
        name = "pebble-fhs";
        targetPkgs = p: with p; [
          # pebble-tool itself
          uv
          python313
          nodejs
          # arm-none-eabi toolchain
          stdenv.cc.cc.lib
          ncurses
          ncurses5
          libxcrypt-legacy
          xz
          zstd
          zlib
          # QEMU emulator
          glib
          pixman
          SDL2
          libpng
          sndio
          alsa-lib
          libpulseaudio
          dtc
        ];
        profile = ''
          export PATH="$HOME/.local/bin:$PATH"
          # uv-managed Python looks for certs in the wrong place on NixOS.
          export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
          # pebble-tool and the SDK venv must use uv-managed Python: the Nix Python cannot
          # load the FHS libraries (libstdc++) that pip wheels need.
          export UV_PYTHON_PREFERENCE=only-managed
          export UV_PYTHON_DOWNLOADS=automatic
        '';
        runScript = "zsh";
      };
    in
    {
      # `nix develop` drops into the FHS shell; run `pebble build` etc. from there.
      devShells.${system}.default = fhs.env;
      # `nix run . -- -c "pebble build"` runs one command inside the FHS env.
      packages.${system}.default = fhs;
    };
}
