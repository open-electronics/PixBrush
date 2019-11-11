/*  
 *  Author: Luca Bellan
 *   
 *  BEFORE UPLOAD:
 *    Edit file: FishinoXPT2046.h
 *    And uncomment/set following lines:
 *      #define TOUCH_CS  6
 *      #define TOUCH_IRQ 3
 */


#include "SPI.h"
#include "FishinoGFX.h"
#include "FishinoILI9341SPI.h"
#include "FishinoXPT2046.h"
#include <EEPROM.h>
#include <SD.h>
#include <FastLED.h>

//  Objects definition
#define tft FishinoILI9341SPI
#define touch FishinoXPT2046

//  Pin definition
#ifdef SDCS
  #define SD_CS SDCS
#else
  #define SD_CS 9
#endif
#define TFT_DC 5
#define TFT_CS 2
#define LED_PIN 7
#define BACKLIGHT_PIN 8
#define BUZZER_PIN 4

//  Buttons position definition
#define BCK_BUTTON_X 0
#define BCK_BUTTON_Y 270
#define BCK_BUTTON_W 50
#define BCK_BUTTON_H 50

#define DWN_BUTTON_X 63
#define DWN_BUTTON_Y 270
#define DWN_BUTTON_W 50
#define DWN_BUTTON_H 50

#define UP_BUTTON_X 126
#define UP_BUTTON_Y 270
#define UP_BUTTON_W 50
#define UP_BUTTON_H 50

#define OK_BUTTON_X 190
#define OK_BUTTON_Y 270
#define OK_BUTTON_W 50
#define OK_BUTTON_H 50

//  Menu elements definition
#define SELECT 0
#define BRIGHTNESS 1
#define DELAY 2
#define COUNTDOWN 3
#define BUZZER 4
#define LEDS 5
#define BACKLIGHT 6

//  Program variables
typedef struct {
  String Description;
  String Value;
} Parameters;

Parameters Params[7];
byte Page = 0;
byte EditParam = -1;
byte CursorMenuPos = 0;
bool BckTouched = false;
bool UpTouched = false;
bool DwnTouched = false;
bool OkTouched = false;
File root;
String Files[50];
byte FileNumber = 0;
byte CursorFilePos = 0;
unsigned long TimerBacklight = 0;

//  File reading variables
#define BUFFPIXEL 50
File     BmpFile, Root;
int      BmpWidth, BmpHeight;
uint8_t  BmpDepth;
uint32_t BmpImageOffset;
uint32_t RowSize;
uint8_t  SDBuffer[3*BUFFPIXEL];
uint8_t  BuffIDx = sizeof(SDBuffer);
boolean  Flip    = true;
uint16_t w, h, Col;
uint16_t Row = -1;
uint8_t  r, g, b;
uint32_t Pos = 0;
uint32_t StartScanTime;
uint32_t TotScanTime;

CRGB leds[288];

void setup() {

  Serial.begin(115200);
  tft.begin(TFT_CS, TFT_DC);
  tft.setRotation(2);
  touch.setRotation(2);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD initialization failed!");
  } else {
    ReadFiles();
  }

  //  Set Params
  Params[SELECT].Description = "SELECT FILE";
  Params[BRIGHTNESS].Description = "BRIGHTNESS";
  Params[DELAY].Description = "DELAY (ms)";
  Params[COUNTDOWN].Description = "COUNTDOWN";
  Params[BUZZER].Description = "BUZZER";
  Params[LEDS].Description = "LEDS";
  Params[BACKLIGHT].Description = "BACKLIGHT";

  //  Check first init: initialize values
  if (EEPROM.read(BRIGHTNESS) == 255) {
    Params[SELECT].Value = "..."; 
    Params[BRIGHTNESS].Value = "10";  
    Params[DELAY].Value = "25";  
    Params[COUNTDOWN].Value = "YES";  
    Params[BUZZER].Value = "NO";  
    Params[LEDS].Value = "60";
    Params[BACKLIGHT].Value = "90";
    EEPROM.write(BRIGHTNESS, Params[BRIGHTNESS].Value.toInt());
    EEPROM.write(DELAY, Params[DELAY].Value.toInt());
    EEPROM.write(COUNTDOWN, (Params[COUNTDOWN].Value == "YES" ? 1 : 0));
    EEPROM.write(BUZZER, (Params[BUZZER].Value == "YES" ? 1 : 0));
    EEPROM.write(LEDS, Params[LEDS].Value.toInt());
    EEPROM.write(BACKLIGHT, Params[BACKLIGHT].Value.toInt());
  } else {
  //  Load params value
    Params[SELECT].Value = "..."; 
    Params[BRIGHTNESS].Value = EEPROM.read(BRIGHTNESS);  
    Params[DELAY].Value = EEPROM.read(DELAY);  
    Params[COUNTDOWN].Value = (EEPROM.read(COUNTDOWN) == 1 ? "YES" : "NO");  
    Params[BUZZER].Value = (EEPROM.read(BUZZER) == 1 ? "YES" : "NO");  
    Params[LEDS].Value = EEPROM.read(LEDS);  
    Params[BACKLIGHT].Value = EEPROM.read(BACKLIGHT);  
  }

  //  Init display backlight
  analogWrite(BACKLIGHT_PIN, round(map(Params[BACKLIGHT].Value.toInt(), 0, 100, 255, 0)));
  TimerBacklight = millis();

  //  Init buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  Buzz(2, 0);

  //  Init LED strip
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, Params[LEDS].Value.toInt());
  for(byte i=0;i<Params[LEDS].Value.toInt();i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.setBrightness(Params[BRIGHTNESS].Value.toInt());
  FastLED.show();

  DrawInterface();

}

void loop() {
  
  //  Check touch input
  CheckTouchInput();
  
  //  Go ahead image scan if it's started
  ShowImageLine();
  
  //  Check if it's time to disable display backlight (no touch input after 30 sec)
  if (millis() - TimerBacklight >= 30000) {
    analogWrite(BACKLIGHT_PIN, 255);
  }

}



void DrawInterface() {

  byte MenuY = 50;
  tft.fillScreen(GFX_BLACK);


  //  MIDDLE SECTION
  switch(Page) {
    //  Menu
    case 0:
      //  Header
      tft.setTextColor(GFX_GREEN);
      tft.setFontScale(1);
      tft.setCursor(12, 14);
      tft.print("> READY TO CREATE? <");
      //  Menu options
      tft.setFontScale(2);
      for (byte i=0; i<7; i++) {
        tft.setTextColor(CursorMenuPos == i ? GFX_BLACK : GFX_WHITE);
        if (CursorMenuPos == i) {tft.fillRect(0, MenuY-1, 240, 16, GFX_WHITE);}
        tft.setCursor(20, MenuY);
        tft.print(Params[i].Description);
        tft.setCursor(190, MenuY);
        tft.print(Params[i].Value);
        MenuY += 20;
      }
    break;
    //  Select file...
    case 1:
      //  Header
      tft.setTextColor(GFX_GREEN);
      tft.setFontScale(2);
      tft.setCursor(12, 10);
      tft.print(Params[CursorMenuPos].Description);
      //  File list
      DisplayFiles();
    break;
    //  Edit params: Brightness, DELAY, Countdown, Buzzer, LEDS
    case 2:
      //  Header
      tft.setTextColor(GFX_GREEN);
      tft.setFontScale(2);
      tft.setCursor(12, 10);
      tft.print(Params[CursorMenuPos].Description);
      //  Edited value
      tft.setTextColor(GFX_WHITE);
      tft.setFontScale(5);
      tft.setCursor(90, 130);
      tft.print(Params[CursorMenuPos].Value + (CursorMenuPos == BRIGHTNESS || CursorMenuPos == BACKLIGHT ? "%" : ""));
    break;
    //  Paint!
    case 3:
      //  Header
      tft.setTextColor(GFX_GREEN);
      tft.setFontScale(2);
      tft.setCursor(12, 10);
      tft.print("PAINT");
      //  Countdown (if set)
      if (Params[COUNTDOWN].Value == "YES") {
        for(byte i=3; i>0; i--) {
          tft.fillRect(110, 130, 150, 40, GFX_BLACK);
          tft.setTextColor(GFX_WHITE);
          tft.setFontScale(5);
          tft.setCursor(110, 130);
          tft.print(i);
          Buzz(200, 0);
          delay((Params[BUZZER].Value == "YES" ? 800 : 1000));
        }
      }
      //  Write "Paint!"
      tft.fillRect(90, 130, 150, 40, GFX_BLACK);
      tft.setTextColor(GFX_YELLOW);
      tft.setFontScale(5);
      tft.setCursor(38, 130);
      tft.print("Paint!");
      Buzz(500, 0);
      //  Start image display on NeoPixels line by line
      StartScanImage(Files[CursorFilePos]);
    break;
    //  ERROR: width of the image is not equal to LEDS
    case 4:
      //  Header
      tft.setTextColor(GFX_GREEN);
      tft.setFontScale(2);
      tft.setCursor(12, 10);
      tft.print("ERROR");
      //  Error text
      tft.setTextColor(GFX_WHITE);
      tft.setFontScale(3);
      tft.setCursor(0, 100);
      tft.print("Error: image width and LEDnumber are   different!");      
    break;
  }

  //  BOTTOM SECTION
  //  Back button
  if (Page == 1 || Page == 3) {
    tft.fillRect(BCK_BUTTON_X, BCK_BUTTON_Y, BCK_BUTTON_W, BCK_BUTTON_H, GFX_RED);
    tft.fillTriangle(35, 280, 10, 295, 35, 310, GFX_WHITE);
  }
  // Down button
  if (Page == 0 || Page == 1 || Page == 2) {
    tft.fillRect(DWN_BUTTON_X, DWN_BUTTON_Y, DWN_BUTTON_W, DWN_BUTTON_H, GFX_RED);
    tft.fillTriangle(87, 310, 72, 280, 103, 280, GFX_WHITE);
  }  
  // Up button
  if (Page == 0 || Page == 1 || Page == 2) {
    tft.fillRect(UP_BUTTON_X, UP_BUTTON_Y, UP_BUTTON_W, UP_BUTTON_H, GFX_RED);
    tft.fillTriangle(135, 310, 167, 310, 151, 280, GFX_WHITE);
  }
  // OK button
  if (Page == 0 || Page == 1 || Page == 2 || Page == 4) {
    tft.fillRect(OK_BUTTON_X, OK_BUTTON_Y, OK_BUTTON_W, OK_BUTTON_H, GFX_RED);
    tft.setTextColor(GFX_WHITE);
    tft.setFontScale(3);
    tft.setCursor(198, 285);
    tft.print("OK");
  }
  
}

void CheckTouchInput() {

  if (touch.touching()) {
    //  Switch on display backlight
    analogWrite(BACKLIGHT_PIN, round(map(Params[BACKLIGHT].Value.toInt(), 0, 100, 255, 0)));
    TimerBacklight = millis();
    //  Check button hit
    uint16_t x, y;
    touch.read(x, y);
    if (x >= BCK_BUTTON_X && x <= BCK_BUTTON_X + BCK_BUTTON_W && y >= BCK_BUTTON_Y && y <= BCK_BUTTON_Y + BCK_BUTTON_H) {
      BckTouched = true;
    }
    if (x >= DWN_BUTTON_X && x <= DWN_BUTTON_X + DWN_BUTTON_W && y >= DWN_BUTTON_Y && y <= DWN_BUTTON_Y + DWN_BUTTON_H) {
      DwnTouched = true;
    }    
    if (x >= UP_BUTTON_X && x <= UP_BUTTON_X + UP_BUTTON_W && y >= UP_BUTTON_Y && y <= UP_BUTTON_Y + UP_BUTTON_H) {
      UpTouched = true;
    }
    if (x >= OK_BUTTON_X && x <= OK_BUTTON_X + OK_BUTTON_W && y >= OK_BUTTON_Y && y <= OK_BUTTON_Y + OK_BUTTON_H) {
      OkTouched = true;
    }
  }
  if (!touch.touching() && BckTouched == true) {
    BckTouched = false;
    Buzz(2, 0);
    switch (Page) {
      //  Select file...
      case 1:
        Page = 0;
        DrawInterface();
      break;
      case 3:
        StopScanImage(0);
      break;
    }
  }
  if (!touch.touching() && DwnTouched == true) {
    DwnTouched = false;
    Buzz(2, 0);
    switch (Page) {
      //  Menu
      case 0:
        if(CursorMenuPos < 6) {
          CursorMenuPos += 1;
          tft.setFontScale(2);
          //  Reset previous line
          tft.setTextColor(GFX_WHITE);
          tft.fillRect(0, 50+((CursorMenuPos-1)*20)-1, 240, 16, GFX_BLACK);
          tft.setCursor(20, 50+((CursorMenuPos-1)*20));
          tft.print(Params[CursorMenuPos-1].Description);
          tft.setCursor(190, 50+((CursorMenuPos-1)*20));
          tft.print(Params[CursorMenuPos-1].Value);
          //  Mark current line
          tft.setTextColor(GFX_BLACK);
          tft.fillRect(0, 50+(CursorMenuPos*20)-1, 240, 16, GFX_WHITE);
          tft.setCursor(20, 50+(CursorMenuPos*20));
          tft.print(Params[CursorMenuPos].Description);
          tft.setCursor(190, 50+(CursorMenuPos*20));
          tft.print(Params[CursorMenuPos].Value);
        }
      break;
      //  Select file...
      case 1:
        if (CursorFilePos == FileNumber - 1) {
          CursorFilePos = 0;
          DrawInterface();
        } else if (CursorFilePos == (CursorFilePos - CursorFilePos % 10) + 9) {
          CursorFilePos++;
          DrawInterface();
        } else{
          CursorFilePos++;
          tft.setFontScale(2);
          //  Reset previous line
          tft.setTextColor(GFX_WHITE);
          tft.fillRect(0, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10)-1)*20)-1, 240, 16, GFX_BLACK);
          tft.setCursor(20, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10)-1)*20));
          tft.print(Files[CursorFilePos-1]);
          //  Mark current line
          tft.setTextColor(GFX_BLACK);
          tft.fillRect(0, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10))*20)-1, 240, 16, GFX_WHITE);
          tft.setCursor(20, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10))*20));
          tft.print(Files[CursorFilePos]);
        }
      break;
      //  Edit params
      case 2:
        switch(CursorMenuPos) {
          case BRIGHTNESS:
          case LEDS:
            if(Params[CursorMenuPos].Value.toInt() > 0) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() - 1;
            }
          break;
          case DELAY:
            if(Params[CursorMenuPos].Value.toInt() > 25) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() - 1;
            }
          break;
          case BACKLIGHT:
            if(Params[CursorMenuPos].Value.toInt() > 0) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() - 10;
              //Re-set display backlight
              analogWrite(BACKLIGHT_PIN, round(map(Params[BACKLIGHT].Value.toInt(), 0, 100, 255, 0)));
            }
          break;
          case COUNTDOWN:
          case BUZZER:
            if(Params[CursorMenuPos].Value == "YES") {
              Params[CursorMenuPos].Value = "NO";
            } else {
              Params[CursorMenuPos].Value = "YES";
            }
          break;
        }
        tft.fillRect(90, 130, 150, 40, GFX_BLACK);
        tft.setTextColor(GFX_WHITE);
        tft.setFontScale(5);
        tft.setCursor(90, 130);
        tft.print(Params[CursorMenuPos].Value + (CursorMenuPos == BRIGHTNESS || CursorMenuPos == BACKLIGHT ? "%" : ""));
      break;
    }
  }
  if (!touch.touching() && UpTouched == true) {
    UpTouched = false;
    Buzz(2, 0);
    switch (Page) {
      //  Menu
      case 0:
        if(CursorMenuPos > 0) {
          CursorMenuPos -= 1;
          tft.setFontScale(2);
          //  Reset previous line
          tft.setTextColor(GFX_WHITE);
          tft.fillRect(0, 50+((CursorMenuPos+1)*20)-1, 240, 16, GFX_BLACK);
          tft.setCursor(20, 50+((CursorMenuPos+1)*20));
          tft.print(Params[CursorMenuPos+1].Description);
          tft.setCursor(190, 50+((CursorMenuPos+1)*20));
          tft.print(Params[CursorMenuPos+1].Value);
          //  Mark current line
          tft.setTextColor(GFX_BLACK);
          tft.fillRect(0, 50+(CursorMenuPos*20)-1, 240, 16, GFX_WHITE);
          tft.setCursor(20, 50+(CursorMenuPos*20));
          tft.print(Params[CursorMenuPos].Description);
          tft.setCursor(190, 50+(CursorMenuPos*20));
          tft.print(Params[CursorMenuPos].Value);
        }
      break;
      //  Select file...
      case 1:
        if (CursorFilePos == 0) {
          CursorFilePos = FileNumber - 1;
          DrawInterface();
        } else if (CursorFilePos == (CursorFilePos - CursorFilePos % 10)) {
          CursorFilePos--;
          DrawInterface();
        } else {
          CursorFilePos--;
          tft.setFontScale(2);
          //  Reset previous line
          tft.setTextColor(GFX_WHITE);
          tft.fillRect(0, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10)+1)*20)-1, 240, 16, GFX_BLACK);
          tft.setCursor(20, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10)+1)*20));
          tft.print(Files[CursorFilePos+1]);
          //  Mark current line
          tft.setTextColor(GFX_BLACK);
          tft.fillRect(0, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10))*20)-1, 240, 16, GFX_WHITE);
          tft.setCursor(20, 50+((CursorFilePos-(CursorFilePos - CursorFilePos % 10))*20));
          tft.print(Files[CursorFilePos]);
        }
      break;
      //  Edit params
      case 2:
        switch(CursorMenuPos) {
          case BRIGHTNESS:
            if(Params[CursorMenuPos].Value.toInt() < 100) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() + 1;
            }
          break;
          case BACKLIGHT:
            if(Params[CursorMenuPos].Value.toInt() < 100) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() + 10;
              //Re-set display backlight
              analogWrite(BACKLIGHT_PIN, round(map(Params[BACKLIGHT].Value.toInt(), 0, 100, 255, 0)));
            }
          break;
          case DELAY:
            if(Params[CursorMenuPos].Value.toInt() < 1000) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() + 1;
            }
          break;
          case COUNTDOWN:
          case BUZZER:
            if(Params[CursorMenuPos].Value == "YES") {
              Params[CursorMenuPos].Value = "NO";
            } else {
              Params[CursorMenuPos].Value = "YES";
            }
          break;
          case LEDS:
            if(Params[CursorMenuPos].Value.toInt() < 150) {
              Params[CursorMenuPos].Value = Params[CursorMenuPos].Value.toInt() + 1;
            }
          break;
        }
        tft.fillRect(90, 130, 150, 40, GFX_BLACK);
        tft.setTextColor(GFX_WHITE);
        tft.setFontScale(5);
        tft.setCursor(90, 130);
        tft.print(Params[CursorMenuPos].Value + (CursorMenuPos == BRIGHTNESS || CursorMenuPos == BACKLIGHT ? "%" : ""));
      break;
    }
  }  
  if (!touch.touching() && OkTouched == true) {
    OkTouched = false;
    Buzz(2, 0);
    switch (Page) {
      //  Menu
      case 0:
        Page = CursorMenuPos == 0 ? 1 : 2;
        CursorFilePos = 0;
        DrawInterface();
      break;
      //  Select file...
      case 1:
        Page = 3;
        DrawInterface();
      break;
      //  Edit params
      case 2:
        Page = 0;
        DrawInterface();
        switch(CursorMenuPos) {
          case BRIGHTNESS:
          case DELAY:
          case LEDS:
          case BACKLIGHT:
            if (EEPROM.read(CursorMenuPos) != Params[CursorMenuPos].Value.toInt()) {
              EEPROM.write(CursorMenuPos, Params[CursorMenuPos].Value.toInt());
            }
            if (CursorMenuPos == LEDS) {
              //  Re-init LED strip
              FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, Params[LEDS].Value.toInt());
              FastLED.clear();
              FastLED.show();
            }
            if (CursorMenuPos == BRIGHTNESS) {
              //  Set strip brightness 
              FastLED.setBrightness(Params[BRIGHTNESS].Value.toInt());
            }
          break;
          case COUNTDOWN:
          case BUZZER:
            if (EEPROM.read(CursorMenuPos) != (Params[CursorMenuPos].Value == "YES" ? 1 : 0)) {
              EEPROM.write(CursorMenuPos, (Params[CursorMenuPos].Value == "YES" ? 1 : 0));
            }
          break;
        }
      break;
      //  ERROR: width of the image is not equal to LEDS
      case 4:
        Page = 0;
        DrawInterface();
      break;
    }
  }
  
}


void DisplayFiles() {
  //  Read all file names from array and display page by page (max 10 per page)
  //  Page is calculated by CursorFilePos
  byte FileY = 50;
  byte Page = (CursorFilePos - CursorFilePos % 10) / 10;
  tft.setFontScale(2);
  for(int i=Page*10; i<((Page+1)*10); i++) {
    if(i==FileNumber){break;}
    tft.setTextColor(CursorFilePos == i ? GFX_BLACK : GFX_WHITE);
    if (CursorFilePos == i) {tft.fillRect(0, FileY-1, 240, 16, GFX_WHITE);}
    tft.setCursor(20, FileY); 
    tft.print(Files[i]);
    FileY += 20;
  }
}


void ReadFiles() {
  //  Read all files from SD root, put in Files array and set FileNumber
  Root = SD.open("/");
  while (true) {
    File Entry =  Root.openNextFile();
    if (!Entry) {
      break;
    }
    //  Check BMP signature
    if (read16(Entry) == 0x4D42) {
      //  Read & ignore file size
      read32(Entry);
      // Read & ignore creator bytes
      read32(Entry);
      // Start of image data
      BmpImageOffset = read32(Entry); 
      // Read DIB header
      read32(Entry);
      //  Read dimensions
      BmpWidth  = read32(Entry);
      BmpHeight = read32(Entry);
      //  Check planes
      if (read16(Entry) == 1) {
        //  Read depth (bits per pixel)
        BmpDepth = read16(Entry);
        //  Check if it's 24 bit and uncompressed (0)
        if ((BmpDepth == 24) && (read32(Entry) == 0)) {
          Files[FileNumber] = Entry.name();
          FileNumber++;
        }
      }
    }
    Entry.close();
  }
}

void StartScanImage(String img) {
  //  Open and start the scanning of the image
  BmpFile = SD.open(img);
  //  Check BMP signature
  if (read16(BmpFile) == 0x4D42) {
    //  Read & ignore file size
    read32(BmpFile);
    // Read & ignore creator bytes
    read32(BmpFile);
    // Start of image data
    BmpImageOffset = read32(BmpFile); 
    // Read DIB header
    read32(BmpFile);
    //  Read dimensions
    BmpWidth  = read32(BmpFile);
    BmpHeight = read32(BmpFile);
    //  Check planes
    if (read16(BmpFile) == 1) {
      //  Read depth (bits per pixel)
      BmpDepth = read16(BmpFile);
      //  Check if it's 24 bit and uncompressed (0)
      if ((BmpDepth == 24) && (read32(BmpFile) == 0)) {
        // BMP Rows are padded (if needed) to 4-byte boundary
        RowSize = (BmpWidth * 3 + 3) & ~3;
        // If BmpHeight is negative, image is in top-down order.
        if (BmpHeight < 0) {
          BmpHeight = -BmpHeight;
          Flip      = false;
        }
        // Crop area to be loaded
        w = BmpWidth;
        h = BmpHeight;
        //  Check if width is not equal to LEDS --> ERROR
        if (w != Params[LEDS].Value.toInt()) {
          StopScanImage(4);
          Buzz(100, 5);
        } else {
          Row = 0;
          tft.setTextColor(GFX_WHITE);
          tft.setFontScale(2);
          tft.setCursor(112, 200);
          tft.print("/" + String(h));
        }
      }
    }
  }
}

void ShowImageLine() {
  
  if(Row < h && Row != -1) {
    StartScanTime = millis();
    if (Flip) {
      Pos = BmpImageOffset + (BmpHeight - 1 - Row) * RowSize;
    } else {
      Pos = BmpImageOffset + Row * RowSize;
    }
    // Seek
    if (BmpFile.position() != Pos) {
      BmpFile.seek(Pos);
      BuffIDx = sizeof(SDBuffer);
    }
    //  Start column scan (pixel by pixel)
    /*
    Serial.println("------");
    Serial.print("ROW: ");
    Serial.println(Row);
    */
    for (Col=0; Col<w; Col++) {
      // Read more pixel data
      if (BuffIDx >= sizeof(SDBuffer)) {
        BmpFile.read(SDBuffer, sizeof(SDBuffer));
        BuffIDx = 0;
      }
      // Read pixel colors
      b = SDBuffer[BuffIDx++];
      g = SDBuffer[BuffIDx++];
      r = SDBuffer[BuffIDx++];
      //  Update NeoPixel strip
      leds[Col] = CRGB(r, g, b);
      /*
      Serial.print("Col: ");
      Serial.print(Col);
      Serial.print("  ");
      Serial.print(r);
      Serial.print(", ");
      Serial.print(g);
      Serial.print(", ");
      Serial.println(b);
      */
    }
    FastLED.show();
    Row++;
    //  Update the row number on the screen
    tft.fillRect(65, 200, 46, 14, GFX_BLACK);
    tft.setTextColor(GFX_WHITE);
    tft.setFontScale(2);
    tft.setCursor(65, 200);
    tft.print(String(Row));
    //  Wait the delay
    TotScanTime = millis() - StartScanTime;
    delay(TotScanTime > Params[DELAY].Value.toInt() ? 0 : Params[DELAY].Value.toInt()-TotScanTime);
  } else if (Row >= h && Row != -1) {
    StopScanImage(0);
  }

}

void StopScanImage(byte page) {
  //  Reset NeoPixel strip
  FastLED.clear();
  FastLED.show();
  //  Close image file and reset row index
  BmpFile.close();
  Row = -1;
  //  Go to specific page
  Page = page;
  DrawInterface();
  //  Buzzer sound only if the correct scanning is finished
  if(page == 0) {
    Buzz(70, 2);  
  }
}


void Buzz(int interval, byte repeat) {
  if (Params[BUZZER].Value == "YES") {
    if (repeat == 0) {
      analogWrite(BUZZER_PIN, 70);
      delay(interval);
      analogWrite(BUZZER_PIN, 0);
    } else {
      for (byte i=0;i<repeat;i++) {
        analogWrite(BUZZER_PIN, 70);
        delay(interval);
        analogWrite(BUZZER_PIN, 0);
        delay(interval);
      }
    }
  }
}


uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
