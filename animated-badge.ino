#include <Arduino.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ZeroDMA.h"
#include "TouchScreen.h"

#include <SD.h>

#include "FileList_impl.h"
#include "prefs.h"
#include "constants.h"
#include "bootscreen_impl.h"
#include "colors.h"
#include "QOIF2_impl.h"
#include "FileBuffer_impl.h"
#include "status_led_impl.h"
#include "main_touch_impl.h"


Adafruit_ILI9341 tft(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS, TFT_RESET, TFT_RD);
TouchScreen touchscreen(TOUCH_XL, TOUCH_YD, TOUCH_XR, TOUCH_YU, 300);

FileList files = FileList(FILE_DIRECTORY);
Prefs prefs;


void setup() {
	Serial.begin(SERIAL_SPEED);

	pinMode(TFT_BACKLIGHT, OUTPUT);
  	digitalWrite(TFT_BACKLIGHT, HIGH);

  	SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));

  	tft.begin();
  	tft.setRotation(4);

  	// TODO: re-enable me for prod
  	// bootscreen(&tft);
  	// still need a delay for serial
  	delay(500);

  	start_sd:
    tft.fillScreen(COLOR_BLUE);

    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        tft.fillScreen(COLOR_RED);
        // die("Failed to initialize SD card", false);
        delay(1000);
        goto start_sd;
    }
    Serial.println("OK!");

    read_prefs(&prefs);
    files.init(&prefs);

    status_led_init();

    // TODO: set backlight
    // ledcWrite(TFT_BL_CHAN, prefs.brightness);
}


void loop() {
	// TODO: zero screen in between images
    File fp;
    int call_res;
    long next_time, delay_until;
    bool one_frame = false, anim_completed = false, in_delay = false;
    static bool paused = false, locked = false;

    // next_time = millis() + (prefs.display_time_s * 1000);
    next_time = millis() + 4000;

    Serial.println("Open file");
    fp = SD.open(files.get_cur_file());
    if (!fp) {
        Serial.println("Can't open file, moving to next");
        Serial.flush();
        files.next_file(&prefs);
        return;
    }

    if (files.is_qoif2) {
        QOIF2 img(&tft, &fp);
        int res = img.open();
        switch (res) {
            case 0:
                goto QOIF2_OPEN_OK;
                break;
            case QOIF2_E_MAGIC:
                Serial.println("Opening QOIF2, bad magic");
                break;
            case QOIF2_E_DIMENSIONS:
                Serial.println("Opening QOIF2, bad dimensions");
                break;
            case QOIF2_E_CHANNELS:
                Serial.println("Opening QOIF2, bad channels");
                break;
            case QOIF2_E_VERSION:
                Serial.println("Opening QOIF2, bad version");
                break;
            default:
                Serial.println("Opening QOIF2, unknown error");
                break;
        }

        fp.close();
        files.next_file(&prefs);
        return;

        QOIF2_OPEN_OK:

        while (true) {
            in_delay = false;
            if (!one_frame) {
                res = img.read_and_render_block();
                switch (res) {
                    case QOIF2_E_TRAILER:
                        Serial.println("QOIF2: in trailer?");
                        fp.close();
                        files.next_file(&prefs);
                        return;
                        break;
                    case QOIF2_B_ONE_FRAME:
                        one_frame = true;
                        break;
                    case QOIF2_B_END:
                        anim_completed = true;
                        break;
                    case QOIF2_B_DELAY:
                        in_delay = true;
                        break;
                    case QOIF2_B_CONTINUE:
                        break;
                    default:
                        Serial.println("QOIF2: unknown error in block");
                        fp.close();
                        files.next_file(&prefs);
                        return;
                        break;
                }
            }

            if (!paused && anim_completed && millis() >= next_time) {
                break;
            }

            delay_until = millis();
            if (in_delay) {
                if (img.delay_ms > 0)
                    delay_until += img.delay_ms;

                if (img.delay_diff >= 0.95)
                    set_status_led(STATUS_LED_GOOD);
                else if (img.delay_diff >- 0.85)
                    set_status_led(STATUS_LED_OK);
                else if (img.delay_diff >= 0.75)
                    set_status_led(STATUS_LED_POOR);
                else
                    set_status_led(STATUS_LED_BAD);
            }

            do {
                switch (get_main_screen_touch(&touchscreen)) {
                    case MAIN_BTN_LOCK:
                        locked = !locked;
                        break;
                    case MAIN_BTN_LEFT:
                        if (locked) break;
                        fp.close();
                        files.prev_file(&prefs);
                        return;
                    case MAIN_BTN_RIGHT:
                        if (locked) break;
                        fp.close();
                        files.next_file(&prefs);
                        return;
                    case MAIN_BTN_PAUSE:
                        if (locked) break;
                        paused = !paused;
                        break;
                    case MAIN_BTN_MENU:
                        if (locked) break;
                        // TODO: implement a menu
                        break;
                }
            } while (millis() < delay_until);
        }

        files.next_file(&prefs);
    } else {
        Serial.println("Bad file type");
        fp.close();
        files.next_file(&prefs);
    }
}


void die(const char *message) {
    die(message, false);
}


void die(const char *message, bool dont_die) {
    tft.fillScreen(COLOR_BLACK);

    tft.setTextWrap(true);

    tft.setCursor(10, 15);
    tft.setTextColor(tft.color565(255, 0, 0));
    tft.setTextSize(3);
    tft.println("ERROR");

    tft.setCursor(0, 50);
    tft.setTextColor(0xffff);
    tft.setTextSize(1);
    tft.println(message);
    while (!dont_die) delay(1000);
}