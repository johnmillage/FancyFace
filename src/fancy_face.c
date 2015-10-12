#include <pebble.h>

#define COLOR_SCHEME_KEY 1
#define COLOR_TYPE_KEY 2
#define TEMP_TYPE_KEY 3
#define SECONDS_KEY 4
    
#define PHONE_TEMPERATURE_KEY 0
#define PHONE_CLOUDS_KEY 1
#define PHONE_COLOR_KEY 2
#define PHONE_TEMP_TYPE_KEY 3
#define PHONE_TEMPERATURE_C_KEY 4
#define PHONE_SECONDS_KEY 5
    
static Window *window;
static Layer *s_simple_bg_layer, *s_date_layer, *s_hands_layer;
static TextLayer *s_day_label, *s_num_label, *s_twelve_label, *s_temperature_label, *s_pebble_label;
static AppSync s_sync;
static uint8_t s_sync_buffer[96];
static int32_t s_clouds_pct;
static char s_num_buffer[4], s_day_buffer[6], s_temp_buffer[6], s_temp_c_buffer[6];
static int32_t s_cur_color_scheme;
static int32_t s_cur_color_type;
static int32_t s_cur_temp_type;
static GColor s_back_color;
static GColor s_fore_color;
static GColor s_fore_light;
static GColor s_fore_dark;
static GColor s_fore_special;
static bool s_needs_weather;
static int32_t s_show_seconds;

#ifdef PBL_SDK_3
static void graphics_draw_line2(GContext *ctx, GPoint p0, GPoint p1){
    graphics_draw_line(ctx, p0, p1);
}
#else
static uint8_t s_line_width;

static void graphics_context_set_stroke_width(GContext *ctx, uint8_t width){
    s_line_width = width;
}

// Draw line with width
// (Based on code found here http://rosettacode.org/wiki/Bitmap/Bresenham's_line_algorithm#C)
static void graphics_draw_line2(GContext *ctx, GPoint p0, GPoint p1) {
    if(s_line_width == 1){
        graphics_draw_line(ctx, p0, p1);
        return;
    }

  // Order points so that lower x is first
  int16_t x0, x1, y0, y1;
  if (p0.x <= p1.x) {
    x0 = p0.x; x1 = p1.x; y0 = p0.y; y1 = p1.y;
  } else {
    x0 = p1.x; x1 = p0.x; y0 = p1.y; y1 = p0.y;
  }
  
  // Init loop variables
  int16_t dx = x1-x0;
  int16_t dy = abs(y1-y0);
  
  int16_t sy = y0<y1 ? 1 : -1; 
  int16_t err = (dx>dy ? dx : -dy)/2;
  int16_t e2;
  
  // Calculate whether line thickness will be added vertically or horizontally based on line angle
  int8_t xdiff, ydiff;
  
  if (dx > dy) {
    xdiff = 0;
    ydiff = s_line_width/2;
  } else {
    xdiff = s_line_width/2;
    ydiff = 0;
  }
  bool first = true;
  bool last = false;
  // Use Bresenham's integer algorithm, with slight modification for line width, to draw line at any angle
  while (true) {
    // Draw line thickness at each point by drawing another line 
    // (horizontally when > +/-45 degrees, vertically when <= +/-45 degrees)
    last = (x0==x1 && y0==y1);
    if(first || last){
        if(dx > dy){
            graphics_draw_line(ctx, GPoint(x0-xdiff, y0-ydiff+1), GPoint(x0+xdiff, y0+ydiff-1));
        }
        else{
            graphics_draw_line(ctx, GPoint(x0-xdiff+1, y0-ydiff), GPoint(x0+xdiff-1, y0+ydiff));
        }
        first = false;
    }
    else{
        graphics_draw_line(ctx, GPoint(x0-xdiff, y0-ydiff), GPoint(x0+xdiff, y0+ydiff));
    }
    
    
    if (x0==x1 && y0==y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0++; }
    if (e2 < dy) { err += dx; y0 += sy; }
  }
 
}
#endif
static void draw_military_ticks(Layer *layer, GContext *ctx)
{
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_context_set_stroke_color(ctx,  s_fore_color);
    for(int32_t i = 1; i < 24; i++)
    {
        int32_t second_angle = TRIG_MAX_ANGLE * i / 24;
        
        
        int32_t len1 = 0;
        int32_t len2 = 0;
        int32_t wid = 0;
        if(i == 1 || i == 11 || i == 13 || i == 23){
            len1 = 88;
            len2 = 76;
            wid = 4;
        }
        else if(i == 3 || i == 9 || i == 15 || i == 21)
        {
            len1 = 120;
            len2 = 103;   
            wid = 6;
        }
        else if(i == 4 || i == 8 || i == 16 || i == 20)
        {
            len1 = 72;
            len2 = 85;   
            wid = 5;
        }
        else
        {
            continue;
        }
        
        int32_t xval = i > 12 ? center.x - 1 : center.x;
        GPoint second_start = 
        {
            .x = (int16_t)(sin_lookup(second_angle) * (len2 - wid) / TRIG_MAX_RATIO) + xval,
            .y = (int16_t)(-cos_lookup(second_angle) * (len1 - wid) / TRIG_MAX_RATIO) + center.y,
        };

        GPoint second_end = 
        {
            .x = (int16_t)(sin_lookup(second_angle) * len2 / TRIG_MAX_RATIO) + xval,
            .y = (int16_t)(-cos_lookup(second_angle) * len1 / TRIG_MAX_RATIO) + center.y,
        };
        // minute ticks
        graphics_draw_line2(ctx, second_start, second_end);
    }
}

static void draw_minute_ticks(Layer *layer, GContext *ctx)
{
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    for(int32_t i = 2; i < 59; i++)
    {
        int32_t second_angle = TRIG_MAX_ANGLE * i / 60;
        int32_t start = ( i % 5 == 0 ?  ((i == 15 || i == 45 || i == 30) ?  64 : 58) : 68);
        int32_t xval = i > 30 ? center.x - 1: center.x;
        GPoint second_start = 
        {
            .x = (int16_t)(sin_lookup(second_angle) * start / TRIG_MAX_RATIO) + xval,
            .y = (int16_t)(-cos_lookup(second_angle) * start / TRIG_MAX_RATIO) + center.y,
        };

        GPoint second_end = 
        {
            .x = (int16_t)(sin_lookup(second_angle) * 72 / TRIG_MAX_RATIO) + xval,
            .y = (int16_t)(-cos_lookup(second_angle) * 72 / TRIG_MAX_RATIO) + center.y,
        };


        // minute ticks
        graphics_context_set_stroke_width(ctx, i % 5 == 0 ? 3 : 1);
        graphics_context_set_stroke_color(ctx, i % 5 == 0 ? s_fore_light : s_fore_color);
        graphics_draw_line2(ctx, second_start, second_end);
         
    }
}

static void draw_compass_ticks(Layer *layer, GContext *ctx)
{
    GPoint center = {.x = 72, .y = 122};
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, s_fore_color);
    for(int32_t i = 1; i < 15; i++)
    {
        if( i == 3 || i == 4 || i == 7 || i == 8 || i == 11 || i == 12 )
            continue;
        
        int32_t second_angle = TRIG_MAX_ANGLE * i / 15;
            GPoint second_start = 
            {
                .x = (int16_t)(sin_lookup(second_angle) * 18 / TRIG_MAX_RATIO) + center.x,
                .y = (int16_t)(-cos_lookup(second_angle) * 18 / TRIG_MAX_RATIO) + center.y,
            };
          
            GPoint second_end = 
             {
                .x = (int16_t)(sin_lookup(second_angle) * 21 / TRIG_MAX_RATIO) + center.x,
                .y = (int16_t)(-cos_lookup(second_angle) * 21 / TRIG_MAX_RATIO) + center.y,
              };
        
        
            // minute ticks
            graphics_draw_line2(ctx, second_start, second_end);
    }
    
    CompassHeadingData chd;
    compass_service_peek(&chd);
    if(chd.compass_status != CompassStatusDataInvalid)
    {
          GPoint comp_start = 
            {
                .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE - chd.true_heading) * 19 / TRIG_MAX_RATIO) + center.x,
                .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE - chd.true_heading) * 19 / TRIG_MAX_RATIO) + center.y,
            };
            graphics_context_set_fill_color(ctx, s_fore_special);
            graphics_fill_circle(ctx, comp_start, 3);
    }
}

static void set_color_scheme(){
    #ifdef PBL_PLATFORM_BASALT
    if(s_cur_color_type == 3){
        s_cur_color_scheme = 0;
    }
    else if(s_cur_color_type == 4){
        s_cur_color_scheme = 1;
    }
    else if(s_cur_color_type == 5){
        s_cur_color_scheme = 2;
    }
    else if(s_cur_color_type == 6){
        s_cur_color_scheme = 3;
    }
    else if(s_cur_color_type == 7){
        s_cur_color_scheme = 4;
    }
    else if(s_cur_color_type == 8){
        s_cur_color_scheme = 5;
    }
    else if(s_cur_color_type == 1){
        if(s_cur_color_scheme == 0)
            s_cur_color_scheme = 1;
        else if(s_cur_color_scheme == 3 || s_cur_color_scheme == 4)
            s_cur_color_scheme = 5;
    }
    else if(s_cur_color_type == 2){
         if(s_cur_color_scheme == 1 || s_cur_color_scheme == 2)
            s_cur_color_scheme = 3;
        else if(s_cur_color_scheme == 5)
            s_cur_color_scheme = 0;
    }
    
    switch(s_cur_color_scheme){
        case 0:
            s_back_color = GColorWhite;
            s_fore_color = GColorBlack;
            s_fore_light = GColorDarkGray;
            s_fore_dark = GColorLightGray;
            s_fore_special = GColorRed;
        break;
        case 1:
            s_back_color = GColorBlack;
            s_fore_color = GColorWhite;
            s_fore_light = GColorLightGray;
            s_fore_dark = GColorDarkGray;
            s_fore_special = GColorChromeYellow;
        break;
        case 2:
            s_back_color = GColorBlue;
            s_fore_color = GColorWhite;
            s_fore_light = GColorCeleste;
            s_fore_dark = GColorVividCerulean;
            s_fore_special = GColorYellow;
        break;
        case 3:
            s_back_color = GColorYellow;
            s_fore_color = GColorBlack;
            s_fore_light = GColorWindsorTan;
            s_fore_dark = GColorChromeYellow;
            s_fore_special = GColorBlue;
            break;
        case 4:
            s_back_color = GColorGreen;
            s_fore_color = GColorBlack;
            s_fore_light = GColorDarkGreen;
            s_fore_dark = GColorIslamicGreen;
            s_fore_special = GColorRed;
        break;
        case 5:
            s_back_color = GColorRed;
            s_fore_color = GColorWhite;
            s_fore_light = GColorMelon;
            s_fore_dark = GColorSunsetOrange;
            s_fore_special = GColorScreaminGreen;
            break;
        
    }
    #else
        s_line_width = 1;
        APP_LOG(APP_LOG_LEVEL_ERROR, "before %d %d", (int)s_cur_color_type, (int)s_cur_color_scheme );
        if(s_cur_color_type != 0)
            {
             if(s_cur_color_type == 2 ||  s_cur_color_type == 3 || s_cur_color_type == 5 || s_cur_color_type == 6){
                s_cur_color_scheme = 0;
             }
            else{
                s_cur_color_scheme = 1;
            }
        }
       
        APP_LOG(APP_LOG_LEVEL_ERROR, "after %d %d", (int)s_cur_color_type, (int)s_cur_color_scheme );
        switch(s_cur_color_scheme){
        case 0:
        case 2:
        case 4:
            s_back_color = GColorWhite;
            s_fore_color = GColorBlack;
            s_fore_light = GColorBlack;
            s_fore_dark = GColorBlack;
            s_fore_special = GColorBlack;
        break;
        case 1:
        case 3:
        case 5:
            s_back_color = GColorBlack;
            s_fore_color = GColorWhite;
            s_fore_light = GColorWhite;
            s_fore_dark = GColorWhite;
            s_fore_special = GColorWhite;
        break;
        
    }
    #endif
}

static void draw_partial_circle( Layer *layer, GContext *ctx, uint32_t pct, GPoint pt){
    GPoint last_point = {.x = 0, .y = 0};
    uint8_t btHalf = (((pct) / 2) * 36) / 10;
    
    for(int i = 0; i < 360; i++){
        int32_t cur_angle = TRIG_MAX_ANGLE * i / 360;
        bool emptypart = ( i  < btHalf || i > 360 - btHalf);
        if( emptypart ){
            #ifdef PBL_PLATFORM_BASALT
            graphics_context_set_stroke_color(ctx, s_fore_dark);
            #else
            graphics_context_set_stroke_color(ctx, s_back_color);
            #endif
        }
        else{
            graphics_context_set_stroke_color(ctx, s_fore_special);
        }
        GPoint new_point = 
        {
            .x = (int16_t)(sin_lookup(cur_angle) * 21 / TRIG_MAX_RATIO) + pt.x,
            .y = (int16_t)(-cos_lookup(cur_angle) * 21 / TRIG_MAX_RATIO) + pt.y,
        };
        if(!emptypart)
        {
            GPoint inner_point = 
            {
                .x = (int16_t)(sin_lookup(cur_angle) * 20 / TRIG_MAX_RATIO) + pt.x,
                .y = (int16_t)(-cos_lookup(cur_angle) * 20 / TRIG_MAX_RATIO) + pt.y,
            };
            if(inner_point.x != last_point.x || inner_point.y != last_point.y){
                graphics_draw_pixel(ctx, inner_point);
            }
        }
        
        if(new_point.x != last_point.x || new_point.y != last_point.y){
            graphics_draw_pixel(ctx, new_point);
        }
        last_point = new_point;
    }
}

static void bg_update_proc(Layer *layer, GContext *ctx) 
{
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, s_back_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    graphics_fill_circle(ctx, center, 72);

    draw_minute_ticks(layer, ctx);
    draw_military_ticks(layer, ctx);
    GPoint day_pt = {.x = 110, .y = 83};
    graphics_context_set_stroke_width(ctx, 1);

    BatteryChargeState bcs = battery_state_service_peek();    
    draw_partial_circle(layer, ctx,100 - bcs.charge_percent, day_pt);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, s_fore_dark);

    GPoint weath_pt = {.x = 33, .y = 83};
    draw_partial_circle(layer, ctx, s_clouds_pct, weath_pt);
    //graphics_draw_circle(ctx, weath_pt, 21);
    graphics_context_set_text_color(ctx, s_fore_color);
    graphics_draw_text(ctx, "N", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(69, 97, 10, 20), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, "S", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(69, 128, 10, 20), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, "W", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(51, 112, 12, 20), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, "E", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(82, 112, 10, 20), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    draw_compass_ticks(layer, ctx);
}

static void hands_update_proc(Layer *layer, GContext *ctx) 
{
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    #ifdef PBL_PLATFORM_BASALT
    int16_t second_hand_length = bounds.size.w / 2 - 5;
    #endif
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    
    graphics_context_set_fill_color(ctx, s_fore_color);
    graphics_context_set_stroke_color(ctx, s_back_color);
    
    // minute hand
    int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
    GPoint minute_end = 
    {
        .x = (int16_t)(sin_lookup(minute_angle) * 50 / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(minute_angle) * 50 / TRIG_MAX_RATIO) + center.y,
    };
    
    #ifdef PBL_PLATFORM_BASALT
    graphics_context_set_stroke_width(ctx,  7 );
    #else
    graphics_context_set_stroke_width(ctx,  5 );
    #endif
    graphics_context_set_stroke_color(ctx, s_fore_dark);
    graphics_draw_line2(ctx, center, minute_end);
    #ifdef PBL_PLATFORM_BASALT
    graphics_context_set_stroke_width(ctx,  3 );
    graphics_context_set_stroke_color(ctx, s_fore_color);
    graphics_draw_line2(ctx, center, minute_end);
    #endif
    
    // Hour Hand
    int32_t hour_angle =  (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / 72;
    GPoint hour_end = 
    {
        .x = (int16_t)(sin_lookup(hour_angle) * 30 / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(hour_angle) * 30 / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_width(ctx,  7 );
    graphics_context_set_stroke_color(ctx, s_fore_dark);
    graphics_draw_line2(ctx, center, hour_end);
    #ifdef PBL_PLATFORM_BASALT
    graphics_context_set_stroke_width(ctx,  3 );
    graphics_context_set_stroke_color(ctx, s_fore_color);
    graphics_draw_line2(ctx, center, hour_end);
    #endif
    
    #ifdef PBL_PLATFORM_BASALT  //no second hand on aplite  too busy
    // dot in the middle
    
    graphics_context_set_fill_color(ctx, s_fore_special);
    graphics_fill_circle(ctx, center, 3);
    int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
    GPoint second_hand = 
    {
        .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
    };

    // second hand
    if(s_show_seconds == 1){
        graphics_context_set_stroke_color(ctx, s_fore_special);
        graphics_context_set_stroke_width(ctx,  1 );
        graphics_draw_line2(ctx, second_hand, center);
    }
    #else
        graphics_context_set_fill_color(ctx, s_back_color);
        graphics_fill_circle(ctx, center, 3);
        graphics_draw_circle(ctx, center, 3);
    #endif
    
    // military hand
    int32_t mil_coef = ((t->tm_hour * 6) + (t->tm_min / 10));
    //mil_coef = mil_test;
    
    int32_t mil_lookup = mil_coef;
    if(mil_lookup > 18 && mil_lookup < 37){
        mil_lookup = 36 - mil_lookup;
    }
    else if(mil_lookup > 36 && mil_lookup < 55 ){
        mil_lookup = mil_lookup - 36;
    }
    else if(mil_lookup > 54 && mil_lookup < 73){
        mil_lookup = 72 - mil_lookup;
    }
    else if(mil_lookup > 72 && mil_lookup < 91){
        mil_lookup = mil_lookup - 72;
    }
    else if(mil_lookup > 90 && mil_lookup < 109){
        mil_lookup = 108 - mil_lookup;
    }
    else if(mil_lookup > 108 && mil_lookup < 127){
        mil_lookup = mil_lookup - 108;
    }
    else if(mil_lookup > 126){
        mil_lookup = 144 - mil_lookup;
    }
    
    
    
    int16_t xLen = 0;
    int16_t yLen = 0;
    
    switch(mil_lookup){
        case 0: xLen = 72; yLen = 84; break;
        case 1: xLen = 72; yLen = 84; break;
        case 2: xLen = 72; yLen = 84; break;
        case 3: xLen = 73; yLen = 85; break;
        case 4: xLen = 73; yLen = 85; break;
        case 5: xLen = 74; yLen = 86; break;
        case 6: xLen = 75; yLen = 87; break;
        case 7: xLen = 75; yLen = 88; break;
        case 8: xLen = 77; yLen = 89; break;
        case 9: xLen = 78; yLen = 91; break;
        case 10: xLen = 79; yLen = 93; break;
        case 11: xLen = 81; yLen = 95; break;
        case 12: xLen = 83; yLen = 97; break;
        case 13: xLen = 85; yLen = 100; break;
        case 14: xLen = 88; yLen = 103; break;
        case 15: xLen = 91; yLen = 106; break;
        case 16: xLen = 94; yLen = 110; break;
        case 17: xLen = 98; yLen = 114; break;
        case 18: xLen = 102; yLen = 119; break;
    }
    
    
    int32_t military_angle =  (TRIG_MAX_ANGLE * mil_coef) / 144;

                      
    int16_t milX = (int16_t)(sin_lookup(military_angle) * (xLen - 5) / TRIG_MAX_RATIO) + center.x;
    int16_t milY = (int16_t)(-cos_lookup(military_angle) * (yLen - 5) / TRIG_MAX_RATIO) + center.y;
     
    
                      
    GPoint military_start = 
     {
        .x = milX,
        .y = milY,
    };
    
    milX = (int16_t)(sin_lookup(military_angle) * (xLen) / TRIG_MAX_RATIO) + center.x;
    milY = (int16_t)(-cos_lookup(military_angle) * (yLen) / TRIG_MAX_RATIO) + center.y;
   
    GPoint military_end = 
    {
        .x = milX,
        .y = milY,
    };
    graphics_context_set_stroke_width(ctx,  5 );
    graphics_draw_line2(ctx, military_start, military_end);
   
}

static void date_update_proc(Layer *layer, GContext *ctx) 
{
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", t);
  text_layer_set_text_color(s_day_label, s_fore_color);
  text_layer_set_text_color(s_num_label, s_fore_color);
  text_layer_set_text_color(s_twelve_label, s_fore_color);
  text_layer_set_text_color(s_temperature_label, s_fore_color);
  text_layer_set_text_color(s_pebble_label, s_fore_color);
  if(s_cur_temp_type != 0){
      text_layer_set_text(s_temperature_label, s_temp_c_buffer);
  }
  else{
      text_layer_set_text(s_temperature_label, s_temp_buffer);
  }
  
    
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
    
  if(bluetooth_connection_service_peek()){
       graphics_context_set_stroke_color(ctx, s_fore_special);
       graphics_context_set_stroke_width(ctx,  1 );
      GPoint bt_start =
        {
        .x = 58,
        .y = 53,
        };
       GPoint bt_end =
        {
        .x = 85,
        .y = 53,
        };

       graphics_draw_line2(ctx, bt_start, bt_end);
  }
    
}

static void request_weather(void) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "error creating");
    return;
  }

  int value = 1;
  dict_write_int(iter, 1, &value, sizeof(int), true);
  dict_write_end(iter);

  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "sent");
  s_needs_weather = false;
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) 
{
  if(tick_time->tm_sec % 5 == 0){
      layer_mark_dirty(s_hands_layer);
  }

  if(s_needs_weather || ((tick_time->tm_min % 15) == 0 && tick_time->tm_sec == 0)){
      APP_LOG(APP_LOG_LEVEL_INFO, "requesting weather %d %d", (int)(tick_time->tm_min % 15), (int) tick_time->tm_min );
      request_weather();
  }
}

static void handle_tap(AccelAxisType axis, int32_t direction){
    APP_LOG(APP_LOG_LEVEL_INFO, "tap %d %d", (int)axis, (int) direction);
    s_cur_color_scheme++;
    if(s_cur_color_scheme > 5){
        s_cur_color_scheme = 0;
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "before set %d %d", (int)s_cur_color_scheme, (int) s_cur_color_type);
    set_color_scheme();
    APP_LOG(APP_LOG_LEVEL_INFO, "after set %d %d", (int)s_cur_color_scheme, (int) s_cur_color_type);
    layer_mark_dirty(s_hands_layer);
    persist_write_int(COLOR_SCHEME_KEY , s_cur_color_scheme);
    persist_write_int(COLOR_TYPE_KEY , s_cur_color_type);
    persist_write_int(TEMP_TYPE_KEY , s_cur_temp_type);
}

char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %s", translate_error(app_message_error));
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "got %d", (int)key);
  if(key == PHONE_TEMPERATURE_KEY){
      // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
      strcpy( s_temp_buffer, new_tuple->value->cstring);      
  }
  else if(key == PHONE_TEMPERATURE_C_KEY){
      // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
      strcpy( s_temp_c_buffer, new_tuple->value->cstring);      
  }
  else if(key == PHONE_CLOUDS_KEY){
      s_clouds_pct = new_tuple->value->int32;
  }
  else if(key == PHONE_COLOR_KEY){
      s_cur_color_type = new_tuple->value->int32;
  }
  else if(key == PHONE_TEMP_TYPE_KEY){
      s_cur_temp_type = new_tuple->value->int32;
  }
  else if(key == PHONE_SECONDS_KEY){
      s_show_seconds = new_tuple->value->int32;
      APP_LOG(APP_LOG_LEVEL_INFO, "seconds %d", (int)s_show_seconds);
  }
    set_color_scheme();
    layer_mark_dirty(s_hands_layer);
    persist_write_int(COLOR_SCHEME_KEY , s_cur_color_scheme);
    persist_write_int(COLOR_TYPE_KEY , s_cur_color_type);
    persist_write_int(TEMP_TYPE_KEY , s_cur_temp_type);
    persist_write_int(SECONDS_KEY, s_show_seconds);
}




static void window_load(Window *window) 
{
    s_clouds_pct = 0;
    s_needs_weather = true;
    
  APP_LOG(APP_LOG_LEVEL_INFO, "before load %d", (int)s_cur_color_scheme);
  if(persist_exists(COLOR_SCHEME_KEY)){
    s_cur_color_scheme = persist_read_int(COLOR_SCHEME_KEY);
  }
  else
  {
      s_cur_color_scheme = 0;
  }
  if(persist_exists(COLOR_TYPE_KEY)){
    s_cur_color_type = persist_read_int(COLOR_TYPE_KEY);
  }
  else
  {
      s_cur_color_scheme = 0;
  }
  if(persist_exists(TEMP_TYPE_KEY)){
    s_cur_temp_type = persist_read_int(TEMP_TYPE_KEY);
  }
  else
  {
      s_cur_color_scheme = 0;
  }
    
  if(persist_exists(SECONDS_KEY)){
      s_show_seconds = persist_read_int(SECONDS_KEY);
  }
  else{
      s_show_seconds = 1;      
  }
  
  
  APP_LOG(APP_LOG_LEVEL_INFO, "after load %d", (int)s_cur_color_scheme);
  set_color_scheme();
    
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
 
  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_simple_bg_layer);

  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);
    
  s_pebble_label = text_layer_create(GRect(51, 37, 40, 20));
  text_layer_set_text(s_pebble_label, "pebble");
  text_layer_set_background_color(s_pebble_label, GColorClear);
  text_layer_set_text_color(s_pebble_label, s_fore_color);
  text_layer_set_text_alignment(s_pebble_label, GTextAlignmentCenter);
  text_layer_set_font(s_pebble_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  layer_add_child(s_date_layer, text_layer_get_layer(s_pebble_label));
    
  s_day_label = text_layer_create(GRect(96, 77, 28, 20));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorClear);
  text_layer_set_text_color(s_day_label, s_fore_color);
  text_layer_set_text_alignment(s_day_label, GTextAlignmentCenter);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(GRect(101, 63, 18, 20));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorClear);
  text_layer_set_text_color(s_num_label, s_fore_color);
  text_layer_set_text_alignment(s_num_label, GTextAlignmentCenter);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    
  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));
    
  s_twelve_label = text_layer_create(GRect(60, 2, 22, 30));
  text_layer_set_text(s_twelve_label, "12");
  text_layer_set_background_color(s_twelve_label, GColorClear);
  text_layer_set_text_color(s_twelve_label, s_fore_color);
  text_layer_set_font(s_twelve_label, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    

  layer_add_child(s_date_layer, text_layer_get_layer(s_twelve_label));
    
  s_temperature_label = text_layer_create(GRect(14, 67, 40, 25));
  text_layer_set_background_color(s_temperature_label, GColorClear);
  text_layer_set_text(s_temperature_label, s_temp_buffer);
  text_layer_set_text_color(s_temperature_label, s_fore_color);
  text_layer_set_text_alignment(s_temperature_label, GTextAlignmentCenter);
  text_layer_set_font(s_temperature_label, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    
  layer_add_child(s_date_layer, text_layer_get_layer(s_temperature_label));

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);

  Tuplet initial_values[] = {
    TupletCString(PHONE_TEMPERATURE_KEY, "---"),
    TupletCString(PHONE_TEMPERATURE_C_KEY, "---"),
    TupletInteger(PHONE_CLOUDS_KEY, (int32_t)0),
    TupletInteger(PHONE_COLOR_KEY, (int32_t)s_cur_color_type),
    TupletInteger(PHONE_TEMP_TYPE_KEY, (int32_t)s_cur_temp_type),
    TupletInteger(PHONE_SECONDS_KEY,(int32_t)s_show_seconds)
  };
    
    app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);
}

static void window_unload(Window *window) 
{
  layer_destroy(s_simple_bg_layer);
  layer_destroy(s_date_layer);

  text_layer_destroy(s_day_label);
  text_layer_destroy(s_num_label);

  layer_destroy(s_hands_layer);
}

static void init() 
{
  APP_LOG(APP_LOG_LEVEL_INFO, "init");
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
   });
  window_stack_push(window, true);

  s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';
  s_temp_buffer[0] = '\0';
  s_temp_c_buffer[0] = '\0'; 
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
  accel_tap_service_subscribe(handle_tap);

  app_message_open(64, 64);
  
}

static void deinit() 
{
  app_sync_deinit(&s_sync);
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  window_destroy(window);
}

int main() 
{
  APP_LOG(APP_LOG_LEVEL_INFO, "main");
  init();
  app_event_loop();
  deinit();
}