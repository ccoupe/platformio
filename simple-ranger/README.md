# Simple Ranger

The purpose of this device is to measure the distance to a 'object'using an HC-SRO4 ultrasonic
sensor. It communicates with MQTT with a psuedo Homie structure,
It listens on  homie/audi_stall/ranger/distance/set for json payload:
{"period": secs} That 16bit integer value is written into NVRAM and read from there on startup
The sensor is read every 'period' seconds, If it's changed then
The distance in cm is published to 'homie/<HDEVICE>/ranger/distance'
Each 4 hours (14400 secs we publish anyway.

You will need to modify lib/Device.h to set the values for your device.
1. WIFI_ID is your wifi network name and WIFI_PASSWORD is your authorization on
that network.
2. The defines that begin with MQTT define where you MQTT broker is and
What you name is at that broker. This has to be unique to that broker. This
is IMPORTANT.
3. Homie topic structure is complicated but fairly static. We need the HDEVICE string. 
This is the 'device', the top most user provided name in homie. It should be unique to
your broker. The HNAME string is for documentation purposes. The Contents might matter to
you but no one else. You have to have one though.
4. The echo_pin and trigger_pin will depend of your esp and how you wired it.
5. You define USE_FLASH or not if you want to write the target distance too
EEROM. I suggest you define it.

There are .stl files for a box and lid if you wish to 3D print one. There are
openscad files if you want to modify the box and generate the stl. 
