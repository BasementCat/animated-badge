#ifndef _MAIN_TOUCH_IMPL_H_
#define _MAIN_TOUCH_IMPL_H_

#define MAIN_BTN_LOCK 1
#define MAIN_BTN_MENU 2
#define MAIN_BTN_LEFT 3
#define MAIN_BTN_PAUSE 4
#define MAIN_BTN_RIGHT 5


uint8_t get_main_screen_touch(TouchScreen* touchscreen) {
    static uint8_t button = 0, last_button = 0;
    static long button_pressed_at = 0, button_released_at = 0;

    uint8_t rval = 0;
    TSPoint p = touchscreen->getPoint();

    if (p.z > touchscreen->pressureThreshhold) {
        int16_t x = map(p.x, X_MIN, X_MAX, 0, 240);
        int16_t y = map(p.y, Y_MIN, Y_MAX, 0, 320);
        uint8_t new_button;
        if (y < 64) {
            new_button = MAIN_BTN_LOCK;
        } else if (y > 256) {
            new_button = MAIN_BTN_MENU;
        } else if (x < 80) {
            new_button = MAIN_BTN_LEFT;
        } else if (x < 160) {
            new_button = MAIN_BTN_PAUSE;
        } else {
            new_button = MAIN_BTN_RIGHT;
        }
        if (!button) {
            button = new_button;
            button_pressed_at = millis();
        }
        last_button = new_button;
        button_released_at = 0;
    } else {
        if (button && button_released_at && millis() - button_released_at > 50 && button == last_button) {
            if (button == MAIN_BTN_LOCK) {
                if (millis() - button_pressed_at >= 3000) {
                    rval = MAIN_BTN_LOCK;
                }
            } else {
                rval = button;
            }

            button = 0;
            last_button = 0;
            button_pressed_at = 0;
            button_released_at = 0;
        } else if (!button_released_at) {
            button_released_at = millis();
        }
    }
    return rval;
}

#endif