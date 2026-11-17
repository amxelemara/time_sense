#pragma once

// Shared by the foreground app (src/c) and the background worker (worker_src/c). Both tasks
// read and write the same persistent storage.

#include <stdbool.h>
#include <stdint.h>

#define PERSIST_KEY_ENABLED 1
#define PERSIST_KEY_INTERVAL 2
#define PERSIST_KEY_VIBE 3
#define PERSIST_KEY_HOUR_VIBE 4

// AppWorkerMessage type sent by the app after any settings change.
#define WORKER_MSG_RELOAD_SETTINGS 1

// The built-in vibes_*_pulse() functions.
typedef enum {
  VIBE_SHORT = 0,
  VIBE_LONG,
  VIBE_DOUBLE,
  VIBE_COUNT,
} VibeType;

typedef struct {
  bool enabled;
  int32_t interval_min;
  VibeType vibe;
  VibeType hour_vibe;
} Settings;

// Intervals are divisors of 60 so every hour starts a fresh, evenly spaced cycle.
static const int32_t INTERVALS_MIN[] = {1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60};
#define NUM_INTERVALS ((int)(sizeof(INTERVALS_MIN) / sizeof(INTERVALS_MIN[0])))
#define DEFAULT_INTERVAL_MIN 5

static inline bool settings_interval_is_valid(int32_t interval_min) {
  for (int i = 0; i < NUM_INTERVALS; i++) {
    if (INTERVALS_MIN[i] == interval_min) {
      return true;
    }
  }
  return false;
}

static inline VibeType prv_settings_read_vibe(uint32_t key, VibeType fallback) {
  if (!persist_exists(key)) {
    return fallback;
  }
  const int32_t value = persist_read_int(key);
  return (value >= 0 && value < VIBE_COUNT) ? (VibeType)value : fallback;
}

static inline Settings settings_load(void) {
  Settings settings = {
    .enabled = persist_exists(PERSIST_KEY_ENABLED) ? persist_read_bool(PERSIST_KEY_ENABLED)
                                                   : true,
    .interval_min = persist_exists(PERSIST_KEY_INTERVAL) ? persist_read_int(PERSIST_KEY_INTERVAL)
                                                         : DEFAULT_INTERVAL_MIN,
    .vibe = prv_settings_read_vibe(PERSIST_KEY_VIBE, VIBE_SHORT),
    .hour_vibe = prv_settings_read_vibe(PERSIST_KEY_HOUR_VIBE, VIBE_DOUBLE),
  };
  if (!settings_interval_is_valid(settings.interval_min)) {
    settings.interval_min = DEFAULT_INTERVAL_MIN;
  }
  return settings;
}
