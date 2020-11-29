# mqtt-domo-io-arduino

This is an arduino sketch. Git-clone me in you arduino sketchbook.


Arduino I/O MQTT core sketches
------------------------------

Source code to multiple modules :

INPUT NODE
----------
- Reads inputs via chained 74HC165E circuits. Publish the events to an MQTT broker.


OUTPUT NODE
-----------
- Subscribes to specific topic on MQTT broker.  Apply values and variations to chained 74HC595 circuits.


CORE NODE
---------
- Subscribes to "input" MQTT topics and apply an internal logic table to set the outputs.

TRACE NODE
----------
- Subscribe to MQTT trace nodes and show on an SSD1306 display

