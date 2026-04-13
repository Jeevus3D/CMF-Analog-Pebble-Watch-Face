#include <pebble.h>

// ── Cardinal indicators: set to 1 to show, 0 to hide ────────────────
#define SHOW_CARDINAL_INDICATORS 1

// ── Platform-adaptive measurements ──────────────────────────────────
#if defined(PBL_ROUND)
  #define CENTER_X      90
  #define CENTER_Y      90
  #define DIAL_RADIUS   82
  #define MIN_LEN       74
  #define MIN_TIP_R      3
  #define HOUR_DOT_R     6
  #define HOUR_ORBIT    58
  #define PIVOT_R        6
#else
  // Basalt — Pebble Time 2 (200x228)
  #define CENTER_X     100
  #define CENTER_Y     114
  #define DIAL_RADIUS   90
  #define MIN_LEN       82
  #define HOUR_DOT_R     7
  #define HOUR_ORBIT    65
  #define PIVOT_R        7
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

static int  s_weather_code   = 0;  // 0=clear 1=cloudy 2=rain 3=snow 4=storm

// ── Geometry helper ──────────────────────────────────────────────────
static GPoint prv_hand_point(GPoint center, int32_t angle, int length) {
  return GPoint(
    center.x + (int)(sin_lookup(angle) * length / TRIG_MAX_RATIO),
    center.y - (int)(cos_lookup(angle) * length / TRIG_MAX_RATIO)
  );
}

// ── Icon: Calendar ────────────────────────────────────────────────────
static void prv_draw_icon_calendar(GContext *ctx, GPoint origin) {
  // origin = top-left of 14x14 icon area
  int x = origin.x, y = origin.y;
  // Outer rect
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, GRect(x, y + 2, 14, 12), 1);
  // Header bar
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(x + 1, y + 2, 12, 4), 0, GCornerNone);
  // Ring tabs
  graphics_draw_line(ctx, GPoint(x + 4, y),     GPoint(x + 4, y + 4));
  graphics_draw_line(ctx, GPoint(x + 10, y),    GPoint(x + 10, y + 4));
  // Day dots (2x2 grid)
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
  // Sole
  graphics_draw_line(ctx, GPoint(x,      y + 13), GPoint(x + 13, y + 13));
  // Heel
  graphics_draw_line(ctx, GPoint(x,      y + 7),  GPoint(x,      y + 13));
  // Toe box curve (approximate with lines)
  graphics_draw_line(ctx, GPoint(x,      y + 7),  GPoint(x + 3,  y + 4));
  graphics_draw_line(ctx, GPoint(x + 3,  y + 4),  GPoint(x + 9,  y + 4));
  graphics_draw_line(ctx, GPoint(x + 9,  y + 4),  GPoint(x + 13, y + 7));
  graphics_draw_line(ctx, GPoint(x + 13, y + 7),  GPoint(x + 13, y + 13));
  // Tongue
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
    // Sun: circle + 4 rays
    graphics_draw_circle(ctx, GPoint(x + 7, y + 7), 3);
    graphics_draw_line(ctx, GPoint(x + 7, y),     GPoint(x + 7, y + 2));   // N
    graphics_draw_line(ctx, GPoint(x + 7, y + 12),GPoint(x + 7, y + 14)); // S
    graphics_draw_line(ctx, GPoint(x,     y + 7), GPoint(x + 2, y + 7));   // W
    graphics_draw_line(ctx, GPoint(x + 12,y + 7), GPoint(x + 14,y + 7));  // E
    graphics_draw_line(ctx, GPoint(x + 2, y + 2), GPoint(x + 3, y + 3));   // NW
    graphics_draw_line(ctx, GPoint(x + 11,y + 2), GPoint(x + 12,y + 3));  // NE
    graphics_draw_line(ctx, GPoint(x + 2, y + 11),GPoint(x + 3, y + 12)); // SW
    graphics_draw_line(ctx, GPoint(x + 11,y + 11),GPoint(x + 12,y + 12));// SE
  } else if (code == 1) {
    // Cloud: two overlapping circles + base rect
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_context_set_fill_color(ctx,  GColorLightGray);
    graphics_draw_circle(ctx, GPoint(x + 5,  y + 7), 4);
    graphics_draw_circle(ctx, GPoint(x + 9,  y + 6), 5);
    graphics_fill_rect(ctx, GRect(x + 1, y + 7, 12, 5), 0, GCornerNone);
  } else if (code == 2) {
    // Cloud + rain drops
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_circle(ctx, GPoint(x + 5, y + 5), 3);
    graphics_draw_circle(ctx, GPoint(x + 9, y + 4), 4);
    graphics_fill_rect(ctx, GRect(x + 1, y + 5, 11, 4), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorPictonBlue);
    graphics_draw_line(ctx, GPoint(x + 3,  y + 10), GPoint(x + 2,  y + 13));
    graphics_draw_line(ctx, GPoint(x + 7,  y + 10), GPoint(x + 6,  y + 13));
    graphics_draw_line(ctx, GPoint(x + 11, y + 10), GPoint(x + 10, y + 13));
  } else if (code == 3) {
    // Cloud + snow dots
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
    // Storm: cloud + lightning bolt
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

  // ── Black background ──────────────────────────────────────────────
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);


  // ── Digital time — upper right, dimmed accent, small ─────────────────
static char s_digital_buffer[6];
strftime(s_digital_buffer, sizeof(s_digital_buffer),
  clock_is_24h_style() ? "%H:%M" : "%I:%M", &s_last_time);

graphics_context_set_text_color(ctx, GColorWindsorTan);
graphics_draw_text(ctx, s_digital_buffer,
  s_dot_matrix_font,
  GRect(bounds.size.w - 106, 20, 86, 34),
  GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // ── Cardinal indicators (optional, compile-time toggle) ───────────
  #if SHOW_CARDINAL_INDICATORS
  for (int i = 0; i < 4; i++) {
    int32_t angle = TRIG_MAX_ANGLE * (i * 3) / 12;
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx,
      prv_hand_point(center, angle, DIAL_RADIUS - 10),
      prv_hand_point(center, angle, DIAL_RADIUS - 2));
  }
  #endif

// ── Minute hand (line from center, rounded tip) ───────────────────
int32_t min_angle = TRIG_MAX_ANGLE * s_last_time.tm_min / 60;
GPoint  min_tip   = prv_hand_point(center, min_angle, MIN_LEN);
graphics_context_set_stroke_color(ctx, GColorWhite);
graphics_context_set_stroke_width(ctx, 3);
graphics_draw_line(ctx, center, min_tip);
graphics_context_set_fill_color(ctx, GColorWhite);
graphics_fill_circle(ctx, min_tip, 1);

  // ── Hour dot (orange, orbits at fixed radius) ──────────────────────
  // Smooth: incorporates minutes for fluid movement
  int32_t hour_angle = (TRIG_MAX_ANGLE *
    (((s_last_time.tm_hour % 12) * 60) + s_last_time.tm_min)) / 720;
  GPoint hour_dot = prv_hand_point(center, hour_angle, HOUR_ORBIT);
  graphics_context_set_fill_color(ctx, GColorOrange);
  graphics_fill_circle(ctx, hour_dot, HOUR_DOT_R);

  // ── Center pivot (white circle, orange ring — ties to hour dot) ───
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, PIVOT_R);
  graphics_context_set_stroke_color(ctx, GColorOrange);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, PIVOT_R);

// ── Complications (vertical stack, right of center, below pivot) ──────
//
//        ●pivot
//        [cal] MON 14
//        [shoe] 4.2k stp
//        [sun] 68°F
//
  
#define COMP_X        CENTER_X                   // left edge of icon = pivot center
#define COMP_TEXT_X   (COMP_X + 16)              // left edge of text
#define COMP_TEXT_W   (bounds.size.w - COMP_TEXT_X - 6)
#define COMP_ROW_H    18
#define COMP_Y_START  (CENTER_Y + PIVOT_R + 8)

// Row 1: Calendar
prv_draw_icon_calendar(ctx, GPoint(COMP_X, COMP_Y_START));
graphics_context_set_text_color(ctx, GColorLightGray);
graphics_draw_text(ctx, s_date_buffer,
  fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
  GRect(COMP_TEXT_X, COMP_Y_START - 1, COMP_TEXT_W, 16),
  GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);


// Row 2: Steps
char steps_str[12];
if (s_step_count >= 1000) {
  snprintf(steps_str, sizeof(steps_str), "%d.%dk",
    s_step_count / 1000, (s_step_count % 1000) / 100);
} else {
  snprintf(steps_str, sizeof(steps_str), "%d", s_step_count);
}
prv_draw_icon_shoe(ctx, GPoint(COMP_X, COMP_Y_START + COMP_ROW_H));
graphics_context_set_text_color(ctx, GColorLightGray);
graphics_draw_text(ctx, steps_str,
  fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
  GRect(COMP_TEXT_X, COMP_Y_START + COMP_ROW_H - 1, COMP_TEXT_W, 16),
  GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);


// Row 3: Weather
prv_draw_icon_weather(ctx, GPoint(COMP_X, COMP_Y_START + COMP_ROW_H * 2), s_weather_code);
graphics_context_set_text_color(ctx, GColorOrange);
graphics_draw_text(ctx, s_weather_buffer,
  fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
  GRect(COMP_TEXT_X, COMP_Y_START + COMP_ROW_H * 2 - 3, COMP_TEXT_W, 16),
  GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // ── Bluetooth disconnect dot ──────────────────────────────────────
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
  resource_get_handle(RESOURCE_ID_FONT_DOT_MATRIX_28));

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