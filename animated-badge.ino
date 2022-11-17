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
#include "Menus_impl.h"


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
        // Serial.println("failed!");
        // tft.fillScreen(COLOR_RED);
        die("Failed to initialize SD card", false);
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


bool paused = false, locked = false;


bool handle_main_touch(File* fp) {
    switch (get_main_screen_touch(&touchscreen)) {
        case MAIN_BTN_LOCK:
            locked = !locked;
            break;
        case MAIN_BTN_LEFT:
            if (locked) break;
            if (fp != NULL) fp->close();
            files.prev_file(&prefs);
            return true;
        case MAIN_BTN_RIGHT:
            if (locked) break;
            if (fp != NULL) fp->close();
            files.next_file(&prefs);
            return true;
        case MAIN_BTN_PAUSE:
            if (locked) break;
            paused = !paused;
            break;
        case MAIN_BTN_MENU:
            if (locked) break;
            // TODO: implement a menu
            break;
    }
    return false;
}


void loop() {
	// TODO: zero screen in between images
    File fp;
    int call_res;
    long next_time, delay_until;
    bool one_frame = false, anim_completed = false, in_delay = false, died = false;

    // next_time = millis() + (prefs.display_time_s * 1000);
    next_time = millis() + 4000;

    Serial.println("Open file");
    fp = SD.open(files.get_cur_file());
    delay(1000);
    if (!fp) {
        die("Can't open file", files.get_cur_file());
        died = true;
    } else {
        if (files.is_qoif2) {
            QOIF2 img(&tft, &fp);
            int res = img.open();
            if (res != 0) {
                switch (res) {
                    case QOIF2_E_MAGIC:
                        die("Opening QOIF2, bad magic", files.get_cur_file());
                        break;
                    case QOIF2_E_DIMENSIONS:
                        die("Opening QOIF2, bad dimensions", files.get_cur_file());
                        break;
                    case QOIF2_E_CHANNELS:
                        die("Opening QOIF2, bad channels", files.get_cur_file());
                        break;
                    case QOIF2_E_VERSION:
                        die("Opening QOIF2, bad version", files.get_cur_file());
                        break;
                    default:
                        die("Opening QOIF2, unknown error", files.get_cur_file());
                        break;
                }
                died = true;
            } else {
                while (true) {
                    in_delay = false;
                    if (!one_frame) {
                        res = img.read_and_render_block();
                        switch (res) {
                            case QOIF2_E_TRAILER:
                                die("QOIF2: in trailer?", files.get_cur_file());
                                died = true;
                                goto FILE_DONE;
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
                                die("QOIF2: unknown error in block", files.get_cur_file());
                                died = true;
                                goto FILE_DONE;
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
                        if (handle_main_touch(&fp)) return;
                    } while (millis() < delay_until);
                }
            }
        } else {
            die("Bad file type", files.get_cur_file());
            died = true;
        }
    }

    FILE_DONE:

    if (fp) fp.close();
    if (died) {
        next_time = millis() + 4000;
        do {
            if (handle_main_touch(NULL)) return;
        } while (millis() < next_time);
    }
    files.next_file(&prefs);
}


void die(const char *message) {
    die(message, true);
}


void die(const char *message, const char *filename) {
    char str[512];
    snprintf(str, sizeof(str), "%s\n\nFilename: %s", message, filename);
    die(str, false);
}


void die(const char *message, bool do_die) {
    tft.fillScreen(COLOR_BLACK);

    tft.setTextWrap(true);

    tft.setCursor(10, 15);
    tft.setTextColor(COLOR_RED);
    tft.setTextSize(3);
    tft.println("ERROR");

    tft.setCursor(0, 50);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.println(message);
    while (do_die) delay(1000);
}