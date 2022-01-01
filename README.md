# ESP32 Platform IO Projects 

These are Home Automation related efforts that use MQTT (and Hubitat Elevation)
They are setup for VS Code and PlatformIO and NOT for Arduino, although
they use Arduino libraries and frameworks. You can be convert them to Arduino
easily enough if you need to.

These all rely on wifi and mqtt, may be interrupt driven and use a timer
interrupt to measure things. 

## Device Types
There are two general classes of projects/devices: Motion Sensors and Range Detectors

### Motion Sensors
These Motion Sensors use a Passive Infrared Receiver (PIR). There are
several variations of PIR. The is also a 'dual' that adds ad Microwave
sensor.  They all share an ESP32 problem: The wifi controller/signal can
cause False Positives for the PIR. This is no bueno for a motion detector.
Some PIR (KeyStudio) are almost good enough. 

### Range measurement
I call these devices 'Ranger' there are many variations. They use an ultrasonic sensor to
measure distance to an object (in integer centimeters) They may
have a Display - an LCD to display messages. They could be modified
to use other kinds of sensors. The cheap SR504 is not very reliable.

Rangers have modes. A simple one just returns the distance when asked.
Another might send a stream on periodic measurements. Yet another could work
as a guide - comparing the stream to upper and lower bounds and notifing
when exceeded. Some 'Ranger's has small LCD displays which the ranger can
use for guiding. 

The display is a separate MQTT device and be written to my MQTT messages.
It can also be written to by the range measuring code (auto-ranger does
that). Simple-ranger doesn't have a display. 


### Display
The display project is the latest and presumably better variation of ranger.
Features can be compiled in or ommitted. It is OTA capable - the update
is triggered by and mqtt message that specifies the url of the .bin file.

Display doesn't have to habe a ranger. I can just be a message display.

It has slightly better behaviour for longer messages. 

V2 - when available will have a much better API including font switching
and colors.

See platformio.ini - this is where the -D command line options are specified

### MQTT

For better or worse, this code attempts to use the Homie MQTT structure and
conventions. That is NOT the same as Homey. It is not the same as HubAssistant
(HA). There isn't a well defined standard. Well defined being the operative part.

Using MQTT provides flexibility for device communications but the topic
structure may be confusing to newcomers.

Hubitat Elevation doesn't define a standard for MQTT either. 

### Settings and EEPROM

Many settings like the ip address of the MQTT server can be written to the EEPROM
so that it can be read at device power up. This allows the code to be tailored
to multiple physical devices at the cost of additional up front setup. 

### OTA
OTA is short for Over The Air updates. OR downloading firmware, flashing
it to the device it's running on and then rebooting into the new firmware.
It's a nice feature to have if you device is not on your desk or close to
hand. It does require more infrastructure like a web server and I use MQTT
to start it.

## C++
After looking at the code you probably think this code is more like simple
C and not C++ and not all that "conventional". That is correct. I might use
more C++ string handling in the future. If don't have something else to do.
