## MM17D * RH/T measuring device  
Copyright (C) 2023 Pozsár Zsolt <pozsarzs@gmail.com>  
Homepage: <http://www.pozsarzs.hu>  
GitHub: <https://github.com/pozsarzs/mm17d-sw>

#### Hardware

ESP8266 Huzzah Breakout microcontroller

#### Software
|features              |                                             |
|:---------------------|---------------------------------------------|
|architecture          |xtensa                                       |
|operation system      |none                                         |
|version               |v0.1                                         |
|language              |en                                           |
|licence               |EUPL v1.2                                    |
|local user interface  |none                                         |
|remote user interface |RS-232 TTL: serial console                   |
|                      |WLAN: web interface                          |
|remote data access    |RS-232 TTL: Modbus/RTU                       |
|                      |WLAN: HTTP (CSV, JSON, TXT, XML), Modbus/TCP |

#### External libraries in package

 - [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) library v1.1.4 by Adafruit Industries
 - [DHT sensor](https://github.com/adafruit/DHT-sensor-library) library v1.4.4 by Adafruit Industries
 - [Modbus-ESP8266](https://github.com/emelianov/modbus-esp8266) library v4.1.0 by Andre Sarmento Barbosa, Alexander Emelianov
 - [ESP8266WebServer](https://github.com/esp8266/Arduino) library v1.0 by Ivan Grokhotkov
 - [ESP8266WiFi](https://github.com/esp8266/Arduino) library v1.0 by Ivan Grokhotkov
