// +---------------------------------------------------------------------------+
// | MM17D v0.1 * T/RH measuring device                                        |
// | Copyright (C) 2023 Pozsár Zsolt <pozsarzs@gmail.com>                      |
// | mm17d.ino                                                                 |
// | Program for Adafruit Huzzah Breakout                                      |
// +---------------------------------------------------------------------------+

//   This program is free software: you can redistribute it and/or modify it
// under the terms of the European Union Public License 1.2 version.
//
//   This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.

#include <DHT.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ModbusIP_ESP8266.h>
#include <ModbusRTU.h>

#define       TYP_SENSOR1 DHT11
// #define       PT100_SIMULATION

#ifdef PT100_SIMULATION
const float   TPT[3]            = {-30, 0, 50}; // T in degree Celsius
const float   RPT[3]            = {88.22, 100, 119.4}; // R in ohm
int count;
#endif

// settings
const char   *WIFI_SSID         = "";
const char   *WIFI_PASSWORD     = "";
// ports
const int     PRT_BUZZER        = 14;
const int     PRT_LEDBLUE       = 2;
const int     PRT_LEDGREEN      = 0;
const int     PRT_LEDRED        = 5;
const int     PRT_LEDYELLOW     = 4;
const int     PRT_SENSOR1       = 12;
const int     PRT_SENSOR2       = 0;
// general constants
const byte    SWMVERSION        = 0;
const byte    SWSVERSION        = 1;
const int     MAXADCVALUE       = 1024;
const int     SERIALSPEED       = 9600;
const int     MB_UID            = 1;
const long    INTERVAL          = 10000;
const String  TEXTHTML          = "text/html";
const String  TEXTPLAIN         = "text/plain";
// general variables
int           syslog[64]        = {};
int           values[3]         = {};
boolean       leds[3]           = {false, false, false};
String        line;
String        myipaddress;
String        mymacaddress;
String        swversion;
unsigned long prevtime          = 0;
// messages
const String MSG[39]            =
{
  /*  0 */  "",
  /*  1 */  "MM17D * T/RH measuring device",
  /*  2 */  "Copyright (C) 2023 Pozsar Zsolt",
  /*  3 */  "http://www.pozsarzs.hu/",
  /*  4 */  "MM17D",
  /*  5 */  "* Initializing GPIO ports...",
  /*  6 */  "* Initializing sensors...",
  /*  7 */  "* Connecting to wireless network",
  /*  8 */  "done",
  /*  9 */  "  my MAC address:         ",
  /* 10 */  "  my IP address:          ",
  /* 11 */  "  subnet mask:            ",
  /* 12 */  "  gateway IP address:     ",
  /* 13 */  "* Starting webserver...",
  /* 14 */  "* HTTP query received ",
  /* 15 */  "* Modbus/TCP query received ",
  /* 16 */  "* Modbus/RTU query received ",
  /* 17 */  "* E01: Failed to read T/RH sensor!",
  /* 18 */  "* E02: Failed to read PT100!",
  /* 19 */  "* E03: Error 404: page not found!",
  /* 20 */  "* E04:",
  /* 21 */  "* Attention! the serial console is off!",
  /* 22 */  "  internal humidity:\t",
  /* 23 */  "  internal temperature:\t",
  /* 24 */  "  external temperature:\t",
  /* 25 */  "* Green",
  /* 26 */  "* Red",
  /* 27 */  " LED is switched ",
  /* 28 */  "on.",
  /* 29 */  "off.",
  /* 30 */  "* Periodic measure:",
  /* 31 */  "  get summary page",
  /* 32 */  "  get help page",
  /* 33 */  "  get log page",
  /* 34 */  "  get all measured data",
  /* 35 */  "* Starting Modbus/TCP server...",
  /* 36 */  "* Starting Modbus/RTU slave...",
  /* 37 */  "  my Modbus UID:          ",
  /* 38 */  "  serial port parameters: "
};

DHT dht(PRT_SENSOR1, TYP_SENSOR1, 11);
ESP8266WebServer server(80);
ModbusIP mbtcp;
ModbusRTU mbrtu;

// switch on/off blue LED
void blueled(boolean b)
{
  digitalWrite(PRT_LEDBLUE, b);
}

// switch on/off green LED
void greenled(boolean b)
{
  digitalWrite(PRT_LEDGREEN, b);
  leds[0] = b;
  mbtcp.Ists(0, leds[0]);
  mbrtu.Ists(0, leds[0]);
}

// switch on/off yellow LED
void yellowled(boolean b)
{
  digitalWrite(PRT_LEDYELLOW, b);
  leds[1] = b;
  mbtcp.Ists(1, leds[1]);
  mbrtu.Ists(1, leds[1]);
}

// switch on/off red LED
void redled(boolean b)
{
  digitalWrite(PRT_LEDRED, b);
  leds[2] = b;
  mbtcp.Ists(2, leds[2]);
  mbrtu.Ists(2, leds[2]);
}

// blinking blue LED
void blinkblueled()
{
  blueled(true);
  delay(25);
  blueled(false);
}

// blinking yellow LED
void blinkyellowled()
{
  yellowled(true);
  delay(25);
  yellowled(false);
}

// blinking all LEDs
void knightrider()
{
  blueled(true);
  delay(75);
  blueled(false);
  greenled(true);
  delay(75);
  greenled(false);
  yellowled(true);
  delay(75);
  yellowled(false);
  redled(true);
  delay(75);
  redled(false);
  yellowled(true);
  delay(75);
  yellowled(false);
  greenled(true);
  delay(75);
  greenled(false);
}

// beep sign
void beep(int num)
{
  for (int i = 0; i < num; i++)
  {
    tone(PRT_BUZZER, 880);
    delay (100);
    noTone(PRT_BUZZER);
    delay (100);
  }
}

// initializing function
void setup(void)
{
  swversion = String(SWMVERSION) + "." + String(SWSVERSION);
  // set serial port
  Serial.begin(SERIALSPEED);
  // write program information
  Serial.println("");
  Serial.println("");
  Serial.println(MSG[1] + " * v" + swversion );
  Serial.println(MSG[2] +  " <" + MSG[3] + ">");
  // initialize GPIO ports
  writetosyslog(5);
  Serial.println(MSG[5]);
  pinMode(PRT_BUZZER, OUTPUT);
  pinMode(PRT_LEDBLUE, OUTPUT);
  pinMode(PRT_LEDGREEN, OUTPUT);
  pinMode(PRT_LEDRED, OUTPUT);
  pinMode(PRT_LEDYELLOW, OUTPUT);
  digitalWrite(PRT_LEDBLUE, LOW);
  digitalWrite(PRT_LEDGREEN, LOW);
  digitalWrite(PRT_LEDRED, LOW);
  digitalWrite(PRT_LEDYELLOW, LOW);
  // initialize sensors
  writetosyslog(6);
  Serial.println(MSG[6]);
  dht.begin();
  // connect to wireless network
  writetosyslog(7);
  Serial.print(MSG[7]);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    knightrider();
    Serial.print(".");
  }
  Serial.println(MSG[8]);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  myipaddress = WiFi.localIP().toString();
  mymacaddress = WiFi.macAddress();
  Serial.println(MSG[9] + mymacaddress);
  Serial.println(MSG[10] + myipaddress);
  Serial.println(MSG[11] + WiFi.subnetMask().toString());
  Serial.println(MSG[12] + WiFi.gatewayIP().toString());
  // start Modbus/TCP server
  writetosyslog(35);
  Serial.print(MSG[35]);
  mbtcp.server();
  Serial.println(MSG[8]);
  // start Modbus/RTU slave
  writetosyslog(36);
  Serial.print(MSG[36]);
  mbrtu.begin(&Serial);
  mbrtu.setBaudrate(SERIALSPEED);
  mbrtu.slave(MB_UID);
  Serial.println(MSG[8]);
  Serial.println(MSG[37] + String(MB_UID));
  Serial.println(MSG[38] + String(SERIALSPEED) + " bps, 8N1");
  // set Modbus registers
  for (int i = 0; i < 3; i++)
  {
    mbtcp.addIreg(i, values[i]);
    mbrtu.addIreg(i, values[i]);
    mbtcp.addIsts(i, leds[i]);
    mbrtu.addIsts(i, leds[i]);
  }
  mbtcp.addIreg(9998, SWMVERSION * 256 + SWSVERSION);
  mbrtu.addIreg(9998, SWMVERSION * 256 + SWSVERSION);
  // set Modbus callback
  mbtcp.onGetIreg(0, modbustcpquery, 3);
  mbtcp.onGetIreg(9998, modbustcpquery);
  mbrtu.onGetIreg(0, modbusrtuquery, 3);
  mbrtu.onGetIreg(9998, modbusrtuquery);
  // start webserver
  writetosyslog(13);
  Serial.print(MSG[13]);
  server.onNotFound(handleNotFound);
  // help page
  server.on("/", []()
  {
    writetosyslog(32);
    line =
      "<html>\n"
      "  <head>\n"
      "    <title>" + MSG[1] + " | Help page</title>\n"
      "  </head>\n"
      "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
      "    <h2>" + MSG[1] + "</h2>\n"
      "    <br>\n"
      "    " + MSG[9] + mymacaddress + "<br>\n"
      "    " + MSG[10] + myipaddress + "<br>\n"
      "    " + MSG[37] + String(MB_UID) + "<br>\n"
      "    " + MSG[38] + String(SERIALSPEED) + " bps, 8N1<br>\n"
      "    software version: v" + swversion + "<br>\n"
      "    <hr>\n"
      "    <h3>Information and data access</h3>\n"
      "    <table border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n"
      "      <tr><td colspan=\"3\" align=\"center\"><b>Information pages</b></td></tr>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/\">http://" + myipaddress + "/</a>"
      "        </td>\n"
      "        <td>Help</td>\n"
      "        <td>" + TEXTHTML + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/summary\">http://" + myipaddress + "/summary</a>"
      "        </td>\n"
      "        <td>Summary page</td>\n"
      "        <td>" + TEXTHTML + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/log\">http://" + myipaddress + "/log</a>"
      "        </td>\n"
      "        <td>Log page</td>\n"
      "        <td>" + TEXTHTML + "</td>\n"
      "      </tr>\n"
      "      <tr><td colspan=\"3\" align=\"center\"><b>Data access with HTTP</b></td>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/get/csv\">http://" + myipaddress + "/get/csv</a>"
      "        </td>\n"
      "        <td>Get all measured values in CSV format</td>\n"
      "        <td>" + TEXTPLAIN + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/get/json\">http://" + myipaddress + "/get/json</a>"
      "        </td>\n"
      "        <td>Get all measured values in JSON format</td>\n"
      "        <td>" + TEXTPLAIN + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/get/txt\">http://" + myipaddress + "/get/txt</a>"
      "        </td>\n"
      "        <td>Get all measured values in TXT format</td>\n"
      "        <td>" + TEXTPLAIN + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>\n"
      "          <a href=\"http://" + myipaddress + "/get/xml\">http://" + myipaddress + "/get/xml</a>"
      "        </td>\n"
      "        <td>Get all measured values in XML format</td>\n"
      "        <td>" + TEXTPLAIN + "</td>\n"
      "      </tr>\n"
      "      <tr><td colspan=\"3\" align=\"center\"><b>Data access with Modbus</b></td>\n"
      "      <tr>\n"
      "        <td>10001</td>\n"
      "        <td>Status of the green LED</td>\n"
      "        <td>bit</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>10002</td>\n"
      "        <td>Status of the yellow LED</td>\n"
      "        <td>bit</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>10003</td>\n"
      "        <td>Status of the red LED</td>\n"
      "        <td>bit</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>30001</td>\n"
      "        <td>Internal humidity in percent</td>\n"
      "        <td>integer</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>30002</td>\n"
      "        <td>Internal temperature in degree Celsius</td>\n"
      "        <td>integer</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>30003</td>\n"
      "        <td>External temperature in degree Celsius</td>\n"
      "        <td>integer</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>39999</td>\n"
      "        <td>Software version</td>\n"
      "        <td>two byte</td>\n"
      "      </tr>\n"
      "    </table>\n"
      "    <br>\n"
      "    <hr>\n"
      "    <center>" + MSG[2] + " <a href=\"" + MSG[3] + "\">" + MSG[3] + "</a><center>\n"
      "    <br>\n"
      "  </body>\n"
      "</html>\n";
    server.send(200, TEXTHTML, line);
    httpquery();
    delay(100);
  });
  // summary page
  server.on("/summary", []()
  {
    writetosyslog(31);
    line =
      "<html>\n"
      "  <head>\n"
      "    <title>" + MSG[1] + " | Summary page</title>\n"
      "  </head>\n"
      "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
      "    <h2>" + MSG[1] + "</h2>\n"
      "    <br>\n"
      "    " + MSG[9] + mymacaddress + "<br>\n"
      "    " + MSG[10] + myipaddress + "<br>\n"
      "    " + MSG[37] + String(MB_UID) + "<br>\n"
      "    " + MSG[38] + String(SERIALSPEED) + " bps, 8N1<br>\n"
      "    software version: v" + swversion + "<br>\n"
      "    <hr>\n"
      "    <h3>Measured values</h3>\n"
      "    <table border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n"
      "      <tr>\n"
      "        <td>Internal humidity</td>\n"
      "        <td align=\"right\">" + String(values[0]) + " %</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>Internal temperature</td>\n"
      "        <td align=\"right\">" + String(values[1]) + " &deg;C</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>External temperature</td>\n"
      "        <td align=\"right\">" + String(values[2]) + " &deg;C</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>Status of the green LED</td>\n"
      "        <td align=\"center\">" + String(leds[0]) + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>Status of the yellow LED</td>\n"
      "        <td align=\"center\">" + String(leds[1]) + "</td>\n"
      "      </tr>\n"
      "      <tr>\n"
      "        <td>Status of the red LED</td>\n"
      "        <td align=\"center\">" + String(leds[2]) + "</td>\n"
      "      </tr>\n"
      "    </table>\n"
      "    <br>\n"
      "    <hr>\n"
      "    <center>" + MSG[2] + " <a href=\"" + MSG[3] + "\">" + MSG[3] + "</a><center>\n"
      "    <br>\n"
      "  </body>\n"
      "</html>\n";
    server.send(200, TEXTHTML, line);
    httpquery();
    delay(100);
  });
  // log page
  server.on("/log", []()
  {
    writetosyslog(33);
    line =
      "<html>\n"
      "  <head>\n"
      "    <title>" + MSG[1] + " | System log</title>\n"
      "  </head>\n"
      "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
      "    <h2>" + MSG[1] + "</h2>\n"
      "    <br>\n"
      "    " + MSG[9] + mymacaddress + "<br>\n"
      "    " + MSG[10] + myipaddress + "<br>\n"
      "    " + MSG[37] + String(MB_UID) + "<br>\n"
      "    " + MSG[38] + String(SERIALSPEED) + " bps, 8N1<br>\n"
      "    software version: v" + swversion + "<br>\n"
      "    <hr>\n"
      "    <h3>Last 64 lines of system log:</h3>\n"
      "    <table border=\"0\" cellpadding=\"3\" cellspacing=\"0\">\n";
    for (int i = 0; i < 64; i++)
      if (syslog[i] > 0)
        line = line + "      <tr><td><pre>" + String(i) + "</pre></td><td><pre>" + MSG[syslog[i]] + "</pre></td></tr>\n";
    line = line +
           "    </table>\n"
           "    <br>\n"
           "    <hr>\n"
           "    <center>" + MSG[2] + " <a href=\"" + MSG[3] + "\">" + MSG[3] + "</a><center>\n"
           "    <br>\n"
           "  </body>\n"
           "</html>\n";
    server.send(200, TEXTHTML, line);
    httpquery();
    delay(100);
  });
  // get all measured data in CSV format
  server.on("/get/csv", []()
  {
    writetosyslog(34);
    line = "\"name\",\"" + MSG[4] + "\"\n"
           "\"version\",\"" + swversion + "\"\n"
           "\"rhint\",\"" + String(values[0]) + "\"\n"
           "\"tint\",\"" + String(values[1]) + "\"\n"
           "\"text\",\"" + String(values[2]) + "\"\n"
           "\"green\",\"" + String(leds[0]) + "\"\n"
           "\"yellow\",\"" + String(leds[1]) + "\"\n"
           "\"red\",\"" + String(leds[2]) + "\"";
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  // get all measured values in JSON format
  server.on("/get/json", []()
  {
    writetosyslog(34);
    line = "{\n"
           "  {\n"
           "    \"name\": \"" + MSG[4] + "\",\n"
           "    \"version\": \"" + swversion + "\"\n"
           "  },\n"
           "  {\n"
           "    \"rhint\": \"" + String(values[0]) + "\",\n"
           "    \"tint\": \"" + String(values[1]) + "\",\n"
           "    \"text\": \"" + String(values[2]) + "\"\n"
           "  },\n"
           "  {\n"
           "    \"green\": \"" + String(leds[0]) + "\",\n"
           "    \"yellow\": \"" + String(leds[1]) + "\",\n"
           "    \"red\": \"" + String(leds[2]) + "\"\n"
           "  }\n"
           "}";
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  // get all measured data in TXT format
  server.on("/get/txt", []()
  {
    writetosyslog(34);
    line = MSG[4] + "\n" +
           swversion + "\n" +
           String(values[0]) + "\n" +
           String(values[1]) + "\n" +
           String(values[2]) + "\n" +
           String(leds[0]) + "\n" +
           String(leds[1]) + "\n" +
           String(leds[2]);
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  // get all measured values in XML format
  server.on("/get/xml", []()
  {
    writetosyslog(34);
    line = "<xml>\n"
           "  <software>\n"
           "    <name>" + MSG[4] + "</name>\n"
           "    <version>" + swversion + "</version>\n"
           "  </software>\n"
           "  <value>\n"
           "    <rhint>" + String(values[0]) + "</rhint>\n"
           "    <tint>" + String(values[1]) + "</tint>\n"
           "    <text>" + String(values[2]) + "</text>\n"
           "  </value>\n"
           "  <led>\n"
           "    <green>" + String(values[0]) + "</green>\n"
           "    <yellow>" + String(values[1]) + "</yellow>\n"
           "    <red>" + String(values[2]) + "</red>\n"
           "  </led>\n"
           "</xml>";
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  server.begin();
  Serial.println(MSG[8]);
  Serial.println(MSG[21]);
  beep(1);
}

// error 404 page
void handleNotFound()
{
  httpquery();
  writetosyslog(19);
  server.send(404, TEXTPLAIN, MSG[19]);
}

// loop function
void loop(void)
{
  boolean measureerror;
  server.handleClient();
  unsigned long currtime = millis();
  if (currtime - prevtime >= INTERVAL)
  {
    prevtime = currtime;
    measureerror = measureinttemphum() && measureexttemp();
    greenled(measureerror);
    redled(! measureerror);
    blinkyellowled();
  }
  mbtcp.task();
  delay(10);
  mbrtu.task();
  yield();
}

// measure internal temperature and relative humidity
int measureinttemphum()
{
  float fh, ft;
  fh = dht.readHumidity();
  ft = dht.readTemperature(false);
  if (isnan(fh) || isnan(ft))
  {
    beep(1);
    writetosyslog(17);
    return 0;
  } else
  {
    values[0] = (int)fh;
    values[1] = (int)ft;
    mbtcp.Ireg(0, values[0]);
    mbtcp.Ireg(1, values[1]);
    mbrtu.Ireg(0, values[0]);
    mbrtu.Ireg(1, values[1]);
    return 1;
  }
}

// measure external temperature
boolean measureexttemp()
{
  float u1;
  float r2;
  float t;
  const float C0 = -245.19;
  const float C1 = 2.5293;
  const float C2 = -0.066046;
  const float C3 = 4.0422E-3;
  const float C4 = -2.0697E-6;
  const float C5 = -0.025422;
  const float C6 = 1.6883E-3;
  const float C7 = -1.3601E-6;
  const float R1 = 560; // ohm
  const float U0 = 5000; // mV

  u1 = analogRead(PRT_SENSOR2);

#ifdef PT100_SIMULATION
  u1 = U0 * (RPT[count] / (R1 + RPT[count]));
  Serial.println("Tpt100: " + String(int(TPT[count])) + " °C");
  Serial.println("Rpt100: " + String(RPT[count]) + " Ω");
  Serial.println("Uadc:   " + String(int(u1)) + " mV");
  if (count < 2)
  {
    count++;
  } else
  {
    count = 0;
  }
#endif

  if ((u1 == 0) || (u1 == MAXADCVALUE))
  {
    beep(1);
    writetosyslog(18);
    return false;
  } else
  {
    r2 = u1 / ((U0 - u1) / R1);
    t = (((((r2 * C4 + C3) * r2 + C2) * r2 + C1) * r2) / ((((r2 * C7 + C6) * r2 + C5) * r2) + 1)) + C0;

#ifdef PT100_SIMULATION
    Serial.println("Tcalc:  " + String(int(t)) + " °C\n");
#endif

    values[2] = (int)t;
    mbtcp.Ireg(2, values[2]);
    mbrtu.Ireg(2, values[2]);
    return true;
  }
}

// blink blue LED and write to log
void httpquery()
{
  blinkblueled();
  writetosyslog(14);
}

// blink blue LED and write to log
uint16_t modbustcpquery(TRegister* reg, uint16_t val)
{
  blinkblueled();
  writetosyslog(15);
  return val;
}

// blink blue LED and write to log
uint16_t modbusrtuquery(TRegister* reg, uint16_t val)
{
  blinkblueled();
  writetosyslog(16);
  return val;
}

// write a line to system log
void writetosyslog(int msgnum)
{
  if (syslog[63] == 0)
  {
    for (int i = 0; i < 64; i++)
    {
      if (syslog[i] == 0)
      {
        syslog[i] = msgnum;
        break;
      }
    }
  } else
  {
    for (int i = 1; i < 64; i++)
      syslog[i - 1] = syslog[i];
    syslog[63] = msgnum;
  }
}
