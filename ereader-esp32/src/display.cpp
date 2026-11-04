#include <Arduino.h>
#include <SPI.h>
#include <epd4in2_V2.h>
#include "fonts.h"
#include "epdpaint.h"
#include "display.h"

#define COLORED 0
#define UNCOLORED 1

#define FULL_REFRESH_EVERY 20

static unsigned char fb[(400 / 8) * 300];
Paint paint(fb, 400, 300);

Epd epd;

int partialCount = 0;

void showPage(const char *l1, const char *l2, const char *l3, bool fullRefresh)
{
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(10, 10, l1, &Font20, COLORED);
    paint.DrawStringAt(10, 50, l2, &Font20, COLORED);
    paint.DrawStringAt(10, 90, l3, &Font20, COLORED);

    if (fullRefresh || partialCount >= FULL_REFRESH_EVERY)
    {
        epd.Init();
        SPI.endTransaction();
        epd.Display_Base(paint.GetImage());
        partialCount = 0;
    }
    else
    {
        epd.Display_Partial(paint.GetImage(), 0, 0, 400, 300);
        partialCount++;
    }
}