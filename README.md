# Time Sense

A Pebble Time 2 (`emery`) app that buzzes every _N_ minutes, aligned to the clock. A background
worker does the buzzing, so it runs under any watchface or app. See `overview.md` for the plan.

## Dev environment (NixOS)

The Pebble SDK downloads prebuilt binaries (ARM gcc, QEMU) that need a standard Linux layout,
so every `pebble` command runs inside the FHS env from `flake.nix`:

```sh
nix develop                          # interactive shell, then use `pebble ...` as normal
nix run . -- -c 'pebble build'       # or run a single command
```

One-time setup (already done on this machine):

```sh
# uv-managed Python, because the Nix Python can't load pip wheels' libstdc++ inside the FHS env.
# This overrides the global UV_PYTHON_PREFERENCE=only-system for this one tool.
nix run . -- -c 'UV_PYTHON_PREFERENCE=only-managed UV_PYTHON_DOWNLOADS=automatic \
  uv tool install pebble-tool --python 3.13 --reinstall'
nix run . -- -c 'pebble sdk install latest'
```

## Building and running

```sh
pebble build
pebble install --emulator emery --vnc --logs   # emulator; --vnc as there's no X window
pebble kill                                     # stop the emulator
```

## Installing on the watch

1. Pebble phone app → Devices → ⋮ on the watch → enable **Dev Connection**.
2. `pebble login` (once; opens a browser and signs in to the same Pebble account).
3. `pebble install --cloudpebble` (add `--logs` to watch `APP_LOG` output; stop it afterwards,
   as an open log stream drains the battery).

Alternatively, over the local network without logging in: Settings → Phone → Connectivity →
_Use LAN developer connection_, then `pebble install --phone <ip shown in the app>`.

## Layout

```
src/c/main.c            foreground app (toggle, settings menu)
worker_src/c/worker.c   background worker (minute tick and vibration)
package.json            metadata: UUID, targetPlatforms ["emery"]
flake.nix               FHS dev shell for the Pebble SDK
```

## License

MIT. See `LICENSE`.
