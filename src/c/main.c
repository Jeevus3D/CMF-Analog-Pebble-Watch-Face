#include <pebble.h>

// ── Cardinal indicators: set to 1 to show, 0 to hide ────────────────
#define SHOW_CARDINAL_INDICATORS 1

// ── Platform-adaptive measurements ──────────────────────────────────
// Platform reference (developer.rebble.io/guides/tools-and-resources/hardware-information):
//   Aplite  = Pebble Classic / Steel         — 144x168 — B/W
//   Basalt  = Pebble Time / Time Steel       — 144x168 — Color
//   Chalk   = Pebble Time Round              — 180x180 — Color — Round
//   Diorite = Pebble 2                       — 144x168 — B/W
//   Flint   = Pebble 2 Duo                   — 144x168 — B/W
//   Emery   = Pebble Time 2                  — 200x228 — Color
#if defined(PBL_PLATFORM_EMERY)
  // Pebble Time 2 — 200x228
  #define CENTER_X     100
  #define CENTER_Y     114
  #define DIAL_RADIUS   90
  #define MIN_LEN       82
  #define HOUR_DOT_R     7
  #define HOUR_ORBIT    65
  #define PIVOT_R        7
#elif defined(PBL_PLATFORM_CHALK)
  // Pebble Time Round — 180x180
  #define CENTER_X      90
  #define CENTER_Y      90
  #define DIAL_RADIUS   82
  #define MIN_LEN       74
  #define HOUR_DOT_R     6
  #define HOUR_ORBIT    58
  #define PIVOT_R        6
#elif defined(PBL_PLATFORM_GABBRO)
  // Pebble Time Round 2 — 260x260
  #define CENTER_X     130
  #define CENTER_Y     130
  #define DIAL_RADIUS  116
  #define MIN_LEN      106
  #define HOUR_DOT_R     8
  #define HOUR_ORBIT    82
  #define PIVOT_R        8
#elif defined(PBL_PLATFORM_BASALT)
  // Pebble Time / Time Steel — 144x168
  #define CENTER_X      72
  #define CENTER_Y      84
  #define DIAL_RADIUS   64
  #define MIN_LEN       58
  #define HOUR_DOT_R     5
  #define HOUR_ORBIT    46
  #define PIVOT_R        5
#elif defined(PBL_PLATFORM_DIORITE)
  // Pebble 2 — 144x168 — B/W
  #define CENTER_X      72
  #define CENTER_Y      84
  #define DIAL_RADIUS   64
  #define MIN_LEN       58
  #define HOUR_DOT_R     5
  #define HOUR_ORBIT    46
  #define PIVOT_R        5
#elif defined(PBL_PLATFORM_FLINT)
  // Pebble 2 Duo — 144x168 — B/W
  #define CENTER_X      72
  #define CENTER_Y      84
  #define DIAL_RADIUS   64
  #define MIN_LEN       58
  #define HOUR_DOT_R     5
  #define HOUR_ORBIT    46
  #define PIVOT_R        5
#else
  // Aplite fallback: Pebble Classic / Steel — 144x168 — B/W
  #define CENTER_X      72
  #define CENTER_Y      84
  #define DIAL_RADIUS   64
  #define MIN_LEN       58
  #define HOUR_DOT_R     5
  #define HOUR_ORBIT    46
  #define PIVOT_R        5
#endif

// ── State ─────────────────────────────────────────────────────────────
static Window *s_window;
static Layer  *s_canvas_layer;

static struct tm s_last_time;
static int       s_step_count    = 0;
static int       s_battery_level = 100;
static bool      s_bt_connected  = true;
static char      s_weather_buffer[16] = "--°F";
static char      s_date_buffer[12]    = "";
static GFont     s_dot_matrix_font;
static PreferredContentSize s_content_size;

static int s_weather_code = 0;  // 0=clear 1=cloudy 2=rain 3=snow 4=storm

// ── Geometry helper ──────────────────────────────────────────────────
static GPoint prv_hand_point(GPoint center, int32_t angle, int length) {
  return GPoint(
    center.x + (int)(sin_lookup(angle) * length / TRIG_MAX_RATIO),
    center.y - (int)(cos_lookup(angle) * length / TRIG_MAX_RATIO)
  );
}

// ── Font helper for complications ────────────────────────────────────
static GFont prv_comp_font(void) {
  switch (s_content_size) {
    case PreferredContentSizeLarge:
    case PreferredContentSizeExtraLarge:
      return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    default:
      return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  }
}

// ── Icon: Calendar ────────────────────────────────────────────────────
static void prv_draw_icon_calendar(GContext *ctx, GPoint origin) {
  int x = origin.x, y = origin.y;
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, GRect(x, y + 2, 14, 12), 1);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(x + 1, y + 2, 12, 4), 0, GCornerNone);
  graphics_draw_line(ctx, GPoint(x + 4,  y),     GPoint(x + 4,  y + 4));
  graphics_draw_line(ctx, GPoint(x + 10, y),     GPoint(x + 10, y + 4));
  graphics_fill_rect(ctx, GRect(x + 2,  y + 8,  2, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 6,  y + 8,  2, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 10, y + 8,  2, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 2,  y + 11, 2, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 6,  y + 11, 2, 2), 0, GCornerNone);
}

// ── Icon: Shoe (steps) ────────────────────────────────────────────────
static void prv_draw_icon_shoe(GContext *ctx, GPoint origin) {
  int x = origin.x, y = origin.y;
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(x,      y + 13), GPoint(x + 13, y + 13));
  graphics_draw_line(ctx, GPoint(x,      y + 7),  GPoint(x,      y + 13));
  graphics_draw_line(ctx, GPoint(x,      y + 7),  GPoint(x + 3,  y + 4));
  graphics_draw_line(ctx, GPoint(x + 3,  y + 4),  GPoint(x + 9,  y + 4));
  graphics_draw_line(ctx, GPoint(x + 9,  y + 4),  GPoint(x + 13, y + 7));
  graphics_draw_line(ctx, GPoint(x + 13, y + 7),  GPoint(x + 13, y + 13));
  graphics_draw_line(ctx, GPoint(x + 4,  y + 4),  GPoint(x + 5,  y + 1));
  graphics_draw_line(ctx, GPoint(x + 8,  y + 4),  GPoint(x + 8,  y + 1));
  graphics_draw_line(ctx, GPoint(x + 5,  y + 1),  GPoint(x + 8,  y + 1));
}

// ── Icon: Weather (varies by code) ────────────────────────────────────
static void prv_draw_icon_weather(GContext *ctx, GPoint origin, int code) {
  int x = origin.x, y = origin.y;
  graphics_context_set_stroke_color(ctx, GColorOrange);
  graphics_context_set_fill_color(ctx,  GColorOrange);
  graphics_context_set_stroke_width(ctx, 1);

  if (code == 0) {
    graphics_draw_circle(ctx, GPoint(x + 7, y + 7), 3);
    graphics_draw_line(ctx, GPoint(x + 7, y),      GPoint(x + 7, y + 2));
    graphics_draw_line(ctx, GPoint(x + 7, y + 12), GPoint(x + 7, y + 14));
    graphics_draw_line(ctx, GPoint(x,     y + 7),  GPoint(x + 2,  y + 7));
    graphics_draw_line(ctx, GPoint(x + 12,y + 7),  GPoint(x + 14, y + 7));
    graphics_draw_line(ctx, GPoint(x + 2, y + 2),  GPoint(x + 3,  y + 3));
    graphics_draw_line(ctx, GPoint(x + 11,y + 2),  GPoint(x + 12, y + 3));
    graphics_draw_line(ctx, GPoint(x + 2, y + 11), GPoint(x + 3,  y + 12));
    graphics_draw_line(ctx, GPoint(x + 11,y + 11), GPoint(x + 12, y + 12));
  } else if (code == 1) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_context_set_fill_color(ctx,  GColorLightGray);
    graphics_draw_circle(ctx, GPoint(x + 5, y + 7), 4);
    graphics_draw_circle(ctx, GPoint(x + 9, y + 6), 5);
    graphics_fill_rect(ctx, GRect(x + 1, y + 7, 12, 5), 0, GCornerNone);
  } else if (code == 2) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_circle(ctx, GPoint(x + 5, y + 5), 3);
    graphics_draw_circle(ctx, GPoint(x + 9, y + 4), 4);
    graphics_fill_rect(ctx, GRect(x + 1, y + 5, 11, 4), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorPictonBlue);
    graphics_draw_line(ctx, GPoint(x + 3,  y + 10), GPoint(x + 2,  y + 13));
    graphics_draw_line(ctx, GPoint(x + 7,  y + 10), GPoint(x + 6,  y + 13));
    graphics_draw_line(ctx, GPoint(x + 11, y + 10), GPoint(x + 10, y + 13));
  } else if (code == 3) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_circle(ctx, GPoint(x + 5, y + 5), 3);
    graphics_draw_circle(ctx, GPoint(x + 9, y + 4), 4);
    graphics_fill_rect(ctx, GRect(x + 1, y + 5, 11, 4), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(x + 3,  y + 11), 1);
    graphics_fill_circle(ctx, GPoint(x + 7,  y + 12), 1);
    graphics_fill_circle(ctx, GPoint(x + 11, y + 11), 1);
    graphics_fill_circle(ctx, GPoint(x + 5,  y + 13), 1);
    graphics_fill_circle(ctx, GPoint(x + 9,  y + 13), 1);
  } else {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_circle(ctx, GPoint(x + 5, y + 4), 3);
    graphics_draw_circle(ctx, GPoint(x + 9, y + 3), 4);
    graphics_fill_rect(ctx, GRect(x + 1, y + 4, 11, 4), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorOrange);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(x + 8, y + 8),  GPoint(x + 5, y + 11));
    graphics_draw_line(ctx, GPoint(x + 5, y + 11), GPoint(x + 7, y + 11));
    graphics_draw_line(ctx, GPoint(x + 7, y + 11), GPoint(x + 4, y + 14));
  }
}

// ── Canvas draw ──────────────────────────────────────────────────────
static void prv_canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(CENTER_X, CENTER_Y);

  // Dynamic row height based on content size preference
  int comp_row_h = (s_content_size >= PreferredContentSizeLarge) ? 24 : 18;

  // Layout defines — declared first so all sections below can reference them
  #define COMP_X       CENTER_X
  #define COMP_TEXT_X  (COMP_X + 16)
  #define COMP_TEXT_W  (bounds.size.w - COMP_TEXT_X - 6)
  #define COMP_Y_START (CENTER_Y + PIVOT_R + 8)

  // 1. Black background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // 2. Digital time — 12h, aligned to complication column, just above pivot
  static char s_digital_buffer[6];
  strftime(s_digital_buffer, sizeof(s_digital_buffer), "%I:%M", &s_last_time);
  graphics_context_set_text_color(ctx, GColorWindsorTan);
  graphics_draw_text(ctx, s_digital_buffer,
    s_dot_matrix_font,
    GRect(CENTER_X, COMP_Y_START - 75, bounds.size.w - CENTER_X - 4, 40),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // 3. Cardinal indicators
  #if SHOW_CARDINAL_INDICATORS
  for (int i = 0; i < 4; i++) {
    int32_t angle = TRIG_MAX_ANGLE * (i * 3) / 12;
    int screen_reach = (i % 2 == 0) ? CENTER_Y : CENTER_X;
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx,
      prv_hand_point(center, angle, screen_reach - 8),
      prv_hand_point(center, angle, screen_reach - 2));
  }
  #endif

  // 4. Complications (drawn before hands so hands paint over them)

  // Row 1: Calendar
  prv_draw_icon_calendar(ctx, GPoint(COMP_X, COMP_Y_START));
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buffer,
    prv_comp_font(),
    GRect(COMP_TEXT_X, COMP_Y_START - 4, COMP_TEXT_W, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Row 2: Steps
  char steps_str[12];
  if (s_step_count >= 1000) {
    snprintf(steps_str, sizeof(steps_str), "%d.%dk",
      s_step_count / 1000, (s_step_count % 1000) / 100);
  } else {
    snprintf(steps_str, sizeof(steps_str), "%d", s_step_count);
  }
  prv_draw_icon_shoe(ctx, GPoint(COMP_X, COMP_Y_START + comp_row_h));
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, steps_str,
    prv_comp_font(),
    GRect(COMP_TEXT_X, COMP_Y_START + comp_row_h - 4, COMP_TEXT_W, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Row 3: Weather
  prv_draw_icon_weather(ctx, GPoint(COMP_X, COMP_Y_START + comp_row_h * 2), s_weather_code);
  graphics_context_set_text_color(ctx, GColorOrange);
  graphics_draw_text(ctx, s_weather_buffer,
    prv_comp_font(),
    GRect(COMP_TEXT_X, COMP_Y_START + comp_row_h * 2 - 6, COMP_TEXT_W, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // 5. Minute hand
  int32_t min_angle = TRIG_MAX_ANGLE * s_last_time.tm_min / 60;
  GPoint  min_tip   = prv_hand_point(center, min_angle, MIN_LEN);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, center, min_tip);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, min_tip, 1);

  // 6. Hour dot
  int32_t hour_angle = (TRIG_MAX_ANGLE *
    (((s_last_time.tm_hour % 12) * 60) + s_last_time.tm_min)) / 720;
  GPoint hour_dot = prv_hand_point(center, hour_angle, HOUR_ORBIT);
  graphics_context_set_fill_color(ctx, GColorOrange);
  graphics_fill_circle(ctx, hour_dot, HOUR_DOT_R);

  // 7. Center pivot
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, PIVOT_R);
  graphics_context_set_stroke_color(ctx, GColorOrange);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, PIVOT_R);

  // 8. Bluetooth disconnect dot
  if (!s_bt_connected) {
    graphics_context_set_fill_color(ctx, GColorOrange);
    graphics_fill_circle(ctx,
      PBL_IF_ROUND_ELSE(GPoint(CENTER_X + 30, 12), GPoint(bounds.size.w - 12, 12)), 4);
  }
}

// ── Step count ────────────────────────────────────────────────────────
static void prv_update_steps(void) {
  time_t start = time_start_of_today();
  time_t end   = time(NULL);
  if (health_service_metric_accessible(HealthMetricStepCount, start, end)) {
    s_step_count = (int)health_service_sum_today(HealthMetricStepCount);
  }
}

// ── Tick handler ──────────────────────────────────────────────────────
static void prv_tick_handler(struct tm *tick_time, TimeUnits changed) {
  s_last_time = *tick_time;
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %d", tick_time);

  if (changed & MINUTE_UNIT) {
    prv_update_steps();
    if (tick_time->tm_min % 30 == 0) {
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
      app_message_outbox_send();
    }
  }
  layer_mark_dirty(s_canvas_layer);
}

// ── Battery ───────────────────────────────────────────────────────────
static void prv_battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_canvas_layer);
}

// ── Bluetooth ─────────────────────────────────────────────────────────
static void prv_bluetooth_callback(bool connected) {
  s_bt_connected = connected;
  if (!connected) vibes_double_pulse();
  layer_mark_dirty(s_canvas_layer);
}

// ── AppMessage ────────────────────────────────────────────────────────
static void prv_inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *temp_tuple = dict_find(iter, MESSAGE_KEY_TEMPERATURE);
  Tuple *code_tuple = dict_find(iter, MESSAGE_KEY_WEATHER_CODE);

  if (temp_tuple) {
    snprintf(s_weather_buffer, sizeof(s_weather_buffer),
      "%d°F", (int)temp_tuple->value->int32);
  }
  if (code_tuple) {
    s_weather_code = (int)code_tuple->value->int32;
  }
  if (temp_tuple || code_tuple) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason);
}

static void prv_outbox_sent(DictionaryIterator *iter, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Weather request sent OK");
}

// ── Window lifecycle ──────────────────────────────────────────────────
static void prv_window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, prv_canvas_update_proc);
  layer_add_child(root, s_canvas_layer);
  s_dot_matrix_font = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_DOT_MATRIX_36));

  time_t now = time(NULL);
  s_last_time = *localtime(&now);
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %d", &s_last_time);
  prv_update_steps();
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  fonts_unload_custom_font(s_dot_matrix_font);
}

// ── Init / Deinit ─────────────────────────────────────────────────────
static void init(void) {
  s_content_size = preferred_content_size();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = prv_window_load,
    .unload = prv_window_unload
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);

  battery_state_service_subscribe(prv_battery_callback);
  prv_battery_callback(battery_state_service_peek());

  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = prv_bluetooth_callback
  });
  prv_bluetooth_callback(connection_service_peek_pebble_app_connection());

  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_register_outbox_sent(prv_outbox_sent);
  app_message_open(128, 128);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
  app_message_outbox_send();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}