# Time Sense — Pebble Time 2 interval buzzer

A Pebble app that buzzes every _N_ minutes, **aligned to the clock** (:00, :05, :10 …) rather
than to when it was started. The point is to feel time passing. It runs in the background no
matter which watchface or app is on screen, until it is turned off.

Status (2026-11-17): **v1.0.0 published** (Listed, Health & Fitness):
https://apps.repebble.com/127494506276466b95246f4e — and installed on my Pebble Time 2.
Phases 0–4 are built: background worker (vibration verified on the real watch), Quick Launch
toggle, on-watch settings menu, and Clay phone settings (both sync directions tested in the
emulator). Not yet verified on real hardware: the Quick Launch toggle and the phone page.
New releases: bump `version` in package.json, then `pebble publish --release-notes "..."`.

Target watch: **Pebble Time 2** (platform `emery`: 200×228 colour touch screen, LRA vibration
motor, Bluetooth LE only, current Core Devices PebbleOS).

## Requirements

- Buzz every _N_ minutes, aligned to the top of the hour.
- _N_ is one of the divisors of 60: **1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60**. Default **5**.
- Configurable vibration type (see [Settings](#settings)).
- Runs in the background regardless of the active watchface or app.
- Quick on/off toggle from the watchface, without going through menus.
- Keeps running after a reboot if it was on, and stays off if it was off.
- Install on my own watch; publishing to the appstore is optional.

## Architecture

The app has two parts, both in one `.pbw`:

| Part | Runs when | Job |
|---|---|---|
| **Background worker** (`worker_src/`) | Always, once started, even across reboots | Gets a callback every minute and buzzes on aligned minutes |
| **Foreground app** (`src/`) | Only while open | On/off toggle, settings menu, receives phone settings |

The user keeps whatever watchface they like. This app is a _watchapp_, not a watchface.

### Why a background worker

Options that were considered:

| Option | Problem |
|---|---|
| Watchface + minute tick | Only runs while that face is on screen. Opening any app pauses it. It also forces a particular watchface. |
| Watchapp + `Wakeup` API | Every buzz launches the app to the foreground, closing whatever you had open. At most 8 wakeups per app. No app may schedule another wakeup within ±1 minute of an existing one, so other apps' wakeups can collide. |
| Worker + `worker_launch_app()` | Same foreground interruption as wakeups. |
| **Worker + minute tick + vibrate directly** | **Chosen.** Nothing comes to the foreground. See the risk below. |

### Risk: vibration from the worker is undocumented

The SDK's `pebble_worker.h` doesn't declare the `vibes_*` functions. They're marked `appOnly`
in `tools/generate_native_sdk/exported_symbols.json`. But reading the firmware source shows:

- `appOnly` only removes them from the **header**. Apps and workers share the same jump-table
  shim, so the symbols are still linkable.
- The vibration system calls (`sys_vibe_pattern_enqueue_step`, `sys_vibe_pattern_trigger_start`)
  have **no task check**, in either the original 2016 firmware (`google/pebble`) or the current
  Core Devices PebbleOS.
- Current PebbleOS explicitly supports it. `services/vibe_pattern/service.c` records
  `VibePatternOwner_Worker` when a worker starts a pattern, and `worker_manager.c` calls
  `vibe_pattern_clear_for_owner(VibePatternOwner_Worker)` when a worker exits.

So the worker declares the prototype itself:

```c
// Not in pebble_worker.h, but the firmware accepts them from the worker task.
void vibes_short_pulse(void);
void vibes_long_pulse(void);
void vibes_double_pulse(void);
```

**Phase 1 tests this on the real watch before anything else is built.** Fallback if it fails:
the worker calls `worker_launch_app()` on aligned minutes, and the app buzzes and exits
straight away. That works but briefly interrupts whatever is on screen.

### Worker constraints (from the docs)

- 10.5 kB of memory, which is plenty here.
- No UI, no `AppMessage`, no resources.
- **Only one background worker can run on the watch at a time.** If another app's worker is
  running, starting this one asks the user to replace it.
- The OS remembers the "default worker" and relaunches it at boot
  (`worker_manager_enable()` → `worker_preferences_get_default_worker()`).
- `app_worker_kill()` does **not** clear that default. A killed worker can come back after a
  reboot, so on/off has to be a stored flag, not just killing the worker (see below).

## Behaviour

### Worker

```
on start:     load settings from persistent storage
              subscribe to the minute tick (MINUTE_UNIT, fires at second 0)
on tick(t):   if !enabled → return
              if t->tm_min % interval != 0 → return
              play (t->tm_min == 0 ? hour_pattern : interval_pattern)
on message:   reload settings (sent by the app after any change)
```

The tick uses local time, so timezone and daylight-saving changes need no special handling.

### On/off toggle

- **On/off is a stored `enabled` flag** that the worker checks on every tick. The worker stays
  running while off, but subscribing to the tick costs almost nothing. This avoids the
  "replace background app?" prompt on every toggle, and stays correct across reboots.
- **Quick toggle:** in the watch's Settings → Quick Launch, assign Time Sense to a button.
  Current PebbleOS offers a single press of Up or Down, and a long press of Up, Select, Down or
  Back (`apps/system/settings/quick_launch.c`). Quick Launch is triggered from the watchface
  and only offers watchapps, which is another reason this isn't a watchface.
- When the launch reason is `APP_LAUNCH_QUICK_LAUNCH`, the app:
  1. flips `enabled` and saves it;
  2. starts the worker if it isn't running (`app_worker_launch()`) and sends it a message;
  3. shows a full-screen "ON" or "OFF" for about 1 second, with a confirmation buzz (double for
     on, single for off);
  4. exits back to the watchface by itself.
- When opened normally from the app menu, it shows the settings menu instead.

### Settings

| Setting | Values | Default |
|---|---|---|
| Enabled | on / off | on |
| Interval | 1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60 min | 5 |
| Interval vibration | short, long, double | short |
| Hour vibration | same as interval / any of the above | double |

Settings are stored with `persist_write_*` (storage is shared by the app and the worker). After
each change the app sends the worker an `AppWorkerMessage` so it reloads.

**Where to edit them:**

1. **On the watch (phase 3):** a `SimpleMenuLayer` / `MenuLayer` in the app. Needs no phone
   and no JavaScript.
2. **On the phone (phase 4, optional):** a settings page in the Pebble mobile app, built with
   Clay (PebbleKit JS). Opening it starts the watchapp, which receives the values over
   `AppMessage`, saves them and messages the worker.

## Project layout (planned)

```
pebble_time_sense/
  overview.md            this file
  package.json           targetPlatforms: ["emery"]; worker enabled
  wscript                generated by `pebble new-project --worker`
  src/c/main.c           foreground app: toggle, settings menu, AppMessage
  src/c/settings.h       persist keys, Settings struct, pattern table (shared with worker)
  worker_src/c/worker.c  minute tick and vibration
  src/pkjs/              phase 4 (later): Clay config page
```

## Phases

0. **Toolchain.** `uv tool install pebble-tool --python 3.13`, `pebble sdk install latest`,
   `pebble new-project --worker --ai time_sense`. Test with `pebble install --emulator emery`.
   On NixOS the emulator (a downloaded QEMU) needs SDL2, glib, pixman, zlib and sndio,
   probably through a `flake.nix` dev shell or nix-ld.
   Fallback: CloudPebble in the browser.
1. **Spike (go/no-go).** A minimal worker that buzzes every minute using the forward-declared
   vibes function. Install it on the real watch, open other apps, and confirm it buzzes.
   Also check the battery after a day.
2. **Worker.** Aligned intervals, both patterns, settings loaded from storage, reload on
   message.
3. **Foreground app.** Quick Launch toggle screen and the on-watch settings menu.
4. **Phone settings (optional).** Clay config page.
5. **Install.** `pebble install --cloudpebble` (needs `pebble login`, and Dev Connection turned
   on in the Pebble app), or `pebble install --phone <ip>` over the local network. Optionally
   `pebble publish`, then set the listing to _Unlisted_ in the Developer Dashboard.

## Decisions

- Watch: Pebble Time 2 (`emery`) only. Other platforms can be added to `targetPlatforms` later.
- Settings: on-watch menu in v1. The phone (Clay) settings page comes later, in phase 4.
- v1 extras: a distinct buzz on the hour only.
- Vibration types are the C API's built-in ones only: `vibes_short_pulse()` (250 ms),
  `vibes_long_pulse()` (500 ms) and `vibes_double_pulse()`. No custom patterns.

## Later ideas

- Quiet hours (no buzzing overnight). First check whether the system's Quiet Time already
  silences app vibrations.
- Patterns that encode the time, e.g. 1 pulse at :15, 2 at :30, 3 at :45, long at :00.
- Touch controls in the settings menu (the Time 2 has a touchscreen; watchapps can use it).

## References

- SDK install: https://developer.repebble.com/sdk/
- Background worker: https://developer.repebble.com/guides/events-and-services/background-worker/
- Wakeups: https://developer.repebble.com/guides/events-and-services/wakeups/
- Vibes API: https://developer.repebble.com/docs/c/User_Interface/Vibes/
- Developer Connection:
  https://developer.repebble.com/guides/tools-and-resources/developer-connection/
- Publishing: https://developer.repebble.com/guides/appstore-publishing/publishing-an-app/
- PebbleOS source: https://github.com/coredevices/PebbleOS
  (`pebble/services/vibe_pattern/service.c`, `pebble/process_management/worker_manager.c`,
  `pebble/syscall/app_worker_syscalls.c`, `pebble/applib/app_launch_reason.h`)
