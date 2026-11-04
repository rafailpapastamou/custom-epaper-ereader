#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <epd4in2_V2.h>
#include <display.h>

#define BUTTON_MENU 4
#define BUTTON_UP 5
#define BUTTON_DOWN 6

#define SPI_SCK 14
#define SPI_MISO 12
#define SPI_MOSI 13

#define SD_CS 15

int prev_state_menu = HIGH;
int prev_state_up = HIGH;
int prev_state_down = HIGH;

int button_state_menu;
int button_state_up;
int button_state_down;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_MENU, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  if (!SD.begin(SD_CS, SPI))
  {
    Serial.println("SD Card initialization failed!");
  }
  else
  {
    Serial.println("SD Card initialized successfully.");
  }

  epd.Init();
  SPI.endTransaction();

  showPage("First test", "of my diy", "ereader", true);
}

void loop()
{
  button_state_menu = digitalRead(BUTTON_MENU);
  button_state_up = digitalRead(BUTTON_UP);
  button_state_down = digitalRead(BUTTON_DOWN);

  if (prev_state_menu == HIGH && button_state_menu == LOW)
  {
    Serial.println("Menu: pressed");
    showPage("hello", "", "", false);
  }

  if (prev_state_menu == LOW && button_state_menu == HIGH)
    Serial.println("Menu: released");

  if (prev_state_up == HIGH && button_state_up == LOW)
  {
    Serial.println("Up: pressed");
    showPage("button", "page up", "", false);
  }

  if (prev_state_up == LOW && button_state_up == HIGH)
    Serial.println("Up: released");

  if (prev_state_down == HIGH && button_state_down == LOW)
  {
    Serial.println("Down: pressed");
    showPage("button", "page down", "", false); // partial
  }

  if (prev_state_down == LOW && button_state_down == HIGH)
    Serial.println("Down: released");

  prev_state_menu = button_state_menu;
  prev_state_up = button_state_up;
  prev_state_down = button_state_down;
}