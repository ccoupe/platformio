#Display V2

##Goals

### MQTT
### HE driver (outputs to MQTT/Display
### Python code to output to MQTT
### mosquitto_pub

## Devices read from MQTT
1.  16 char X 2 or 4 lines - LCD clunky
2.  Various OLED sizes & capabilities
3.  Crawl Scroll text if message is larger. Repeat

## API has to be something login.py can handle
    and push to Python-Tk. Also light enough
    for ESP32 and casual users.
    
### Json. 

  { "text": [ <frag>,... ],
    mode: <mode>
  }
  
<mode> 
  set of key value pairs:
  {"fg": <rgb>
  "bg": <rgb>
  "blank: <n>       // screens goes blank <n> seconds after last message.
  "scroll: <bool>
  }
  
<frag> 
  {"ft": <n>,
   "sz": <n>,
   "fg": <rgb>,
   "bg": <rgb>,
   "text": <chars>
  }
  All optional, but w/o text: nothing matters.
  
<rgb>
  "#rrggbb" hex digits, leading sharp sign required"
  "color"  a few color names are build in - BLACK,WHITE,RED,GREEN,BLUE

<font> and <size> 
  This is font number in the device (firmware) not a name. Size is a multiplier
  so 2 is double, 3 is triple. - Neither of these is guaranteed for any
  particular device.
