#ifndef _SCREEN_IMPL_H_
#define _SCREEN_IMPL_H_

#include "Adafruit_ILI9341.h"

#include "constants.h"


class Screen {
public:
	void Screen(Adafruit_ILI9341* tft) {
		this->tft = tft;
	}

	void setAddrWindow(int x, int y, int w, int h) {
		this->sc_x = x;
		this->sc_y = y;
		this->sc_w = w;
		this->sc_h = h;
	}

	void setAddrWindow() {
		this->setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
private:
	Adafruit_ILI9341* tft;
	uint16_t buf[2][SCREEN_PX];
	int wbuf = 0, rbuf = 0;
	int sc_x = 0, sc_y = 0;
	int sc_w = SCREEN_WIDTH, sc_h = SCREEN_HEIGHT;
}

#endif