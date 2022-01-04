#Display and Ranger V2
NOTE: The json  also defines what the Hubitat Elevation Driver has to look like.
  and a good hint at the needed global variables. The power of Specifications!


##Goals
  Will Not Be Compatible with auto-ranger and simple ranger.

### MQTT
  homie/device/node/property/attribute
                    parameter/type
  when json
    homie/device/node/cmd/set
          
esp32 device(s) will subscribe to:
ranger:  homie/<device>/ranger/cmd/set

display: homie/<device>/display/cmd/set   //for json formated txt and cmds
         homied/<device>/display/text/set // just text

auto-ranger: ??

Decide syntax elements which are ranger and which are display?


### Hubitat Elevation drivers (outputs to MQTT/Display
Two - one for simple/formatted text display.
Another one for ranger stuff (unless this can do it all)

### Python code to output to MQTT

### mosquitto_pub - esp32 publishes to these:

homie/<device>ranger/distance   <integer-centimeters>
                    /range      "above"|"below"  
                    
-- nothing for display - 

## Devices read from MQTT
1.  16 char X 2 or 4 lines - LCD clunky
2.  Various OLED sizes & capabilities
3.  Crawl Scroll text if message is larger. Repeat

## API has to be something login.py can handle
    and push to Python-Tk. Also light enough
    for ESP32 and casual users.
    
## MQTT payloads
  For compatibility. Two topics :ranger/cmd/set and display/cmd/set
  Not sure I care about compatibility. 
  
### ranger json:

  {
    "settings": {<settings>}
    "cmd:" <cmd>
  }
  
<cmd> : 
  'off' turns off free running and guide modes
  'on' takes one measurement. will restart free /guide mode
      (if they were turned off)
   "update", "url:" <string>"

<settings> set of key value pairs:
  "mode": "guide"| "once" | "free"
  "every": <n>          // wake up sensor every n seconds, report
  "keep-alive" <n>      // wake up sensor every n seconds don't report
  "sample> <n>          // how often to sample. free running doesn't change this
  "range-low": <n>      // send above/below these limits.
  "range-high": <n>     // 
  "slopH": <n>
  "slopL": <n>
  "report-all": <bool>  // report if value outside range h/l but
  "average": n          // 1 means no averaging
  "bounds-low": <n>     //  within out of bounds range.
  "bounds-high": <n>
  "guide-low": <gd-str>   
  "guide-high": <gd-str>
  "guide-target: <gd-str>
  }



### display json

  { "text": [ <frag>,... ],
    "settings": {<settings>}
    "cmd:" <cmd>
  }
  
<cmd> : 
  "on" | "off"    // controls display on/off
  "update", "url:" <string>"
     
<settings> set of key value pairs:
  {"color": <rgb>       // screen color (background, default black or none.
  "blank-after": <n>    // screens goes blank <n> seconds after last message.
  "scroll: <bool>       // may not work with guide mode, default: false
  }


<gd-str> := "text chars" | "color name" | <frag>
  <frag> is json object {} (vs String) if String interpret:
  Standard color name are checked. If don't match, then its a text message
  If matches, the color is used for whole screen - kind of like a Button. 
  
<frag> 
  {"font": <n>,
   "size": <n>,
   "fg": <rgb>,
   "bg": <rgb>,
   "text": <chars>
  }
  All optional, but w/o text: nothing matters.
  
<rgb>
  "#rrggbb" hex digits, leading sharp sign required"
  "std-color"
    
<std-color> := BLACK|WHITE|RED|GREEN|BLUE|YELLOW. 

<font> and <size> 
  This is a font number in the device (firmware) not a name. Size is a multiplier
  so 2 is double, 3 is triple. - None of these is guaranteed for any
  particular device. The font will have to be compiled it. 
