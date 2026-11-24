#include "usb.h"
#include <USB.h>
#include <USBMSC.h>
#include <SD.h>
#include "display/display.h"
#include "screens/main_menu.h"
#include "screens/reader.h"
#include "screens/settings.h"

enum screen_selected
{
    MAIN_SCREEN,
    BOOK_SCREEN,
    SETTINGS_SCREEN
};

struct Button
{
    uint8_t pin;
    int lastReading;
    int stableState;
    unsigned long lastDebounceTime;
};

extern Button menuButton;
extern Button leftButton;
extern screen_selected screen;
extern bool buttonPressed(Button &button);

static USBMSC msc;
static volatile bool usbHostConnected = false;
static volatile bool usbEjected = false;
static bool usbPromptShown = false;
static bool usbDeclined = false;
static bool usbMode = false;

bool usbIsActive()
{
    return usbMode;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)offset;
    for (uint32_t i = 0; i < bufsize / 512; i++)
    {
        SD.readRAW((uint8_t *)buffer + i * 512, lba + i);
    }
    return bufsize;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    (void)offset;
    for (uint32_t i = 0; i < bufsize / 512; i++)
    {
        SD.writeRAW(buffer + i * 512, lba + i);
    }
    return bufsize;
}

static bool onStartStop(uint8_t power, bool start, bool eject)
{
    (void)power;
    (void)start;
    if (eject)
    {
        usbEjected = true;
    }
    return true;
}

static void usbEventCallback(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base != ARDUINO_USB_EVENTS)
    {
        return;
    }

    if (id == ARDUINO_USB_STARTED_EVENT)
    {
        usbHostConnected = true;
    }
    else if (id == ARDUINO_USB_STOPPED_EVENT)
    {
        usbHostConnected = false;
        if (usbMode)
        {
            usbEjected = true;
        }
    }
}

static void showUsbPrompt()
{
    // Βάλε εδώ τον κώδικα οθόνης σου, π.χ.:
    // "Σύνδεση με PC?  MENU=Ναι  LEFT=Όχι"
    Serial.println("USB detected: MENU = Connect, LEFT = Cancel");
}

static void showUsbConnected()
{
    // Βάλε εδώ τον κώδικα οθόνης σου, π.χ.:
    // "Συνδέθηκε. Κάνε eject από τον PC."
    Serial.println("USB MSC active. Safely eject from PC when done.");
}

static void redrawCurrentScreen()
{
    if (screen == MAIN_SCREEN)
    {
        menuOpen();
    }
    else if (screen == BOOK_SCREEN)
    {
        readerResume();
    }
    else if (screen == SETTINGS_SCREEN)
    {
        showSettingsMenu();
    }
}

void usbInit()
{
    msc.vendorID("EREADER");
    msc.productID("Books");
    msc.productRevision("1.0");
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.onStartStop(onStartStop);
    msc.mediaPresent(false); // κρυφός δίσκος μέχρι να πατήσει ο χρήστης Σύνδεση
    msc.begin(SD.numSectors(), 512);
    USB.onEvent(usbEventCallback);
    USB.begin();
}

void usbLoop()
{
    if (usbEjected)
    {
        delay(500);
        ESP.restart(); // ξανασαρώνει τη βιβλιοθήκη μετά το eject
    }

    if (usbMode)
    {
        return;
    }

    if (!usbHostConnected)
    {
        usbPromptShown = false;
        usbDeclined = false;
        return;
    }

    if (usbDeclined)
    {
        return;
    }

    if (!usbPromptShown)
    {
        showUsbPrompt();
        usbPromptShown = true;
    }

    if (buttonPressed(menuButton))
    {
        usbMode = true;
        msc.mediaPresent(true);
        showUsbConnected();
    }
    else if (buttonPressed(leftButton))
    {
        usbDeclined = true;
        redrawCurrentScreen();
    }
}