// Display Layout Version 2
#include <Arduino.h> 
#include "Device.h"
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
#include <U8x8lib.h>
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
#elif defined(LCD_160X)
#include <LiquidCrystal_I2C.h> 
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

void myterm(int x, int y, String ln);
int fontHgt;

#ifdef DISPLAY_U8
const uint8_t colLow = 4;
const uint8_t colHigh = 13;
const uint8_t rowCups = 0;
const uint8_t rowState = 2; // Double spacing the Rows
const uint8_t rowTemp = 4; // Double spacing the Rows
const uint8_t rowTime = 6; // Double spacing the Rows

void lcdBold(bool aVal) {
  if (aVal) {
    u8x8.setFont(u8x8_font_victoriabold8_r); // BOLD
  } else {
    u8x8.setFont(u8x8_font_victoriamedium8_r); // NORMAL
  }
}
#endif


void display_init() {
#if defined(DISPLAY_U8)
  u8x8.begin();
  lcdBold(true); // You MUST make this call here to set a Font
  // u8x8.setPowerSave(0); // Too lazy to find out if I need this
  u8x8.clear();
  u8x8.setCursor(0,rowCups);
  u8x8.print(HDEVICE);
  u8x8.setCursor(0,rowState);
  u8x8.print(__DATE__);
  u8x8.setCursor(0,rowTemp);
  u8x8.print(__TIME__);
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
  //Serial.print(" len: "+String(len)+" wid: "+String(wid)+" ");
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
  // x and y in half characters -  SORT OF
  //Serial.println("x: "+String(x)+" y: "+String(y)+" "+ln);
  if (y <= 0) y = 1;
  y--;
  if (x < 0) {
    int len = ln.length();
    int xp = (DISPLAY_COLUMNS - len) / 2;
    if (xp < 0) xp = 0;
    //Serial.println("xp: "+String(xp));
    u8x8.setCursor(xp*2, y*4);
    u8x8.print(ln);
  } else {
    x--;
    if (x < 0) x = 0;
    u8x8.setCursor(x,y*4);
    u8x8.print(ln);
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

//   u8x8.setFont(u8x8_font_profont29_2x3_r); // only 8 chars across
//   u8x8.setFont(u8x8_font_inr21_2x4_f); // only 8 chars across

void start_screen() {
#if defined( DISPLAY_U8)
    u8x8.clear();
    u8x8.setFont(u8x8_font_profont29_2x3_r); // 8 chars across. 2 Lines
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
void doDisplay(boolean state, String strarg) {
  /* if str == Null then turn display on/off based on state 
   * if str != Null then turn on and clear display
   * we always clear the display. str arg may have \n inside
   * to separate lines.
   */
  if (strarg.c_str()[0]=='{') {
    displayV2(strarg);
  }
  if (state == false) {
    start_screen();
  } else {
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
    // count them
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] == '\n' || str[i] == ' ') {
            j++;
        }
    }
    // Now setup array of pointers to word (substrings - modifiable not ROM)
    int nwords = j+1;
    //Serial.println("nwords: "+String(nwords));
    char *words[nwords];         // easily run out of stack space ?
    j = 0;
    words[0] = str;
    for (int i = 0; i < len; i++) {
        if (str[i] == '\n' || str[i] == ' ') {
            j++;
            words[j] = str+i+1;     // ptr arith
            str[i] = '\0';
      }
    }

    if (nwords <= DISPLAY_LINES) {
        // all words will be centered in individual lines
        int startln = 1;  // 1 relative, remember? 
#if (DISPLAY_LINES > 2)
        if (nwords < (DISPLAY_LINES - 1)) startln += 1;  //heuristic is fancy word
#endif
        for (int i=0; i < nwords; i++) {
            myterm(-1, i+startln, words[i]);
        }
    } else {
        // We'll have to pack words. Assume all characters are equal width
        int ln = 1;
        char t[DISPLAY_COLUMNS+2];
        int wid = 0;        // index into partial line, 't'

        for (int i = 0; i < nwords; i++){
            int len = strlen(words[i]);
            //Serial.println(String(words[i])+":w len "+String(len)+" wid: "+String(wid));
            if ((wid + len) >= DISPLAY_COLUMNS) {
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
  free(str);
  }
}  

int fill_line(int lnum, char **words, int wdlens[], int wdst, int maxw, int wid) {
  int len = 0;
  char ln[wid + 1] = {0};
  int pos = 0;
  for (int i = wdst; i < maxw; i++) {
    if ((len + wdlens[i]) <= wid) {
      strcat(ln, words[i]);
      strcat(ln, " ");
      len += (wdlens[i] + 1);
    } else {
      // would overflow, center, print and return
      if (len <= wid) 
         pos = (wid - len) / 2;
#if defined(DISPLAY_U8)
       u8x8.setCursor(pos*2, lnum*4);
       u8x8.print(ln);
#elif defined(TFT_SPI)
      myterm(-1, lnum, ln);
#elif defined(LCD160X)
       lcd.setCursor(pos, lnum);
       lcd.print(ln);
#endif
      return i;
    }
  }
  // if we get here, there is a partial line to print.
  if (len <= wid) 
     pos = (wid - len) / 2;
#if defined(DISPLAY_U8)
  u8x8.setCursor(pos*2,lnum*4);
  u8x8.print(ln);
#elif defined(TFT_SPI)
  lcd.setCursor(-1,lnum*4);
  lcd.print(ln);
#elif defined(LCD_160X)
  lcd.setCursor(pos, lnum);
  lcd.print(ln);
#endif
  return maxw;
}

void displayV2(String jsonstr) {
    Serial.print("JSON: ");
    Serial.println(jsonstr);
}
