#ifndef _PREFS_H_
#define _PREFS_H_

#include <SD.h>

#define PREFS_VERSION 3
#define PREFS_FILENAME "/preferences.bin"

#define PREFS_FLAG_BL_AUTO 0

typedef struct {
    uint16_t version;
    uint16_t display_time_s;
    char last_filename[128];
    uint8_t brightness;
    uint16_t flags;
    uint8_t bri_auto_min;
    uint8_t bri_auto_max;
} __attribute__ ((packed)) Prefs;

void set_pref_last_filename(Prefs* prefs, const char* filename);
void set_pref_flag(Prefs* prefs, int flag, bool value);
bool read_pref_flag(Prefs* prefs, int flag);
void write_prefs(Prefs* prefs);
void read_prefs(Prefs* prefs);

#endif