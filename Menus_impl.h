#ifndef _MENUS_IMPL_H_
#define _MENUS_IMPL_H_

#include "Adafruit_ILI9341.h"
#include "TouchScreen.h"

#include "colors.h"
#include "prefs.h"
#include "backlight_impl.h"


#define BUTTON_H_MARGIN 6
#define BUTTON_PAD 8
#define CONTROL_V_MARGIN 10


uint16_t _write_centered_text(Adafruit_ILI9341* tft, int16_t top, const char* text) {
    int16_t  x1, y1;
    uint16_t w, h;
    tft->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft->setCursor((SCREEN_WIDTH - w) / 2, top);
    tft->print(text);
    return top + h;
}

uint16_t _render_menu_base(Adafruit_ILI9341* tft, const char* heading) {
    tft->fillScreen(COLOR_BLACK);
    tft->drawRoundRect(0, 0, 240, 320, 4, COLOR_PURPLE);
    tft->setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft->setTextSize(2);
    uint16_t text_bottom = _write_centered_text(tft, 3, heading) + 2;
    tft->drawLine(0, text_bottom, SCREEN_WIDTH, text_bottom, COLOR_PURPLE);
    return text_bottom;
}


class Button {
protected:
    Adafruit_ILI9341* tft;
    TouchScreen* ts;
    const char* text;
    uint16_t x1, y1, w, h, text_x, text_y, press_color;
    bool is_pressed = false;
    long released_at = 0;

    bool contains(int16_t x, int16_t y) {
        return
            x >= this->x1
            && x <= (this->x1 + this->w)
            && y >= this->y1
            && y <= (this->y1 + this->h);
    }

    virtual uint16_t get_bgcolor() {
        return this->is_pressed ? COLOR_PURPLE : COLOR_BLACK;
    }

public:
    Button(Adafruit_ILI9341* tft, TouchScreen* ts, const char* text, uint16_t top, float left, float width) {
        int16_t  x1, y1;
        uint16_t w, h;

        this->tft = tft;
        this->ts = ts;
        this->text = text;
        this->x1 = ((SCREEN_WIDTH - (BUTTON_H_MARGIN * 2)) * left) + BUTTON_H_MARGIN;
        this->y1 = top;
        this->w = (SCREEN_WIDTH - (BUTTON_H_MARGIN * 2)) * width;
        this->tft->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        this->text_x = this->x1 + BUTTON_PAD;
        this->text_y = this->y1 + BUTTON_PAD;
        this->h = h + (BUTTON_PAD * 2);
    }

    uint16_t render() {
        this->tft->fillRect(this->x1, this->y1, this->w, this->h, COLOR_BLACK);
        if (this->is_pressed) {
            this->tft->fillRoundRect(this->x1, this->y1, this->w, this->h, 4, this->get_bgcolor());
            this->tft->drawRoundRect(this->x1, this->y1, this->w, this->h, 4, COLOR_PURPLE);
            this->tft->setTextColor(COLOR_WHITE, this->get_bgcolor());
        } else {
            this->tft->fillRoundRect(this->x1, this->y1, this->w, this->h, 4, this->get_bgcolor());
            this->tft->drawRoundRect(this->x1, this->y1, this->w, this->h, 4, COLOR_PURPLE);
            this->tft->setTextColor(COLOR_WHITE, this->get_bgcolor());
        }
        this->tft->setCursor(this->text_x, this->text_y);
        this->tft->print(this->text);
        return this->y1 + this->h;
    }

    bool check() {
        TSPoint p = this->ts->getPoint();
        int16_t x = map(p.x, X_MIN, X_MAX, 0, SCREEN_WIDTH);
        int16_t y = map(p.y, Y_MIN, Y_MAX, 0, SCREEN_HEIGHT);
        if (p.z > this->ts->pressureThreshhold) {
            if (this->contains(x, y)) {
                this->released_at = 0;
                if (!this->is_pressed) {
                    this->is_pressed = true;
                    this->render();
                }
            } else {
                this->released_at = 0;
                if (this->is_pressed) {
                    this->is_pressed = false;
                    this->render();
                }
            }
        } else {
            if (!this->released_at) this->released_at = millis();
            if (millis() - this->released_at >= 50 && this->is_pressed) {
                this->is_pressed = false;
                this->released_at = 0;
                return true;
            }
        }
        return false;
    }
};


class Toggle : Button {
protected:
    bool state = false;

    virtual uint16_t get_bgcolor() {
        if (this->is_pressed) return COLOR_DARKGREY;
        if (this->state) return COLOR_PURPLE;
        return COLOR_BLACK;
    }

public:
    using Button::Button;
    using Button::render;

    bool check() {
        bool res = Button::check();
        if (res) {
            this->state = !this->state;
            this->render();
        }
        return res;
    }

    bool get_state() {
        return this->state;
    }

    void set_state(bool val) {
        this->state = val;
        this->render();
    }
};


class Label {
protected:
    Adafruit_ILI9341* tft;
    const char* text;
    uint8_t size;
    uint16_t x1, y1, w, h, text_x, text_y;
public:
    Label(Adafruit_ILI9341* tft, const char* text, uint8_t size, uint16_t top, float left, float width) {
        int16_t  x1, y1;
        uint16_t w, h;

        this->tft = tft;
        this->text = text;
        this->size = size;
        this->x1 = ((SCREEN_WIDTH - (BUTTON_H_MARGIN * 2)) * left) + BUTTON_H_MARGIN;
        this->y1 = top;
        this->w = (SCREEN_WIDTH - (BUTTON_H_MARGIN * 2)) * width;
        this->tft->setTextSize(this->size);
        this->tft->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        this->tft->setTextSize(2);
        this->text_x = this->x1 + BUTTON_PAD;
        this->text_y = this->y1;
        this->h = h + BUTTON_PAD;
    }

    uint16_t render() {
        this->tft->fillRect(this->x1, this->y1, this->w, this->h, COLOR_BLACK);
        this->tft->setTextColor(COLOR_WHITE, COLOR_BLACK);
        this->tft->setCursor(this->text_x, this->text_y);
        this->tft->setTextSize(this->size);
        this->tft->print(this->text);
        this->tft->setTextSize(2);
        return this->y1 + this->h;
    }

    void set_text(const char* text) {
        // assumes text is the same height, and not too wide
        this->text = text;
        this->render();
    }
};

uint8_t _clamp_bri(uint8_t val, int8_t inc) {
    if (val + inc < 10) return 10;
    if (val + inc > 100) return 100;
    return val + inc;
}

void backlight_menu(Prefs* prefs, Adafruit_ILI9341* tft, TouchScreen* ts) {
    backlight_menu_top:
    uint16_t top = _render_menu_base(tft, "Backlight");

    uint8_t brightness = ((float)prefs->brightness / 255) * 100,
            bri_auto_min = ((float)prefs->bri_auto_min / 255) * 100,
            bri_auto_max = ((float)prefs->bri_auto_max / 255) * 100;
    bool bri_auto = read_pref_flag(prefs, PREFS_FLAG_BL_AUTO);
    char brightness_s[4],
         bri_auto_min_s[4],
         bri_auto_max_s[4];
    itoa(brightness, brightness_s, 10);
    itoa(bri_auto_min, bri_auto_min_s, 10);
    itoa(bri_auto_max, bri_auto_max_s, 10);

    Label bri_lbl(tft, "Brightness", 1, top + CONTROL_V_MARGIN, 0, 1);
    top = bri_lbl.render();

    Button bri_down(tft, ts, "-10", top + CONTROL_V_MARGIN, 0, .3);
    bri_down.render();

    Label bri_val(tft, brightness_s, 2, top + CONTROL_V_MARGIN, .33, .3);
    bri_val.render();

    Button bri_up(tft, ts, "+10", top + CONTROL_V_MARGIN, .66, .3);
    top = bri_up.render();

    Toggle auto_bri(tft, ts, "Auto Brightness", top + CONTROL_V_MARGIN, 0, 1);
    top = auto_bri.render();
    auto_bri.set_state(bri_auto);

    Label bri_auto_min_lbl(tft, "Auto Min", 1, top + CONTROL_V_MARGIN, 0, 1);
    top = bri_auto_min_lbl.render();

    Button bri_auto_min_down(tft, ts, "-10", top + CONTROL_V_MARGIN, 0, .3);
    bri_auto_min_down.render();

    Label bri_auto_min_val(tft, bri_auto_min_s, 2, top + CONTROL_V_MARGIN, .33, .3);
    bri_auto_min_val.render();

    Button bri_auto_min_up(tft, ts, "+10", top + CONTROL_V_MARGIN, .66, .3);
    top = bri_auto_min_up.render();

    Label bri_auto_max_lbl(tft, "Auto Max", 1, top + CONTROL_V_MARGIN, 0, 1);
    top = bri_auto_max_lbl.render();

    Button bri_auto_max_down(tft, ts, "-10", top + CONTROL_V_MARGIN, 0, .3);
    bri_auto_max_down.render();

    Label bri_auto_max_val(tft, bri_auto_max_s, 2, top + CONTROL_V_MARGIN, .33, .3);
    bri_auto_max_val.render();

    Button bri_auto_max_up(tft, ts, "+10", top + CONTROL_V_MARGIN, .66, .3);
    top = bri_auto_max_up.render();

    Button back(tft, ts, "< Back", top + CONTROL_V_MARGIN, 0, 1);
    top = back.render();

    while (true) {
        if (bri_down.check()) {
            brightness = _clamp_bri(brightness, -10);
            itoa(brightness, brightness_s, 10);
            bri_val.set_text(brightness_s);
            bri_down.render();
        }
        if (bri_up.check()) {
            brightness = _clamp_bri(brightness, 10);
            itoa(brightness, brightness_s, 10);
            bri_val.set_text(brightness_s);
            bri_up.render();
        }
        if (auto_bri.check()) {
            bri_auto = auto_bri.get_state();
        }
        if (bri_auto_min_down.check()) {
            bri_auto_min = _clamp_bri(bri_auto_min, -10);
            itoa(bri_auto_min, bri_auto_min_s, 10);
            bri_auto_min_val.set_text(bri_auto_min_s);
            bri_auto_min_down.render();
        }
        if (bri_auto_min_up.check()) {
            bri_auto_min = _clamp_bri(bri_auto_min, 10);
            itoa(bri_auto_min, bri_auto_min_s, 10);
            bri_auto_min_val.set_text(bri_auto_min_s);
            bri_auto_min_up.render();
        }
        if (bri_auto_max_down.check()) {
            bri_auto_max = _clamp_bri(bri_auto_max, -10);
            itoa(bri_auto_max, bri_auto_max_s, 10);
            bri_auto_max_val.set_text(bri_auto_max_s);
            bri_auto_max_down.render();
        }
        if (bri_auto_max_up.check()) {
            bri_auto_max = _clamp_bri(bri_auto_max, 10);
            itoa(bri_auto_max, bri_auto_max_s, 10);
            bri_auto_max_val.set_text(bri_auto_max_s);
            bri_auto_max_up.render();
        }
        if (back.check()) {
            write_prefs(prefs);
            return;
        }

        prefs->brightness = ((float)brightness / 100) * 255;
        prefs->bri_auto_min = ((float)bri_auto_min / 100) * 255;
        prefs->bri_auto_max = ((float)bri_auto_max / 100) * 255;
        set_pref_flag(prefs, PREFS_FLAG_BL_AUTO, bri_auto);
        update_backlight(prefs, true);
    }
}

void display_menu(Prefs* prefs, Adafruit_ILI9341* tft, TouchScreen* ts) {
    display_menu_top:
    uint16_t top = _render_menu_base(tft, "Display");

    char display_time_s[4];
    itoa(prefs->display_time_s, display_time_s, 10);

    Label disp_time_lbl(tft, "Image Display Time - Seconds", 1, top + CONTROL_V_MARGIN, 0, 1);
    top = disp_time_lbl.render();

    Button disp_time_down(tft, ts, "-10", top + CONTROL_V_MARGIN, 0, .3);
    disp_time_down.render();

    Label disp_time_val(tft, display_time_s, 2, top + CONTROL_V_MARGIN, .33, .3);
    disp_time_val.render();

    Button disp_time_up(tft, ts, "+10", top + CONTROL_V_MARGIN, .66, .3);
    top = disp_time_up.render();

    Button back(tft, ts, "< Back", top + CONTROL_V_MARGIN, 0, 1);
    top = back.render();

    while (true) {
        if (disp_time_down.check()) {
            if (prefs->display_time_s > 1)
                prefs->display_time_s -= 1;
            itoa(prefs->display_time_s, display_time_s, 10);
            disp_time_val.set_text(display_time_s);
            disp_time_down.render();
        }
        if (disp_time_up.check()) {
            if (prefs->display_time_s < 999)
                prefs->display_time_s += 1;
            itoa(prefs->display_time_s, display_time_s, 10);
            disp_time_val.set_text(display_time_s);
            disp_time_up.render();
        }
        if (back.check()) {
            write_prefs(prefs);
            return;
        }

        update_backlight(prefs);
    }
}

void main_menu(Prefs* prefs, Adafruit_ILI9341* tft, TouchScreen* ts) {
    main_menu_top:
    uint16_t top = _render_menu_base(tft, "Menu");
    Button backlight(tft, ts, "Backlight", top + CONTROL_V_MARGIN, 0, 1);
    top = backlight.render();
    Button display(tft, ts, "Display", top + CONTROL_V_MARGIN, 0, 1);
    top = display.render();
    Button back(tft, ts, "< Back", top + CONTROL_V_MARGIN, 0, 1);
    top = back.render();

    while (true) {
        if (backlight.check()) {
            backlight_menu(prefs, tft, ts);
            goto main_menu_top;
        }
        if (display.check()) {
            display_menu(prefs, tft, ts);
            goto main_menu_top;
        }
        if (back.check()) {
            return;
        }
        update_backlight(prefs);
    }
}

#endif