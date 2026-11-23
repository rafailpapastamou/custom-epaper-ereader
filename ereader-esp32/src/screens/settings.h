#pragma once

enum SettingsAction
{
    SETTINGS_STAY,    // nothing for main.cpp to do
    SETTINGS_RESUME,  // go back to the book (needs a redraw)
    SETTINGS_RESUMED, // went back to the book, already drawn
    SETTINGS_LIBRARY  // close the book, go to the library
};

void settingsInit(); // loads the saved font; call after sdInit()/displayInit()
void showSettingsMenu();
void settingsUp();
void settingsDown();
SettingsAction settingsSelect(); // menu button
SettingsAction settingsBack();   // left button

const unsigned char *settingsReadingFont(); // used by reader.cpp