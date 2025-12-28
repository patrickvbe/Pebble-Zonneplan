#include <pebble.h>

#include <string.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define INT_TO_FLOAT(n) n / 1000, n % 1000

static Window *s_window;
static TextLayer *s_text_layer;
static Layer *s_graph_layer;
static bool s_js_ready;

#define TOP_AREA 50
#define TEXTBUF_SIZE 100
static char s_textbuffer[TEXTBUF_SIZE];

// We get two days of data in the buffer.
#define STROOM_BUF_SIZE 24
int32_t s_stroom_today[STROOM_BUF_SIZE];
int32_t s_stroom_tomorrow[STROOM_BUF_SIZE];
int s_in_buf_today=0, s_in_buf_tomorrow=0;
int32_t s_tar_min=0, s_tar_max=0, s_display_min=0;
bool display_today = true; // false = tomorrow.

// App sync
// static AppSync s_sync;
// static uint8_t s_sync_buffer[64];
int s_ymd_today = 0, s_ymd_tomorrow = 0;
int s_hour_now = 0;

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

void data_updated() {
  // Calculate statistics over both days.
  s_tar_min = s_in_buf_today != 0 ? s_stroom_today[0] : s_in_buf_tomorrow != 0 ? s_stroom_tomorrow[0] : 0;
  s_tar_max = s_tar_min;
  if ( s_in_buf_today    != 0 ) update_mm(s_stroom_today);
  if ( s_in_buf_tomorrow != 0 ) update_mm(s_stroom_tomorrow);
  s_display_min = s_tar_min > 0 ? 0 : s_tar_min;
  layer_mark_dirty(s_graph_layer);
  
  // Display textual information.
  s_textbuffer[0] = 0;
  if ( s_in_buf_today == 0 && s_in_buf_tomorrow == 0 ) {
    snprintf(s_textbuffer, TEXTBUF_SIZE, "Geen gegevens");
  } else {
    snprintf(s_textbuffer, TEXTBUF_SIZE, "%d-%d-%d %d:00: ", s_ymd_today % 100, (s_ymd_today/100) % 100, s_ymd_today / 10000, s_hour_now);
    int buflen = strlen(s_textbuffer);
    int rest_size = TEXTBUF_SIZE - buflen;
    if ( s_in_buf_today == 0 ) {
      snprintf(s_textbuffer + buflen, rest_size, "Geen gegevens");
    } else {
      snprintf(s_textbuffer + buflen, rest_size, "%ld,%03ld", INT_TO_FLOAT(s_stroom_today[s_hour_now]));
    }
    buflen = strlen(s_textbuffer);
    rest_size = TEXTBUF_SIZE - buflen;
    snprintf(s_textbuffer + buflen, rest_size, "\nMin: %ld,%03ld ct\nMax: %ld,%03ld ct", INT_TO_FLOAT(s_tar_min), INT_TO_FLOAT(s_tar_max));
  }
  text_layer_set_text(s_text_layer, s_textbuffer);
}

void synchronize_data() {
  update_time();

  // If we went to the next day, we can take-over the data of tomorrow we already got.
  if ( s_in_buf_tomorrow == s_ymd_today )
  {
    s_in_buf_today = s_in_buf_tomorrow;
    s_in_buf_tomorrow = 0;
    memcpy(s_stroom_today, s_stroom_tomorrow, sizeof(s_stroom_today));
    data_updated();
  }
  if ( s_in_buf_today != s_ymd_today ) {
    request_stroom(s_ymd_today);
  } else if ( s_in_buf_tomorrow != s_ymd_tomorrow ) {
    request_stroom(s_ymd_tomorrow);
  }
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
  if ( date == s_ymd_today ) {
    target_ymd = &s_in_buf_today;
    targetbuf = s_stroom_today;
  } else if ( date == s_ymd_tomorrow ) {
    target_ymd = &s_in_buf_tomorrow;
    targetbuf = s_stroom_tomorrow;
  } else return;

  // Process the update
  if ( count == STROOM_BUF_SIZE ) {
    *target_ymd = date;
    memcpy(targetbuf, &pbuffer[3], sizeof(s_stroom_today));
    if ( date == s_ymd_today ) synchronize_data(); // We can already get tomorrow, if needed.
  } else {
    *target_ymd = 0;
  }

  data_updated();
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
}


// static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "Changed callback %ld", key);
//   if (key == MESSAGE_KEY_Stroom) {
//       APP_LOG(APP_LOG_LEVEL_DEBUG, "Stroom Changed %ld", new_tuple->value->int32);
//       snprintf(s_textbuffer, TEXTBUF_SIZE, "%ld ct", new_tuple->value->int32);
//       text_layer_set_text(s_text_layer, s_textbuffer);
//   }
// }

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  display_today = !display_today;
  layer_mark_dirty(s_graph_layer);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Up");
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Down");
}

static void graph_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if ( (display_today && s_in_buf_today != s_ymd_today) || (!display_today && s_in_buf_tomorrow != s_ymd_tomorrow) ) return;

  const int16_t bar_width = bounds.size.w / STROOM_BUF_SIZE;
  const int16_t min_bar_y = bounds.origin.y + TOP_AREA;
  const int16_t max_bar_size = bounds.size.h - TOP_AREA;
  const int32_t tar_per_pixel = (s_tar_max - s_display_min) / max_bar_size;
  int hour = 0;
  GRect rect = GRect(bounds.origin.x, 0, bar_width - 1, 0);
  int32_t* data = display_today ? s_stroom_today : s_stroom_tomorrow;
  for ( int idx=0; idx < STROOM_BUF_SIZE; idx++ ) {
    if ( !display_today || hour > s_hour_now ) {
      graphics_context_set_fill_color(ctx, GColorMayGreen);
    } else if ( hour == s_hour_now ) {
      graphics_context_set_fill_color(ctx, GColorGreen);
    } else {
      graphics_context_set_fill_color(ctx, GColorDarkGreen);
    }
    hour++;
    const int16_t bar_height = (data[idx]-s_display_min) / tar_per_pixel;
    rect.origin.y = min_bar_y + max_bar_size - bar_height;
    rect.size.h = bounds.size.h - rect.origin.y;
    graphics_fill_rect(ctx, rect, 1, GCornersTop);
    rect.origin.x += bar_width;
  }
  // for ( int idx=1; idx < 4; idx++ ) {
  //   graphics_context_set_stroke_color(ctx, GColorBlue);
  //   const int16_t x = bounds.origin.x + bar_width * 6 * idx - 1;
  //   graphics_draw_line(ctx, GPoint(x, bounds.origin.y), GPoint(x,bounds.size.h));
  // }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_graph_layer = layer_create(bounds);
  layer_set_update_proc(s_graph_layer, graph_update_proc);
  layer_add_child(window_layer, s_graph_layer);

  s_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, TOP_AREA));
  text_layer_set_background_color(s_text_layer, GColorBlack);
  text_layer_set_text_color(s_text_layer, GColorWhite);
  text_layer_set_text(s_text_layer, "Press a button");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));

  // Tuplet initial_values[] = {
  //   TupletInteger(MESSAGE_KEY_Stroom, (int32_t) 1),
  // };
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "app_sync_init");
  // app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
  //     initial_values, ARRAY_LENGTH(initial_values),
  //     sync_tuple_changed_callback, NULL, NULL);

}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
  layer_destroy(s_graph_layer);
}

static void prv_init(void) {
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
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
