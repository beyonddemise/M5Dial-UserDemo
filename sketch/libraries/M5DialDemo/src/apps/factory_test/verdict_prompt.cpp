/**
 * @file verdict_prompt.cpp
 * @brief promptVerdict(): draw the PASS/FAIL/SKIP screen, read knob and button.
 */
#include "verdict_prompt.h"

#include <M5Dial.h>

#include "../../config.h"
#include "../../dial_encoder.h"
#include "../../encoder_detent.h"
#include "../../m5dd_begin.h"
#include "verdict_choice.h"

namespace factory_test {

namespace {

constexpr int CENTER = 120;      ///< centre of the 240x240 round panel
constexpr int TITLE_Y = 50;      ///< "<step>?" line
constexpr int FIRST_Y = 100;     ///< centre of the PASS row
constexpr int ROW_PITCH = 42;    ///< row centre to row centre
constexpr int ROW_W = 140;       ///< highlight box size
constexpr int ROW_H = 36;
constexpr int ROW_R = 10;        ///< highlight corner radius
constexpr uint32_t POLL_MS = 5;  ///< loop pause; the knob counts in an ISR, nothing is lost

/** @return the colour of an option (PASS green, FAIL red, SKIP orange). */
uint16_t optionColor(Verdict v) {
    switch (v) {
        case Verdict::Pass: return TFT_GREEN;
        case Verdict::Fail: return TFT_RED;
        default: return TFT_ORANGE;
    }
}

/** Draws the whole prompt with option `selected` highlighted and pushes it. */
void draw(M5Canvas& canvas, const char* stepName, int selected) {
    canvas.fillScreen(TFT_BLACK);
    canvas.drawCircle(CENTER, CENTER, CENTER - 1, TFT_DARKGRAY);
    canvas.setTextColor(TFT_YELLOW);
    canvas.drawString((std::string(stepName) + "?").c_str(), CENTER, TITLE_Y);
    for (int i = 0; i < verdict_choice::COUNT; ++i) {
        Verdict v = verdict_choice::at(i);
        int y = FIRST_Y + i * ROW_PITCH;
        uint16_t color = optionColor(v);
        if (i == selected) {
            canvas.fillSmoothRoundRect(CENTER - ROW_W / 2, y - ROW_H / 2, ROW_W, ROW_H, ROW_R, color);
            canvas.setTextColor(TFT_BLACK);
        } else {
            canvas.drawRoundRect(CENTER - ROW_W / 2, y - ROW_H / 2, ROW_W, ROW_H, ROW_R, TFT_DARKGRAY);
            canvas.setTextColor(color);
        }
        canvas.drawString(verdictName(v), CENTER, y);
    }
    canvas.pushSprite(0, 0);
}

}  // namespace

Verdict promptVerdict(M5Canvas& canvas, const char* stepName, Verdict defaultVerdict) {
    // why save/restore: the step apps draw with the canvas state the original HAL left
    // (efontCN_24, size 1, top-left datum) and printf at fixed cursors
    const lgfx::IFont* oldFont = canvas.getFont();
    float oldSize = canvas.getTextSizeX();
    textdatum_t oldDatum = canvas.getTextDatum();
    canvas.setFont(GUI_FONT_CN_BIG);
    canvas.setTextSize(1);
    canvas.setTextDatum(middle_center);

    int selected = verdict_choice::indexOf(defaultVerdict);
    EncoderDetent detent;
    // why reset: detents turned during the step belong to the step, not to the prompt
    detent.reset(dialEncoder.read());
    draw(canvas, stepName, selected);
    M5_LOGI("factory: prompt %s", stepName);

    while (true) {
        // why M5Dial.update(): BtnA only changes state inside update() (Review Focus 1)
        M5Dial.update();
        int32_t steps = m5dd::readDetentsWithClick(detent);
        if (steps != 0) {
            int next = verdict_choice::move(selected, steps);
            if (next != selected) {
                selected = next;
                draw(canvas, stepName, selected);
            }
        }
        // why buttonClicked (wasPressed edge + release wait): a button still held from the
        // click that ended the step cannot confirm at once, and the next step starts released
        if (m5dd::buttonClicked()) break;
        delay(POLL_MS);
    }

    Verdict v = verdict_choice::at(selected);
    M5_LOGI("factory: verdict %s=%s", stepName, verdictName(v));

    canvas.setFont(oldFont);
    canvas.setTextSize(oldSize);
    canvas.setTextDatum(oldDatum);
    return v;
}

}  // namespace factory_test
