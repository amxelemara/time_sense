#include <pebble_worker.h>

#include "../../src/c/settings.h"

// The vibes API is marked appOnly in the SDK, so pebble_worker.h does not declare it. The
// symbols are still in the shared jump table, and PebbleOS tracks worker-owned vibe patterns
// (VibePatternOwner_Worker in services/vibe_pattern/service.c). Verified on a Pebble Time 2.
void vibes_short_pulse(void);
void vibes_long_pulse(void);
void vibes_double_pulse(void);

static Settings s_settings;

static void prv_play(VibeType vibe) {
  switch (vibe) {
    case VIBE_LONG:
      vibes_long_pulse();
      break;
    case VIBE_DOUBLE:
      vibes_double_pulse();
      break;
    case VIBE_SHORT:
    default:
      vibes_short_pulse();
      break;
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (!s_settings.enabled || tick_time->tm_min % s_settings.interval_min != 0) {
    return;
  }
  prv_play(tick_time->tm_min == 0 ? s_settings.hour_vibe : s_settings.vibe);
}

static void prv_message_handler(uint16_t type, AppWorkerMessage *data) {
  if (type == WORKER_MSG_RELOAD_SETTINGS) {
    s_settings = settings_load();
    APP_LOG(APP_LOG_LEVEL_INFO, "settings: enabled=%d interval=%d vibe=%d hour=%d",
            s_settings.enabled, (int)s_settings.interval_min, s_settings.vibe,
            s_settings.hour_vibe);
  }
}

static void prv_init(void) {
  s_settings = settings_load();
  app_worker_message_subscribe(prv_message_handler);
  // MINUTE_UNIT fires at second 0 of every minute, in local time.
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  app_worker_message_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}
