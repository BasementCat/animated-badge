#ifndef _STATUS_LED_IMPL_H_
#define _STATUS_LED_IMPL_H_

#include "Adafruit_NeoPixel.h"

#define STATUS_LED_GOOD 0
#define STATUS_LED_OK 1
#define STATUS_LED_POOR 2
#define STATUS_LED_BAD 3

#define STATUS_LED_DEFAULT_BRI 32

#define STATUS_LED_C_WHITE 0x00ffffff
#define STATUS_LED_C_GREEN 0x0000ff00
#define STATUS_LED_C_BLUE 0x000000ff
#define STATUS_LED_C_YELLOW 0x00ffff00
#define STATUS_LED_C_RED 0x00ff0000
#define STATUS_LED_C_PINK 0x00f00ff


Adafruit_NeoPixel status_led(1, NEOPIXEL, NEO_GRB+NEO_KHZ800);


void set_status_led_color(uint32_t color) {
    status_led.setPixelColor(0, color);
    status_led.show();
}

void set_status_led(uint8_t status) {
    static long last_status = 0;
    if (!last_status || millis() - last_status >= 500) {
        last_status = millis();
        switch (status) {
            case STATUS_LED_GOOD:
                set_status_led_color(STATUS_LED_C_GREEN);
                break;
            case STATUS_LED_OK:
                set_status_led_color(STATUS_LED_C_BLUE);
                break;
            case STATUS_LED_POOR:
                set_status_led_color(STATUS_LED_C_YELLOW);
                break;
            case STATUS_LED_BAD:
                set_status_led_color(STATUS_LED_C_RED);
                break;
            default:
                set_status_led_color(STATUS_LED_C_PINK);
        }
    }
}

void status_led_init() {
    status_led.begin();
    status_led.clear();
    status_led.setBrightness(STATUS_LED_DEFAULT_BRI);
    set_status_led(99);
}

#endif