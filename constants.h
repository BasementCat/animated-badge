#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#define VERSION "2.0"

#define SERIAL_SPEED 115200

#define SD_DETECT 33 // D33
#define SD_CS 32 // D32

#define ESP_CS 8 // D8
#define ESP_BUSY 5 // D5
#define ESP_RESET 7 // D7

#define SPEAKER A0

#define TOUCH_YD A4 // or YP
#define TOUCH_YU A6 // or YM
#define TOUCH_XL A5 // or XP
#define TOUCH_XR A7 // or XM
#define X_MIN  170
#define X_MAX  870
#define Y_MIN  920
#define Y_MAX  130

#define TFT_RESET 24 // D24
#define TFT_D0 34
#define TFT_WR 26 // D25
#define TFT_DC 10 // D25
#define TFT_RD 9 // D9
#define TFT_RS 10 // D10
#define TFT_CS 11 // D11
#define TFT_TE 12 // D12
#define TFT_BACKLIGHT 25 // A25
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define SCREEN_PX (SCREEN_WIDTH * SCREEN_HEIGHT)

#define LIGHT_SENSOR A2

#define NEOPIXEL 2 // D2
#define LED 13 // D13

#define FILE_DIRECTORY "/"

#endif