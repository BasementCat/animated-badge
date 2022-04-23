#ifndef _COLORS_H_
#define _COLORS_H_

// Color definitions
#define COLOR_BLACK 0x0000       ///<   0,   0,   0
#define COLOR_NAVY 0x000F        ///<   0,   0, 123
#define COLOR_DARKGREEN 0x03E0   ///<   0, 125,   0
#define COLOR_DARKCYAN 0x03EF    ///<   0, 125, 123
#define COLOR_MAROON 0x7800      ///< 123,   0,   0
#define COLOR_PURPLE 0x780F      ///< 123,   0, 123
#define COLOR_OLIVE 0x7BE0       ///< 123, 125,   0
#define COLOR_LIGHTGREY 0xC618   ///< 198, 195, 198
#define COLOR_DARKGREY 0x7BEF    ///< 123, 125, 123
#define COLOR_BLUE 0x001F        ///<   0,   0, 255
#define COLOR_GREEN 0x07E0       ///<   0, 255,   0
#define COLOR_CYAN 0x07FF        ///<   0, 255, 255
#define COLOR_RED 0xF800         ///< 255,   0,   0
#define COLOR_MAGENTA 0xF81F     ///< 255,   0, 255
#define COLOR_YELLOW 0xFFE0      ///< 255, 255,   0
#define COLOR_WHITE 0xFFFF       ///< 255, 255, 255
#define COLOR_ORANGE 0xFD20      ///< 255, 165,   0
#define COLOR_GREENYELLOW 0xAFE5 ///< 173, 255,  41
#define COLOR_PINK 0xFC18        ///< 255, 130, 198

// uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
//     return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
// }

#endif