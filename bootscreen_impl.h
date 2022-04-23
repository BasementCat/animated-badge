#ifndef _BOOTSCREEN_IMPL_H_
#define _BOOTSCREEN_IMPL_H_

#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#include "constants.h"
#include "colors.h"

void bootscreen(Adafruit_ILI9341* tft) {
	int box_height = tft->height() / 7;
	tft->fillRect(0, 0, tft->width(), box_height, COLOR_RED);
	tft->fillRect(0, box_height, tft->width(), box_height, COLOR_ORANGE);
	tft->fillRect(0, box_height * 2, tft->width(), box_height, COLOR_YELLOW);
	tft->fillRect(0, box_height * 3, tft->width(), box_height, COLOR_GREEN);
	tft->fillRect(0, box_height * 4, tft->width(), box_height, COLOR_BLUE);
	tft->fillRect(0, box_height * 5, tft->width(), box_height, COLOR_PURPLE);
	tft->fillRect(0, box_height * 6, tft->width(), box_height, COLOR_MAGENTA);
	tft->setCursor(0, 0);
	tft->setTextColor(COLOR_WHITE, COLOR_BLACK);
	tft->setTextSize(3);
	tft->print(VERSION);
	delay(3000);
}

#endif