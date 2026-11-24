#include <Arduino.h>
#include <SPI.h>
#include "epub/usb.h"
#include "display/display.h"
#include "epub/sd_card.h"
#include "screens/main_menu.h"
#include "screens/reader.h"
#include "screens/settings.h"

#define BUTTON_MENU 4
#define BUTTON_UP 5
#define BUTTON_DOWN 6
#define BUTTON_RIGHT 16
#define BUTTON_LEFT 17

#define DEBOUNCE_DELAY 50

#define SPI_SCK 14
#define SPI_MISO 12
#define SPI_MOSI 13

SET_LOOP_TASK_STACK_SIZE(32 * 1024);

enum screen_selected
{
  MAIN_SCREEN,
  BOOK_SCREEN,
  SETTINGS_SCREEN
};

screen_selected screen = MAIN_SCREEN;

struct Button
{
  uint8_t pin;
  int lastReading;
  int stableState;
  unsigned long lastDebounceTime;
};

Button menuButton = {BUTTON_MENU, HIGH, HIGH, 0};
Button upButton = {BUTTON_UP, HIGH, HIGH, 0};
Button downButton = {BUTTON_DOWN, HIGH, HIGH, 0};
Button rightButton = {BUTTON_RIGHT, HIGH, HIGH, 0};
Button leftButton = {BUTTON_LEFT, HIGH, HIGH, 0};

bool buttonPressed(Button &button)
{
  int reading = digitalRead(button.pin);

  if (reading != button.lastReading)
  {
    button.lastDebounceTime = millis();
    button.lastReading = reading;
  }

  if (millis() - button.lastDebounceTime >= DEBOUNCE_DELAY)
  {
    if (reading != button.stableState)
    {
      button.stableState = reading;

      if (button.stableState == LOW)
      {
        return true;
      }
    }
  }

  return false;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_MENU, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  sdInit(SPI);
  displayInit();
  settingsInit();
  menuOpen();

  usbInit(); // αρχικοποιείται αφού έχει γίνει sdInit(SPI) ώστε να υπάρχει το SD.numSectors()
}

static void applySettingsAction(SettingsAction a)
{
  switch (a)
  {
  case SETTINGS_RESUME:
    readerResume();
    screen = BOOK_SCREEN;
    break;
  case SETTINGS_RESUMED:
    screen = BOOK_SCREEN;
    break;
  case SETTINGS_LIBRARY:
    readerClose();
    screen = MAIN_SCREEN;
    break;
  default:
    break;
  }
}

void loop()
{
  usbLoop();

  // Όταν ο δίσκος είναι ενεργός στον υπολογιστή, παγώνουμε τον κανονικό χειρισμό
  // μέχρι να γίνει eject και restart.
  if (usbIsActive())
  {
    delay(10);
    return;
  }

  if (buttonPressed(menuButton))
  {
    Serial.println("Menu: pressed");

    if (screen == MAIN_SCREEN)
    {
      openBook();
      screen = BOOK_SCREEN;
    }
    else if (screen == BOOK_SCREEN)
    {
      showSettingsMenu();
      screen = SETTINGS_SCREEN;
    }
    else
    {
      applySettingsAction(settingsSelect());
    }
  }

  if (buttonPressed(upButton))
  {
    Serial.println("Up: pressed");

    if (screen == MAIN_SCREEN)
      goToPreviousBook();
    else if (screen == BOOK_SCREEN)
      goToPreviousPage();
    else
      settingsUp();
  }

  if (buttonPressed(downButton))
  {
    Serial.println("Down: pressed");

    if (screen == MAIN_SCREEN)
      goToNextBook();
    else if (screen == BOOK_SCREEN)
      goToNextPage();
    else
      settingsDown();
  }

  if (buttonPressed(rightButton))
  {
    Serial.println("Right: pressed");

    if (screen == MAIN_SCREEN)
      goToNextLetter();
    else if (screen == BOOK_SCREEN)
      goToNextPage();
  }

  if (buttonPressed(leftButton))
  {
    Serial.println("Left: pressed");

    if (screen == MAIN_SCREEN)
      goToPreviousLetter();
    else if (screen == BOOK_SCREEN)
      goToPreviousPage();
    else
      applySettingsAction(settingsBack());
  }
}