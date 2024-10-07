Printer-auto-shutdown
=====================

This is a simple programm for ESP32 S2 that establishes a WiFi connection, periodically checks the Duet3D web interface of my 3D printer and sends the poweroff command to my tasmota smart plug when a print has finished and the heat bed is cooled down to < 50°C. The auto shutdown can be enabled/disabled via button press.

The network addresses of my printer and smart plug and pins are hard coded so you have to change it in the sketch.