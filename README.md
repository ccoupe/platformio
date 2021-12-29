# Platform IO Projects 

These are Home Automation related efforts that use MQTT (and Hubitat Elevation)


Ranger - has several variations. They use an ultrasonic sensor to
measure distance to an object (in integer centimeters) They may
have a Display - an LCD to display messages.

The display is a separate MQTT device and be written to my MQTT messages.
It can also be written to by the range measuring code (auto-ranger does
that). Simple-ranger doesn't have a display. 

The display project is the latest and presumably better variation of ranger.
Features can be compiled in or ommitted. It is OTA capable - the update
is triggered by and mqtt message that specifies the url of the .bin file.

It has slightly better behaviour for longer messages. 

V2 - when available will have a much better API including font switching
and colors.

See platformio.ini - this is where the -D command line options are specified/
