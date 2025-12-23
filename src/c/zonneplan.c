#include <pebble.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static Window *s_window;
static TextLayer *s_text_layer;
static Layer *s_graph_layer;
static bool s_js_ready;

#define TEXTBUF_SIZE 16
static char s_textbuffer[TEXTBUF_SIZE];

#define STROOM_TARIEF_COUNT 24
int32_t s_stroom_tarief[STROOM_TARIEF_COUNT];

// App sync
// static AppSync s_sync;
// static uint8_t s_sync_buffer[64];

static void request_stroom() {
  DictionaryIterator *out_iter;
  AppMessageResult result = app_message_outbox_begin(&out_iter);
  if(result == APP_MSG_OK) {
    int value = 20251218;
    dict_write_int(out_iter, MESSAGE_KEY_RequestData, &value, sizeof(int), true);
    result = app_message_outbox_send();
    if(result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
    }
  } else {
    // The outbox cannot be used right now
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
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
  text_layer_set_text(s_text_layer, "Select");
  request_stroom();
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Up");
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Down");
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* tp = dict_read_first(iter);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Inbox received %ld", tp->key);
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_JSReady);
  if(tuple) {
    // PebbleKit JS is ready! Safe to send messages
    APP_LOG(APP_LOG_LEVEL_DEBUG, "JSReady received");
    s_js_ready = true;
    return;
  }
  tuple = dict_find(iter, MESSAGE_KEY_Stroom);
  if(tuple) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Stroom received");
    memcpy(s_stroom_tarief, tuple->value->data, sizeof(s_stroom_tarief));
    snprintf(s_textbuffer, TEXTBUF_SIZE, "%ld ct", s_stroom_tarief[1]);
    text_layer_set_text(s_text_layer, s_textbuffer);
    layer_mark_dirty(s_graph_layer);
    return;
  }
}

static void graph_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorGreen);
  const int16_t bar_width = bounds.size.w / STROOM_TARIEF_COUNT;
  int32_t tar_min = s_stroom_tarief[0], tar_max = s_stroom_tarief[0];
  for ( int idx=1; idx < STROOM_TARIEF_COUNT; idx++ ) {
    tar_min = MIN(tar_min, s_stroom_tarief[idx]);
    tar_max = MAX(tar_max, s_stroom_tarief[idx]);
  }
  const int16_t min_bar_y = bounds.origin.y + 10;
  const int16_t max_bar_size = bounds.size.h - 20;
  const int32_t tar_per_pixel = (tar_max - tar_min) / max_bar_size;
  GRect rect = GRect(bounds.origin.x, 0, bar_width - 1, 0);
  for ( int idx=1; idx < STROOM_TARIEF_COUNT; idx++ ) {
    const int16_t bar_height = (s_stroom_tarief[idx]-tar_min) / tar_per_pixel;
    rect.origin.y = min_bar_y + max_bar_size - bar_height;
    rect.size.h = bounds.size.h - rect.origin.y;
    graphics_fill_rect(ctx, rect, 1, GCornersTop);
    rect.origin.x += bar_width;
  }
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

  s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  text_layer_set_text(s_text_layer, "Press a button");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
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
