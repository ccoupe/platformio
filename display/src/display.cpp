// Display Layout Version 2
#include <Arduino.h> 
#include "Device.h"
#include "MQTT_Ranger.h"
/* 
* The TFT_eSPI library has some 'terminal' or character / line drawing 
* methods. X and Y are still pixels. For these calls Y appears to
* be the top,left of the cell - unlike toolkits that display at the
* baseline. 
*
* In theory the u8x8lib stuff could be done my TFT_eSPI.
*/

#if defined(TFT_SPI)
#include "SPI.h"
#include "TFT_eSPI.h"
//#include "font.h"                 //TODO

TFT_eSPI lcd = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);

#elif defined(DISPLAY_U8)
//#include <U8x8lib.h>
#include <U8g2lib.h>
//U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#elif defined(LCD_160X)
#include <LiquidCrystal_I2C.h> 
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

void myterm(int x, int y, String ln);

#ifdef TFT_SPI
int fontHgt;
#endif

#ifdef DISPLAY_U8
int display_columns;
int display_lines;
int device_width;
int max_char_width;
int spaceWidth;
int chosen_font = 1;      // modified by Mqtt {"settings": {"font": <n>}}}
int fontHgt = 16;
int baseline;
#endif

void displayOn() {

}

void displayOff() {
#if defined(DISPLAY_U8)
  //u8x8.clear();
  u8g2.clear();
#elif defined(TFT_SPI)
  lcd.fillScreen(TFT_BLACK);
#elif defined(LCD160X)
  lcd.nobacklight();
#endif
}

void switch_font(int incoming) {
  device_width = u8g2.getDisplayWidth();
  if (incoming == 2) {
    //u8g2.setFont(u8g2_font_inr16_mf); // bit too tall
    u8g2.setFont(u8g2_font_t0_22_tf); 
    display_columns = 16;
    display_lines = 3;
  } else if (incoming == 3) {
    // u8g2.setFont(u8g2_font_t0_11_tf); // bit too small
    u8g2.setFont(u8g2_font_7x14_tf);
    display_columns = 16;
    display_lines = 4;
  } else {
    // Font 1 is the Big Font - also the default
    // u8g2.setFont(u8g2_font_inr21_mf); // bit too tall
    u8g2.setFont(u8g2_font_helvB18_tf);
    display_columns = 8;
    display_lines = 2;
  }
  baseline = u8g2.getAscent();
  fontHgt = u8g2.getMaxCharHeight() + 1;
  spaceWidth = u8g2.getUTF8Width(" ");
  max_char_width =u8g2.getMaxCharWidth();
}

// This is tcalled from the Arduino setup()
void display_init() {
#if defined(DISPLAY_U8)
  //Serial.println("leaving display_init()");
  //return;
  u8g2.begin();
  u8g2.firstPage();
  do {
    u8g2.clear();
    switch_font(3);
    int y = baseline;
    u8g2.drawUTF8(0, y, HDEVICE);
    y += fontHgt;
    u8g2.drawUTF8(0, y, __DATE__);
    y += fontHgt;
    u8g2.drawUTF8(0, y, __TIME__);
    y += fontHgt;
    u8g2.drawUTF8(3*8, y ,"Hello");
    y += fontHgt;
    u8g2.drawUTF8(0, y,"There");
 } while (u8g2.nextPage());
/* 
  u8x8.print();
  u8x8.setCursor(0,5);
  u8x8.print("Big Boy");
  u8x8.setCursor(0,6);
  u8x8.print("Seven Lines!");
*/  
 #elif defined(TFT_SPI)
  Serial.println("Init TFT_SPI");
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextFont(4);
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextWrap(false, false); 
  fontHgt = lcd.fontHeight();
#if 0 // test cursor positioning - it's by pixel, not character
  for (int i = 0; i < DISPLAY_LINES; i++) {
    lcd.setCursor(i*12,i*fontHgt);
    lcd.print("Trumpy "+String(i+1));
    Serial.println("line "+String(i)+" pos "+String(i*fontHgt));
  }
  fontinfo fnt = fontdata[4];
  Serial.println("FONT Hgt: "+String(fnt.height)+" Baseline: "+String(fnt.baseline));
#endif
  doDisplay(true, HDEVICE);
#elif defined(LCD160X)
  //lcd.begin(DISPLAY_COLUMNS, DISPLAY_LINES );
  lcd.init();
  lcd.display();
  lcd.backlight();
  lcd.print("Trumpy Bear");
#endif    
}

// try not to be TOO clever. 
// x and y are character/line positions NOT PIXELS. They start at 1 (one)
// except x == -1 means center 'string'.

void myterm(int x, int y, String ln) {
  //int len = ln.length();
#if defined(TFT_SPI)
  int wid = lcd.textWidth(ln);
  //Serial.print("x: "+String(x)+" y: "+String(y));
  if (y <= 0) y = 1;
  y = y - 1;
  //Serial.print(" len: "+String(ln.length())+" wid: "+String(wid)+" ");
  //Serial.println(ln);
  if (x < 0) {
    int xp = (320 - wid) / 2;
    if (xp < 0) xp = 0;  // ensure it is not negative
    lcd.setCursor(xp, (y * fontHgt));
    lcd.print(ln);
  } else {
    lcd.setCursor(1, (y * fontHgt));
    lcd.print(ln);
  }
#elif defined(DISPLAY_U8)
  // x and y in characters and lines -  SORT OF
  //Serial.println("x: "+String(x)+" y: "+String(y)+" "+ln);
  if (y <= 0) y = 1;
  y--;
  if (y >= display_lines) return;
  if (x < 0) {
    //int len = ln.length();
    //int xp = (display_columns - len) / 2;
    //if (xp < 0) xp = 0;
    //Serial.println("xp: "+String(xp));
    //u8x8.setCursor(xp, y*ln_step);
    //u8x8.print(ln);
    //getMaxCharWidth
    int xp = (device_width - u8g2.getUTF8Width(ln.c_str()))/2;
    u8g2.setCursor(xp, (y+1)*fontHgt);
    u8g2.print(ln);
  } else {
    x--;
    if (x < 0) x = 0;
    //u8x8.setCursor(x,y*ln_step);
    //u8x8.print(ln);
    u8g2.setCursor(x,(y+1)*fontHgt);
    u8g2.print(ln);
  }
#elif defined(LCD160X)
    int wid = strlen(ln);
    if (y < 0) y = 1;
    y = y - 1;
    if (x < 0) {
        int cp = (DISPLAY_COLUMNS - wid) / 2;
        if (cp < 0) cp = 0;
        lcd.setCursor(cp, y);
        lcd.print(ln);
    } else {
        lcd.setCursor(x, y);
        lcd.print(ln)
    }
#endif
}

/* called from Mqtt and (internally for communicating status)
 *  Will pack words to fit available lines.
 *  Always centered. New line in imput str is treated as space.
 * 
 * 
 * Version 2 allows json string {...} in arg 'strarg'
*/
typedef struct {
    short   beg;
    short   end;
} lnMark;
lnMark lnMarks[0];
char *wordLst[0];

void displayLines(int nwords,char *words[]) {
  // words is an array of char pointers
  if (nwords <= display_lines) {
    int xp;
    int y = baseline;
    for (int i=0; i < nwords; i++) {
      xp = (device_width - u8g2.getUTF8Width(words[i]))/2;
      u8g2.setCursor(xp, y);
      u8g2.print(words[i]);
      y += fontHgt;
    }
  } else {
    // already tricky, it's not getting better.
    // Fill a line until the new word plus a space doesn't fit. 
    // Flush that line (centered - because why not?)
    // Continue filling next line.
    char ln[display_columns+4];
    int llen = 0;
    int j = 0;
    int y = baseline;
    //char *p = ln;
    for (int i = 0; i < nwords; i++) {
      int wlen = u8g2.getUTF8Width(words[i]) + spaceWidth;
      if ((llen + wlen) > device_width) {
        // won't fit flush centered line
        int x = (device_width - u8g2.getUTF8Width(ln)) / 2;
        u8g2.setCursor(x, y);
        u8g2.print(ln);
        j = 0;
        ln[j] = '\0';
        y = y + fontHgt;
      } 
      if (j == 0) {
        // first word on line
        j = strlen(words[i]);
        strcpy(ln, words[i]);
        llen =  u8g2.getUTF8Width(ln);
      } else {
        // append to line
        ln[j++]=' ';
        strcpy(ln+j, words[i]);
        j = strlen(ln);
        llen =  u8g2.getUTF8Width(ln);
      }
    }
    // anything left in line? It's very likely
    if (j != 0) {
      int x = (device_width - u8g2.getUTF8Width(ln)) / 2; 
      u8g2.setCursor(x, y);
      u8g2.print(ln);

    }
  }

}
void start_screen() {
#if defined( DISPLAY_U8)
    //u8x8.clear();
    u8g2.clear();
    switch_font(chosen_font);
#elif defined(TFT_SPI)
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextFont(4);
    lcd.setCursor(0,0);
#elif defined(LCD_160X)
    lcd.display();
    lcd.backlight();
    lcd.clear();
#endif
}

void doDisplay(boolean state, int num) {
  char tmp[12];
  itoa(num, tmp, 10);
  doDisplay(state, tmp);
}
void doDisplay(boolean state, String strarg) {
  /* if str == Null then turn display on/off based on state 
   * if str != Null then turn on and clear display
   * we always clear the display. str arg may have \n inside
   * to separate lines.
   */
#ifndef DISPLAYG
#else
  if (strarg.c_str()[0]=='{') {
    displayV2(strarg);
  }
  if (state == false) {
#if defined(DISPLAY_U8)
    u8g2.firstPage();
    do { start_screen(); } while (u8g2.nextPage());
#else
    start_screen();
#endif
  } else {

    u8g2.firstPage();
    do {
      start_screen();
      const char *argstr = strarg.c_str();
      if (argstr == (char *)0) {
         Serial.println("display on without string");
        return;
      }
      // special mode handling finished. Screen is on and setup.
      // Separate input into words
      char *str;
      str = strdup(argstr);
      int len = strlen(str);
      int nwords = 0;
      boolean ldsp = true;
      boolean inwd = false;
      for (int i = 0; i < len; i++) {
        if (str[i] == '\n' || str[i] == ' ') {
          if (ldsp) continue; // skip leading sp
          ldsp = true;
          if (inwd) {
            nwords++;
            inwd = false;
          }
        } else {
          // not a space.
          if (ldsp) ldsp = false;
          inwd = true;
        }
      }
      // Now setup array of pointers to word (substrings - modifiable not in ROM)
      if (inwd) nwords += 1;
      //Serial.println("nwords: "+String(nwords));
      char *words[nwords];         // easily run out of stack space ?
      int j = 0;                   // current word number 
      words[0] = str;
      ldsp = true;
      inwd = false;
      for (int i = 0; i < len; i++) {
        if (str[i] == '\n' || str[i] == ' ') {
          if (ldsp) continue; // skip continus sp
          ldsp = true;
          if (inwd) {
            // finish word
            str[i] = '\0';    // null char to end word
            j++;
            inwd = false;
          }
        } else {
          // not a space.
          if (ldsp) {
            ldsp = false;
            words[j] = str+i;  // pointer to first char of word
          }
          inwd = true;
        }
      }
      
      //for (int i = 0; i < nwords; i++) Serial.println("word: "+String(words[i]));
#ifdef DISPLAY_U8
      displayLines(nwords,words);
#else
      if (nwords <= display_lines) {
        // all words will be centered in individual lines
        int startln = 1;  // 1 relative, remember? 
        if (display_lines > 2)
          if (nwords < (display_lines - 1)) startln += 1;  //heuristic is fancy word
        for (int i=0; i < nwords; i++) {
            myterm(-1, i+startln, words[i]);
        }
      } else {
        // We'll have to pack words. Try
        int ln = 1;
        char t[display_columns+2];
        int wid = 0;        // index into partial line, 't'

        for (int i = 0; i < nwords; i++){
          int len = strlen(words[i]);
          //Serial.println(String(words[i])+":w len "+String(len)+" wid: "+String(wid));
          if ((wid + len) >= display_columns) {
            // not enough room for current word
                // flush line
            t[wid++] = '\0';
            //Serial.println(String(t)+":ln flush");
            myterm(-1, ln, String(t));
            // start new line
            ln++;
            wid = 0;
          }            
          if (wid != 0) {
            // not first word, overwrites null?
            t[wid++] = ' ';
            t[wid] = '\0';
          }
          strcpy(t+wid, words[i]);
          wid = wid + len;
          //Serial.println(String(t)+":ln part");
        }
        if (wid != 0) {
          //Serial.println("flush leftovers");
          t[wid++] = '\0';
          myterm(-1, ln, t);  // left overs
        }
      }
#endif
      free(str);
    } while (u8g2.nextPage() );
  }
#endif // NDEF DISPLAYG
}  


void displayV2(String jsonstr) {
    Serial.print("JSON: ");
    Serial.println(jsonstr);
}
