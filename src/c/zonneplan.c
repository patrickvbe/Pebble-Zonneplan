#include <pebble.h>

#include <string.h>
#include <ctype.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define INT_TO_FLOAT2(n) (n) / 1000, ((n) % 1000)/10
#define INT_TO_FLOAT3(n) (n) / 1000, (n) % 1000

#define FILLER_SIZE 10

static Window *s_window;
static TextLayer *s_text_layer;
static TextLayer *s_rate_layer;
static Layer *s_graph_layer;
static bool s_js_ready;

int s_top_area_height = 0;
int16_t s_bar_width = 0;
int16_t s_graph_offset = 0;
#define TEXTBUF_SIZE 30
static char s_textbuffer[TEXTBUF_SIZE];
#define RATE_BUF_SIZE 10
static char s_rate_buffer[RATE_BUF_SIZE];

// We get two days of data in the buffer.
#define STROOM_BUF_SIZE 24
int32_t s_stroom_today[STROOM_BUF_SIZE];
int32_t s_stroom_tomorrow[STROOM_BUF_SIZE];
int s_in_buf_today=0, s_in_buf_tomorrow=0;
int32_t s_tar_min=0, s_tar_max=0, s_display_min=0;
bool s_display_today = true; // false = tomorrow.

// Persistency of data, so we don't have to communicate each time we start the app.
#define STORAGE_KEY_IN_BUF_TODAY      0
#define STORAGE_KEY_IN_BUF_TOMORROW   1
#define STORAGE_KEY_STROOM_TODAY      2
#define STORAGE_KEY_STROOM_TOMORROW   3
#define STORAGE_KEY_SETTINGS          4

// Define our settings struct
typedef struct Settings {
  GColor BackgroundColor;
  GColor TextColor;
  GColor ForegroundColorPast;
  GColor ForegroundColorFuture;
  GColor HighlightColor;
  int32_t InkoopVergoeding;  // * 1000
  int32_t EnergieBelasting;  // * 1000
  int32_t BTW;  // * 1000
} Settings;
Settings s_settings;
bool s_settings_changed = false;  // So we know we should save it.

// App sync
// static AppSync s_sync;
// static uint8_t s_sync_buffer[64];
int s_ymd_today = 0, s_ymd_tomorrow = 0;
int s_hour_now = 0;
int s_highlight_hour = 0;

int tm_to_int(struct tm *t) {
  return (t->tm_year+1900)*10000 + (t->tm_mon+1)*100 + t->tm_mday;
}

static void update_time() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_ymd_today = tm_to_int(t);
  s_hour_now = t->tm_hour;
  now +=  SECONDS_PER_DAY;
  t = localtime(&now);
  s_ymd_tomorrow = tm_to_int(t);
}

void request_stroom(int date) {
  DictionaryIterator *out_iter;
  AppMessageResult result = app_message_outbox_begin(&out_iter);
  if(result == APP_MSG_OK) {
    dict_write_int(out_iter, MESSAGE_KEY_RequestData, &date, sizeof(int), true);
    result = app_message_outbox_send();
    if(result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
    }
  } else {
    // The outbox cannot be used right now
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
  }
}

void update_mm(int32_t* buf) {
  for ( int idx=0; idx < STROOM_BUF_SIZE; idx++ ) {
    s_tar_min = MIN(s_tar_min, buf[idx]);
    s_tar_max = MAX(s_tar_max, buf[idx]);
  }
}

bool has_valid_data_for_selection() {
  return (s_display_today && s_in_buf_today == s_ymd_today) || (!s_display_today && s_in_buf_tomorrow == s_ymd_tomorrow);
}

int32_t multiply1000(int32_t a, int32_t b) {
  return ((int64_t)a * (int64_t)b) / 1000;
}

int32_t calc_rate(int rate) {
  return multiply1000(rate, s_settings.BTW) + s_settings.InkoopVergoeding + s_settings.EnergieBelasting;
}

void update_text() {
  if ( s_in_buf_today == 0 && s_in_buf_tomorrow == 0 ) {
    snprintf(s_textbuffer, TEXTBUF_SIZE, "Geen gegevens");
  } else {
    int32_t* data = s_display_today ? s_stroom_today : s_stroom_tomorrow;
    int ymd = s_display_today ? s_ymd_today : s_ymd_tomorrow;
    int32_t min = calc_rate(s_tar_min);
    int32_t max = calc_rate(s_tar_max);
    int32_t rate = calc_rate(data[s_highlight_hour]);
    snprintf(s_textbuffer, TEXTBUF_SIZE, "%ld.%02ld-%ld.%02ld\n%d-%d-%d %d:00", INT_TO_FLOAT2(min), INT_TO_FLOAT2(max), ymd % 100, (ymd/100) % 100, ymd / 10000, s_highlight_hour);
    if ( has_valid_data_for_selection() ) {
      snprintf(s_rate_buffer, RATE_BUF_SIZE, "%ld.%02ld", INT_TO_FLOAT2(rate));
    } else {
      snprintf(s_rate_buffer, RATE_BUF_SIZE, "-.-");
    }
  }
  text_layer_set_text(s_text_layer, s_textbuffer);
  text_layer_set_text(s_rate_layer, s_rate_buffer);
}

void redraw() {
  layer_mark_dirty(s_graph_layer);
  update_text();
}

void set_display_today(bool value) {
  if ( (s_display_today = value) ) {
    s_highlight_hour = s_hour_now;
  } else {
    s_highlight_hour = 12;
  }
  redraw();
}

void data_updated() {
  // Calculate statistics over both days.
  s_tar_min = s_in_buf_today != 0 ? s_stroom_today[0] : s_in_buf_tomorrow != 0 ? s_stroom_tomorrow[0] : 0;
  s_tar_max = s_tar_min;
  if ( s_in_buf_today    != 0 ) update_mm(s_stroom_today);
  if ( s_in_buf_tomorrow != 0 ) update_mm(s_stroom_tomorrow);
  s_display_min = s_tar_min > 0 ? 0 : s_tar_min;
  set_display_today(true);
}

void synchronize_data() {
  update_time();

  // If we went to the next day, we can take-over the data of tomorrow we already got.
  if ( s_in_buf_tomorrow == s_ymd_today )
  {
    s_in_buf_today = s_in_buf_tomorrow;
    s_in_buf_tomorrow = 0;
    memcpy(s_stroom_today, s_stroom_tomorrow, sizeof(s_stroom_today));
    persist_write_int(STORAGE_KEY_IN_BUF_TOMORROW, s_in_buf_tomorrow);
    persist_write_int(STORAGE_KEY_IN_BUF_TODAY, s_in_buf_today);
    persist_write_data(STORAGE_KEY_STROOM_TODAY, s_stroom_today, sizeof(s_stroom_today));
    data_updated();
  }
  if ( s_in_buf_today != s_ymd_today ) {
    request_stroom(s_ymd_today);
  } else if ( s_in_buf_tomorrow != s_ymd_tomorrow ) {
    request_stroom(s_ymd_tomorrow);
  }

  // ToDo: Add timer to fire e.g. every minute (and increase interval) when today not in sync or after 13:00 and tomorrow not in sync.
  // ToDo: Add timer to fire e.g. at 13:00 when tomorrow not in sync.
}

void update_stroom_received(Tuple* tuple) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Stroom received");
  int32_t* pbuffer = (int32_t*)tuple->value->data;
  int date = (int)pbuffer[0];
  //int32_t belasting = pbuffer[1];
  int count = (int)pbuffer[2];
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Received data for %d, %d items.", date, count);
  
  // See which day / part of the buffer got the update
  int32_t* targetbuf = NULL;
  int* target_ymd = NULL;
  uint32_t target_in_buf_key = 0, target_stroom_key = 0;
  if ( date == s_ymd_today ) {
    target_ymd = &s_in_buf_today;
    targetbuf = s_stroom_today;
    target_in_buf_key = STORAGE_KEY_IN_BUF_TODAY;
    target_stroom_key = STORAGE_KEY_STROOM_TODAY;
  } else if ( date == s_ymd_tomorrow ) {
    target_ymd = &s_in_buf_tomorrow;
    targetbuf = s_stroom_tomorrow;
    target_in_buf_key = STORAGE_KEY_IN_BUF_TOMORROW;
    target_stroom_key = STORAGE_KEY_STROOM_TOMORROW;
  } else return;

  // Process the update
  if ( count == STROOM_BUF_SIZE ) {
    *target_ymd = date;
    memcpy(targetbuf, &pbuffer[3], sizeof(s_stroom_today));
    if ( date == s_ymd_today ) synchronize_data(); // We can already get tomorrow, if needed.
  } else {
    *target_ymd = 0;
  }
  persist_write_int(target_in_buf_key, *target_ymd);
  persist_write_data(target_stroom_key, targetbuf, sizeof(s_stroom_today));

  data_updated();
}

int32_t str_to_int100000(const char* s) {
  bool negative = false;
  bool infraction = false;
  int32_t result = 0;
  int decimals = 5;
  while ( decimals > 0 ) {
    result *= 10;
    if ( *s == 0 ) {
      decimals--;
      continue;
    }
    if ( *s == '-' ) negative = true;
    else if ( *s == '.' || *s == ',' ) infraction = true;
    else if ( isdigit((int)*s) ) {
      result += *s - '0';
      if ( infraction ) decimals--;
    }
    s++;
  }
  return negative ? -result : result;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* tp = dict_read_first(iter);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Inbox received %ld", tp->key);
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_JSReady);
  if(tuple) {
    // PebbleKit JS is ready! Safe to send messages
    APP_LOG(APP_LOG_LEVEL_DEBUG, "JSReady received");
    s_js_ready = true;
    synchronize_data();
    return;
  }
  tuple = dict_find(iter, MESSAGE_KEY_Stroom);
  if(tuple) {
    update_stroom_received(tuple);
    return;
  }
  // Settings
  tuple = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if(tuple) {
    s_settings.BackgroundColor = GColorFromHEX(tuple->value->int32);
    layer_mark_dirty(s_graph_layer);
    text_layer_set_background_color(s_text_layer, s_settings.BackgroundColor);
    text_layer_set_background_color(s_rate_layer, s_settings.BackgroundColor);
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_TextColor);
  if(tuple) {
    s_settings.TextColor = GColorFromHEX(tuple->value->int32);
    text_layer_set_text_color(s_text_layer, s_settings.TextColor);
    text_layer_set_text_color(s_rate_layer, s_settings.TextColor);
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_ForegroundColorPast);
  if(tuple) {
    s_settings.ForegroundColorPast = GColorFromHEX(tuple->value->int32);
    layer_mark_dirty(s_graph_layer);
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_ForegroundColorFuture);
  if(tuple) {
    s_settings.ForegroundColorFuture = GColorFromHEX(tuple->value->int32);
    layer_mark_dirty(s_graph_layer);
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_HighlightColor);
  if(tuple) {
    s_settings.HighlightColor = GColorFromHEX(tuple->value->int32);
    layer_mark_dirty(s_graph_layer);
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_EnergieBelasting);
  if(tuple) {
    s_settings.EnergieBelasting = str_to_int100000(tuple->value->cstring);
    update_text();
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_BTW);
  if(tuple) {
    s_settings.BTW = str_to_int100000(tuple->value->cstring) / 10000 + 1000;
    update_text();
    s_settings_changed = true;
  }
  tuple = dict_find(iter, MESSAGE_KEY_InkoopVergoeding);
  if(tuple) {
    s_settings.InkoopVergoeding = str_to_int100000(tuple->value->cstring);
    update_text();
    s_settings_changed = true;
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  set_display_today(!s_display_today);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if ( ++s_highlight_hour > 23 ) {
    if ( s_display_today ) {
      s_display_today = false;
      s_highlight_hour = 0;
    } else {
      s_highlight_hour--;
      return;
    }
  }
  redraw();
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if ( --s_highlight_hour < 0 ) {
    if ( !s_display_today ) {
      s_display_today = true;
      s_highlight_hour = 23;
    } else {
      s_highlight_hour++;
      return;
    }
  }
  redraw();
}

static void graph_update_proc(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "graph_update_proc");
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_settings.BackgroundColor);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if ( !has_valid_data_for_selection() ) return;

  const int32_t tar_per_pixel = (s_tar_max - s_display_min) / bounds.size.h;
  GRect rect = GRect(s_graph_offset, 0, s_bar_width - 1, 0);
  int32_t* data = s_display_today ? s_stroom_today : s_stroom_tomorrow;
  for ( int hour=0; hour < STROOM_BUF_SIZE; hour++ ) {
    if ( hour == s_highlight_hour ) {
      graphics_context_set_fill_color(ctx, s_settings.HighlightColor);
    } else if ( !s_display_today || hour >= s_hour_now ) {
      graphics_context_set_fill_color(ctx, s_settings.ForegroundColorFuture);
    } else {
      graphics_context_set_fill_color(ctx, s_settings.ForegroundColorPast);
    }
    rect.size.h = MIN((data[hour]-s_display_min) / tar_per_pixel, bounds.size.h);
    rect.origin.y = bounds.size.h - rect.size.h;
    graphics_fill_rect(ctx, rect, 3, GCornersTop);
    rect.origin.x += s_bar_width;
  }
  // for ( int idx=1; idx < 4; idx++ ) {
  //   graphics_context_set_stroke_color(ctx, GColorBlue);
  //   const int16_t x = bounds.origin.x + s_bar_width * 6 * idx - 1;
  //   graphics_draw_line(ctx, GPoint(x, bounds.origin.y), GPoint(x,bounds.size.h));
  // }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GSize font_size = graphics_text_layout_get_content_size("1", font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  s_top_area_height = font_size.h * 2;

  s_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, s_top_area_height));
  text_layer_set_font(s_text_layer, font);
  text_layer_set_background_color(s_text_layer, s_settings.BackgroundColor);
  text_layer_set_text_color(s_text_layer, s_settings.TextColor);
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));
  
  font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  font_size = graphics_text_layout_get_content_size("1", font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  
  s_rate_layer = text_layer_create(GRect(0, s_top_area_height, bounds.size.w, font_size.h + FILLER_SIZE));
  text_layer_set_font(s_rate_layer, font);
  text_layer_set_background_color(s_rate_layer, s_settings.BackgroundColor);
  text_layer_set_text_color(s_rate_layer, s_settings.TextColor);
  text_layer_set_text(s_rate_layer, "-.-");
  text_layer_set_text_alignment(s_rate_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_rate_layer));
  s_top_area_height += font_size.h + FILLER_SIZE;
  
  s_bar_width = bounds.size.w / STROOM_BUF_SIZE;
  s_graph_offset = (bounds.size.w - s_bar_width * STROOM_BUF_SIZE) / 2; // Center the graph area.
  s_graph_layer = layer_create(GRect(0, s_top_area_height, bounds.size.w, bounds.size.h - s_top_area_height));
  layer_set_update_proc(s_graph_layer, graph_update_proc);
  layer_add_child(window_layer, s_graph_layer);

  if ( s_in_buf_today != 0 || s_in_buf_tomorrow != 0 ) {
    update_time();
    data_updated();
  }
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
  text_layer_destroy(s_rate_layer);
  layer_destroy(s_graph_layer);
}

static void prv_init(void) {
  // Get data from storage
  s_settings = (struct Settings){GColorBlack, GColorWhite, GColorDarkGreen, GColorMediumSpringGreen, GColorGreen, 2000, 11080, 1210};
  persist_read_data(STORAGE_KEY_SETTINGS, &s_settings, sizeof(s_settings));
  if ( persist_exists(STORAGE_KEY_IN_BUF_TODAY) ) {
    s_in_buf_today = persist_read_int(STORAGE_KEY_IN_BUF_TODAY);
    persist_read_data(STORAGE_KEY_STROOM_TODAY, s_stroom_today, sizeof(s_stroom_today));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read today: %d", s_in_buf_today);
  }
  if ( persist_exists(STORAGE_KEY_IN_BUF_TOMORROW) ) {
    s_in_buf_tomorrow = persist_read_int(STORAGE_KEY_IN_BUF_TOMORROW);
    persist_read_data(STORAGE_KEY_STROOM_TOMORROW, s_stroom_tomorrow, sizeof(s_stroom_tomorrow));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read tomorrow: %d", s_in_buf_tomorrow);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(256, 256);
}

static void prv_deinit(void) {
  window_destroy(s_window);

  // Store settings before we exit.
  if ( s_settings_changed ) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing settings");
    persist_write_data(STORAGE_KEY_SETTINGS, &s_settings, sizeof(s_settings));
  }
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
