#include <pebble.h>

#include "settings.h"

// Opened from Quick Launch: flip on/off, show the result briefly, exit.
// Opened from the app menu: settings menu.
// Opened by the phone settings page: settings menu, plus the changes arriving over AppMessage.

#define TOGGLE_SCREEN_MS 1200

static Settings s_settings;

static const char *const VIBE_NAMES[VIBE_COUNT] = {"Short", "Long", "Double"};

// ---------------------------------------------------------------------------------------------
// Settings and worker

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

static void prv_ensure_worker_running(void) {
  if (!app_worker_is_running()) {
    const AppWorkerResult result = app_worker_launch();
    APP_LOG(APP_LOG_LEVEL_INFO, "app_worker_launch -> %d", (int)result);
  }
}

static void prv_save_settings(void) {
  persist_write_bool(PERSIST_KEY_ENABLED, s_settings.enabled);
  persist_write_int(PERSIST_KEY_INTERVAL, s_settings.interval_min);
  persist_write_int(PERSIST_KEY_VIBE, s_settings.vibe);
  persist_write_int(PERSIST_KEY_HOUR_VIBE, s_settings.hour_vibe);

  // If the worker isn't running yet it reads the new values when it starts.
  AppWorkerMessage message = {0};
  app_worker_send_message(WORKER_MSG_RELOAD_SETTINGS, &message);
}

static int32_t prv_next_interval(int32_t interval_min) {
  for (int i = 0; i < NUM_INTERVALS; i++) {
    if (INTERVALS_MIN[i] == interval_min) {
      return INTERVALS_MIN[(i + 1) % NUM_INTERVALS];
    }
  }
  return DEFAULT_INTERVAL_MIN;
}

// ---------------------------------------------------------------------------------------------
// Phone settings page (Clay, src/pkjs). Selects arrive as strings, toggles as integers.

static MenuLayer *s_menu_layer;

static int32_t prv_tuple_to_int(const Tuple *tuple) {
  switch (tuple->type) {
    case TUPLE_CSTRING:
      return atoi(tuple->value->cstring);
    case TUPLE_INT:
      return tuple->length == 1 ? tuple->value->int8
             : tuple->length == 2 ? tuple->value->int16 : tuple->value->int32;
    case TUPLE_UINT:
      return tuple->length == 1 ? tuple->value->uint8
             : tuple->length == 2 ? tuple->value->uint16 : (int32_t)tuple->value->uint32;
    default:
      return 0;
  }
}

static void prv_send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_Enabled, s_settings.enabled ? 1 : 0);
  dict_write_int32(iter, MESSAGE_KEY_Interval, s_settings.interval_min);
  dict_write_int32(iter, MESSAGE_KEY_Vibe, s_settings.vibe);
  dict_write_int32(iter, MESSAGE_KEY_HourVibe, s_settings.hour_vibe);
  app_message_outbox_send();
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, MESSAGE_KEY_RequestSettings)) {
    prv_send_settings_to_phone();
    return;
  }

  bool changed = false;
  const Tuple *tuple;
  if ((tuple = dict_find(iter, MESSAGE_KEY_Enabled))) {
    s_settings.enabled = prv_tuple_to_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iter, MESSAGE_KEY_Interval))) {
    const int32_t interval_min = prv_tuple_to_int(tuple);
    if (settings_interval_is_valid(interval_min)) {
      s_settings.interval_min = interval_min;
      changed = true;
    }
  }
  if ((tuple = dict_find(iter, MESSAGE_KEY_Vibe))) {
    const int32_t vibe = prv_tuple_to_int(tuple);
    if (vibe >= 0 && vibe < VIBE_COUNT) {
      s_settings.vibe = (VibeType)vibe;
      changed = true;
    }
  }
  if ((tuple = dict_find(iter, MESSAGE_KEY_HourVibe))) {
    const int32_t vibe = prv_tuple_to_int(tuple);
    if (vibe >= 0 && vibe < VIBE_COUNT) {
      s_settings.hour_vibe = (VibeType)vibe;
      changed = true;
    }
  }

  if (changed) {
    prv_save_settings();
    prv_ensure_worker_running();
    if (s_menu_layer) {
      menu_layer_reload_data(s_menu_layer);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Quick Launch toggle screen

static Window *s_toggle_window;
static TextLayer *s_toggle_text_layer;

static void prv_exit_timer_callback(void *context) {
  window_stack_pop_all(true);
}

static void prv_toggle_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, s_settings.enabled ? GColorIslamicGreen : GColorDarkGray);

  GFont font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  const int16_t text_height = 50;
  s_toggle_text_layer = text_layer_create(
      GRect(0, (bounds.size.h - text_height) / 2 - 8, bounds.size.w, text_height));
  text_layer_set_font(s_toggle_text_layer, font);
  text_layer_set_text_alignment(s_toggle_text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_toggle_text_layer, GColorClear);
  text_layer_set_text_color(s_toggle_text_layer, GColorWhite);
  text_layer_set_text(s_toggle_text_layer, s_settings.enabled ? "ON" : "OFF");
  layer_add_child(window_layer, text_layer_get_layer(s_toggle_text_layer));
}

static void prv_toggle_window_unload(Window *window) {
  text_layer_destroy(s_toggle_text_layer);
}

static void prv_quick_toggle(void) {
  s_settings.enabled = !s_settings.enabled;
  prv_save_settings();
  prv_ensure_worker_running();

  if (s_settings.enabled) {
    vibes_double_pulse();
  } else {
    vibes_short_pulse();
  }

  s_toggle_window = window_create();
  window_set_window_handlers(s_toggle_window, (WindowHandlers) {
    .load = prv_toggle_window_load,
    .unload = prv_toggle_window_unload,
  });
  window_stack_push(s_toggle_window, false);
  app_timer_register(TOGGLE_SCREEN_MS, prv_exit_timer_callback, NULL);
}

// ---------------------------------------------------------------------------------------------
// Settings menu

typedef enum {
  ROW_ENABLED = 0,
  ROW_INTERVAL,
  ROW_VIBE,
  ROW_HOUR_VIBE,
  NUM_ROWS,
} MenuRow;

static Window *s_menu_window;
static char s_interval_text[16];

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return NUM_ROWS;
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *context) {
  const char *title = "";
  const char *subtitle = "";
  switch ((MenuRow)cell_index->row) {
    case ROW_ENABLED:
      title = "Buzzing";
      if (!app_worker_is_running()) {
        subtitle = "Background app stopped";
      } else {
        subtitle = s_settings.enabled ? "On" : "Off";
      }
      break;
    case ROW_INTERVAL:
      title = "Every";
      snprintf(s_interval_text, sizeof(s_interval_text), "%d min", (int)s_settings.interval_min);
      subtitle = s_interval_text;
      break;
    case ROW_VIBE:
      title = "Buzz";
      subtitle = VIBE_NAMES[s_settings.vibe];
      break;
    case ROW_HOUR_VIBE:
      title = "On the hour";
      subtitle = VIBE_NAMES[s_settings.hour_vibe];
      break;
    case NUM_ROWS:
      break;
  }
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
}

static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch ((MenuRow)cell_index->row) {
    case ROW_ENABLED:
      if (app_worker_is_running()) {
        s_settings.enabled = !s_settings.enabled;
      } else {
        s_settings.enabled = true;
      }
      break;
    case ROW_INTERVAL:
      s_settings.interval_min = prv_next_interval(s_settings.interval_min);
      break;
    case ROW_VIBE:
      s_settings.vibe = (s_settings.vibe + 1) % VIBE_COUNT;
      prv_play(s_settings.vibe);
      break;
    case ROW_HOUR_VIBE:
      s_settings.hour_vibe = (s_settings.hour_vibe + 1) % VIBE_COUNT;
      prv_play(s_settings.hour_vibe);
      break;
    case NUM_ROWS:
      break;
  }
  prv_save_settings();
  prv_ensure_worker_running();
  menu_layer_reload_data(menu_layer);
}

static void prv_refresh_timer_callback(void *context) {
  if (s_menu_layer) {
    menu_layer_reload_data(s_menu_layer);
  }
}

static void prv_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  s_menu_layer = menu_layer_create(layer_get_bounds(window_layer));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  menu_layer_set_highlight_colors(s_menu_layer, GColorIslamicGreen, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void prv_menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
}

static void prv_show_menu(void) {
  prv_ensure_worker_running();

  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers) {
    .load = prv_menu_window_load,
    .unload = prv_menu_window_unload,
  });
  window_stack_push(s_menu_window, true);
  // Worker launch is asynchronous; refresh the "Buzzing" row once it has started.
  app_timer_register(500, prv_refresh_timer_callback, NULL);
}

// ---------------------------------------------------------------------------------------------

static void prv_init(void) {
  s_settings = settings_load();

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);

  if (launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    prv_quick_toggle();
  } else {
    prv_show_menu();
  }
}

static void prv_deinit(void) {
  if (s_toggle_window) {
    window_destroy(s_toggle_window);
  }
  if (s_menu_window) {
    window_destroy(s_menu_window);
  }
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
