// // #include <epd4in2_V2.h>
#include <cstdint>

void displayInit();
void displayRender(void (*draw)(), bool full);
void displayText(int x, int y, const char *s);
void displayRect(int x, int y, int w, int h);
int displayWidth();
int displayHeight();
int displayTextWidth(const char *s);
int displayFontAscent();
int displayFontDescent();
void displaySetFont(const uint8_t *font);
void displayUseUiFont();