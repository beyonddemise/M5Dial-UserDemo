/**
 * @file LvglDemo.ino
 * @brief LVGL 9 starter sketch for the M5Dial: an arc (0..100) driven by the knob, a centred
 *        value label and a button that toggles the arc colour. Not part of UserDemo and not
 *        built on the app framework; a template for LVGL projects on this board.
 *
 * Wiring to LVGL:
 *   - display: 240x240, partial rendering into one 240x24 px RGB565 buffer, flushed with
 *     M5Dial.Display.pushImage()
 *   - encoder indev: detent steps from dialEncoder (EncoderDetent, clockwise = +1), press = BtnA
 *   - pointer indev: M5Dial.Touch.getDetail()
 *   - tick: lv_tick_set_cb(millis)
 * The knob starts in edit mode on the arc (turn = value). Press the knob to leave edit mode,
 * turn to focus the button, press to toggle the colour; or just tap the button.
 *
 * Log lines (tag: message), checked by sim/scenarios/LvglDemo/knob.test.yaml:
 *   boot: LvglDemo | init: rfid ok|missing   (both from m5dd::begin)
 *   lvgl: ready
 *   lvgl: value=<n>                           (on every change of the arc value)
 *   lvgl: colour=blue|orange                  (on every button click)
 *
 * Build: tools/build.sh examples/LvglDemo (adds the lv_conf.h flags, see lv_conf.h).
 */
#include <M5DialDemo.h>
#include <lvgl.h>

// why: explicit prototypes, as in DialProbe - arduino-cli's prototype generator otherwise may
// insert them inside setup()'s body
void setup();
void loop();

namespace {

constexpr int32_t SCREEN_SIZE = 240;  ///< the round panel is 240x240
constexpr int32_t BUF_LINES = 24;     ///< partial buffer height (1/10 of the screen)
constexpr int32_t ARC_MIN = 0;
constexpr int32_t ARC_MAX = 100;
constexpr int32_t ARC_SIZE = 200;
constexpr int32_t ARC_WIDTH = 16;
constexpr uint32_t LOOP_DELAY_MAX_MS = 5;  ///< upper bound on the idle wait between loops

/**
 * Partial render buffer: 240 x 24 px x 2 bytes = 11.25 KB in internal RAM (.bss).
 * why aligned: lv_display_set_buffers() requires LV_DRAW_BUF_ALIGN alignment.
 */
alignas(LV_DRAW_BUF_ALIGN) uint8_t draw_buf[SCREEN_SIZE * BUF_LINES * 2];

EncoderDetent detent;  ///< M5DD_ENCODER_COUNTS_PER_DETENT / M5DD_ENCODER_CW_SIGN from config.h
lv_obj_t* arc = nullptr;
lv_obj_t* value_label = nullptr;
bool orange = false;  ///< current arc colour: false = blue, true = orange

/**
 * LVGL flush callback: copy one rendered area to the panel.
 * why lgfx::rgb565_t: LVGL renders RGB565 in native (little-endian) uint16 order. M5GFX reads
 * a plain `uint16_t*` as byte-swapped swap565 unless setSwapBytes(true); the typed pointer
 * says "native RGB565" explicitly, independent of that global flag, and M5GFX swaps to the
 * panel's big-endian order while it copies.
 */
void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    M5Dial.Display.pushImage(area->x1, area->y1, lv_area_get_width(area),
                             lv_area_get_height(area),
                             reinterpret_cast<const lgfx::rgb565_t*>(px_map));
    lv_display_flush_ready(disp);
}

/**
 * LVGL encoder read callback: detent steps since the last read, and the knob button.
 * why EncoderDetent: dialEncoder counts x4 edges in an ISR; LVGL wants whole detents
 * (one step per click, clockwise positive = LV_KEY_RIGHT = arc value up).
 */
void encoder_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    data->enc_diff = (int16_t)detent.update(dialEncoder.read());
    // why isPressed(): BtnA's state is refreshed by M5Dial.update() in loop()
    data->state = M5Dial.BtnA.isPressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/** LVGL pointer read callback: the current touch point, if any. */
void touch_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    auto t = M5Dial.Touch.getDetail();
    if (t.isPressed()) {
        data->point.x = t.x;
        data->point.y = t.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;  // why: LVGL keeps the last point on release
    }
}

/** LVGL tick source: milliseconds since boot. */
uint32_t tick_cb() { return millis(); }

/** Arc moved: update the label and log the new value. */
void arc_changed_cb(lv_event_t*) {
    int32_t v = lv_arc_get_value(arc);
    lv_label_set_text_fmt(value_label, "%d", (int)v);
    M5_LOGI("lvgl: value=%d", (int)v);
}

/** Set the arc's indicator colour from `orange`. */
void apply_arc_colour() {
    lv_obj_set_style_arc_color(arc, lv_palette_main(orange ? LV_PALETTE_ORANGE : LV_PALETTE_BLUE),
                               LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_palette_main(orange ? LV_PALETTE_ORANGE : LV_PALETTE_BLUE),
                              LV_PART_KNOB);
}

/** Button clicked (touch or knob press while focused): toggle the arc colour. */
void button_clicked_cb(lv_event_t*) {
    orange = !orange;
    apply_arc_colour();
    M5_LOGI("lvgl: colour=%s", orange ? "orange" : "blue");
}

/** Create the display, attached to the panel through flush_cb. */
void init_display() {
    lv_display_t* disp = lv_display_create(SCREEN_SIZE, SCREEN_SIZE);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/**
 * Build the UI and the input devices: arc + label in the middle, button below, both in one
 * group that the encoder drives.
 */
void init_ui() {
    lv_obj_t* scr = lv_screen_active();

    arc = lv_arc_create(scr);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_arc_set_range(arc, ARC_MIN, ARC_MAX);
    lv_arc_set_value(arc, ARC_MIN);
    lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_center(arc);
    lv_obj_add_event_cb(arc, arc_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    apply_arc_colour();

    value_label = lv_label_create(scr);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_label_set_text_fmt(value_label, "%d", (int)ARC_MIN);
    lv_obj_center(value_label);

    // why inside the arc's open bottom (the default arc leaves 90 degrees free there)
    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_add_event_cb(btn, button_clicked_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Colour");
    lv_obj_center(btn_label);

    lv_group_t* group = lv_group_create();
    lv_group_add_obj(group, arc);
    lv_group_add_obj(group, btn);
    lv_group_focus_obj(arc);
    // why: start in edit mode so the knob changes the value at once (the common case)
    lv_group_set_editing(group, true);

    lv_indev_t* enc = lv_indev_create();
    lv_indev_set_type(enc, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(enc, encoder_read_cb);
    lv_indev_set_group(enc, group);

    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, touch_read_cb);
}

}  // namespace

/**
 * Bring up the M5Dial (m5dd::begin without the 115 KB canvas: LVGL renders into draw_buf),
 * then LVGL, the UI and the input devices.
 */
void setup() {
    m5dd::begin("LvglDemo");
    detent.reset(dialEncoder.read());

    lv_init();
    // why lv_tick_set_cb (not lv_tick_inc from a timer): LVGL 9 documents it as the way to
    // give it a time source; millis() is already monotonic, so no extra timer or ISR is needed
    lv_tick_set_cb(tick_cb);
    init_display();
    init_ui();
    lv_timer_handler();  // first full frame, before the ready line
    M5_LOGI("lvgl: ready");
}

/** Poll the inputs (M5Dial.update()) and run LVGL's timers (input reads, redraws). */
void loop() {
    M5Dial.update();
    uint32_t next_ms = lv_timer_handler();
    // why bounded: lv_timer_handler() may report a long idle time, but touch and the button
    // are only sampled in M5Dial.update(), so keep polling at least every few milliseconds
    delay(next_ms < LOOP_DELAY_MAX_MS ? next_ms : LOOP_DELAY_MAX_MS);
}
