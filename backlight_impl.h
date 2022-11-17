#ifndef _BACKLIGHT_IMPL_H_
#define _BACKLIGHT_IMPL_H_

#include "prefs.h"
#include "constants.h"

#define BACKLIGHT_UPDATE_FREQ 1000

long backlight_last_updated = 0;

void update_backlight(Prefs* prefs, bool force) {
    if (!force && backlight_last_updated && backlight_last_updated + BACKLIGHT_UPDATE_FREQ > millis())
        return;
    backlight_last_updated = millis();

    analogWrite(TFT_BACKLIGHT, prefs->brightness);
    // TODO: if read_prefs_flag(&prefs, PREF_FLAG_BL_AUTO), set up auto backlight
    // Serial.println(analogRead(LIGHT_SENSOR));
}

void update_backlight(Prefs* prefs) {
    update_backlight(prefs, false);
}

# endif