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
#include <StringSplitter.h> // Note: Change MAX = 5 to MAX = 6 in StringSplitter.h.

#define       TYP_SENSOR1 DHT11
//#define       PT100_SIMULATION

#ifdef PT100_SIMULATION
const float   TPT[3]            = { -30, 0, 50};       // T in degree Celsius
const float   RPT[3]            = {88.22, 100, 119.4}; // R in ohm
int count;
#endif

// settings
const int     COM_SPEED         = 9600;
const int     MB_UID            = 1;
const char   *WIFI_SSID         = "";
const char   *WIFI_PASSWORD     = "";

// ports
const int     PRT_AI_SENSOR2    = 0;
const int     PRT_DI_SENSOR1    = 12;
const int     PRT_DO_BUZZER     = 14;
const int     PRT_DO_LEDBLUE    = 2;
const int     PRT_DO_LEDGREEN   = 0;
const int     PRT_DO_LEDRED     = 5;
const int     PRT_DO_LEDYELLOW  = 4;

// name of the Modbus registers
const String  DI_NAME[3]        = {"ledg", "ledy", "ledr"};
const String  IR_NAME[3]        = {"rhint", "tint", "text"};
const String  HR_NAME[6]        = {"name", "version", "mac_address", "ip_address", "modbus_uid", "com_speed"};

// other constants
const int     MAXADCVALUE       = 1024;
const long    INTERVAL          = 10000;
const String  SWNAME            = "MM17D";
const String  SWVERSION         = "0.1.0";
const String  TEXTHTML          = "text/html";
const String  TEXTPLAIN         = "text/plain";
const String  DOCTYPEHTML       = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">";

// Modbus registers
boolean       di_values[3]      = {};
int           ir_values[3]      = {};
int           hr_values[28]     = {};

// other variables
int           syslog[64]        = {};
String        line;
String        myipaddress;
String        mymacaddress;
unsigned long prevtime          = 0;

// messages
const String MSG[29]            =
{
  /*  0 */  "",
  /*  1 */  "MM17D * T/RH measuring device",
  /*  2 */  "Copyright (C) 2023 Pozsar Zsolt",
  /*  3 */  "  software version:       ",
  /*  4 */  "Starting device...",
  /*  5 */  "* Initializing GPIO ports",
  /*  6 */  "* Initializing sensors",
  /*  7 */  "* Connecting to wireless network",
  /*  8 */  "done",
  /*  9 */  "  my MAC address:         ",
  /* 10 */  "  my IP address:          ",
  /* 11 */  "  subnet mask:            ",
  /* 12 */  "  gateway IP address:     ",
  /* 13 */  "* Starting Modbus/TCP server",
  /* 14 */  "* Starting Modbus/RTU slave",
  /* 15 */  "  my Modbus UID:          ",
  /* 16 */  "  serial port speed:      ",
  /* 17 */  "* Starting webserver",
  /* 18 */  "* Ready, the serial console is off.",
  /* 19 */  "* Modbus query received ",
  /* 20 */  "* HTTP query received ",
  /* 21 */  "  get help page",
  /* 22 */  "  get summary page",
  /* 23 */  "  get log page",
  /* 24 */  "  get all data",
  /* 25 */  "* E01: Failed to read T/RH sensor!",
  /* 26 */  "* E02: Failed to read PT100!",
  /* 27 */  "* E03: No such page!",
  /* 28 */  "http://www.pozsarzs.hu"
};
const String DI_DESC[3]         =
{
  /*  0 */  "status of the green LED",
  /*  1 */  "status of the yellow LED",
  /*  2 */  "status of the red LED"
};
const String  IR_DESC[3]        =
{
  /*  0 */  "internal relative humidity in %",
  /*  1 */  "internal temperature in &deg;C",
  /*  2 */  "external temperature in &deg;C"
};

DHT dht(PRT_DI_SENSOR1, TYP_SENSOR1, 11);
ESP8266WebServer httpserver(80);
ModbusIP mbtcp;
ModbusRTU mbrtu;

// --- SYSTEM LOG ---
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

// --- STATIC MODBUS REGISTERS ---
// convert hex string to byte
byte hstol(String recv) {
  return strtol(recv.c_str(), NULL, 16);
}

// fill holding registers with configuration data
void fillholdingregisters()
{
  int    itemCount;
  String s;
  // name
  s = SWNAME;
  while (s.length() < 8)
    s = char(0x00) + s;
  for (int i = 0; i < 9; i++)
    hr_values[i] = char(s[i]);
  // version
  StringSplitter *splitter1 = new StringSplitter(SWVERSION, '.', 3);
  itemCount = splitter1->getItemCount();
  for (int i = 0; i < itemCount; i++)
  {
    String item = splitter1->getItemAtIndex(i);
    hr_values[8 + i] = item.toInt();
  }
  delete splitter1;
  // MAC-address
  StringSplitter *splitter2 = new StringSplitter(mymacaddress, ':', 6);
  itemCount = splitter2->getItemCount();
  for (int i = 0; i < itemCount; i++)
  {
    String item = splitter2->getItemAtIndex(i);
    hr_values[11 + i] = hstol(item);
  }
  delete splitter2;
  // IP-address
  StringSplitter *splitter3 = new StringSplitter(myipaddress, '.', 4);
  itemCount = splitter3->getItemCount();
  for (int i = 0; i < itemCount; i++)
  {
    String item = splitter3->getItemAtIndex(i);
    hr_values[17 + i] = item.toInt();
  }
  delete splitter3;
  // MB UID
  hr_values[21] = MB_UID;
  // serial speed
  s = String(COM_SPEED);
  while (s.length() < 6)
    s = char(0x00) + s;
  for (int i = 0; i < 6; i++)
    hr_values[22 + i] = char(s[i]);
  for (int i = 0; i < 28; i++)
  {
    mbtcp.Hreg(i, hr_values[i]);
    mbrtu.Hreg(i, hr_values[i]);
  }
}

// --- LEDS AND BUZZER ---
// switch on/off blue LED
void blueled(boolean b)
{
  digitalWrite(PRT_DO_LEDBLUE, b);
}

// switch on/off green LED
void greenled(boolean b)
{
  di_values[0] = b;
  mbtcp.Ists(0, di_values[0]);
  mbrtu.Ists(0, di_values[0]);
  digitalWrite(PRT_DO_LEDGREEN, di_values[0]);
}

// switch on/off yellow LED
void yellowled(boolean b)
{
  di_values[1] = b;
  mbtcp.Ists(1, di_values[1]);
  mbrtu.Ists(1, di_values[1]);
  digitalWrite(PRT_DO_LEDYELLOW, di_values[1]);
}

// switch on/off red LED
void redled(boolean b)
{
  di_values[2] = b;
  mbtcp.Ists(2, di_values[2]);
  mbrtu.Ists(2, di_values[2]);
  digitalWrite(PRT_DO_LEDRED, di_values[2]);
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
    tone(PRT_DO_BUZZER, 880);
    delay (100);
    noTone(PRT_DO_BUZZER);
    delay (100);
  }
}

// --- MEASURING ---
// measure internal temperature and relative humidity
int measureinttemphum()
{
  float fh, ft;
  fh = dht.readHumidity();
  ft = dht.readTemperature(false);
  if (isnan(fh) || isnan(ft))
  {
    beep(1);
    writetosyslog(25);
    return 0;
  } else
  {
    ir_values[0] = (int)fh;
    ir_values[1] = (int)ft;
    mbtcp.Ireg(0, ir_values[0]);
    mbtcp.Ireg(1, ir_values[1]);
    mbrtu.Ireg(0, ir_values[0]);
    mbrtu.Ireg(1, ir_values[1]);
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

  u1 = analogRead(PRT_AI_SENSOR2);
#ifdef PT100_SIMULATION
  u1 = U0 * (RPT[count] / (R1 + RPT[count]));
  Serial.println("Tpt100: " + String(int(TPT[count])) + " °C");
  Serial.println("Rpt100: " + String(RPT[count]) + " Ω");
  Serial.println("Uadc:   " + String(int(u1)) + " mV");
  if (count < 2) count++ else count = 0;
#endif
  if ((u1 == 0) || (u1 == MAXADCVALUE))
  {
    beep(1);
    writetosyslog(26);
    return false;
  } else
  {
    r2 = u1 / ((U0 - u1) / R1);
    t = (((((r2 * C4 + C3) * r2 + C2) * r2 + C1) * r2) / ((((r2 * C7 + C6) * r2 + C5) * r2) + 1)) + C0;
#ifdef PT100_SIMULATION
    Serial.println("Tcalc:  " + String(int(t)) + " °C\n");
#endif
    ir_values[2] = (int)t;
    mbtcp.Ireg(2, ir_values[2]);
    mbrtu.Ireg(2, ir_values[2]);
    return true;
  }
}

// --- DATA RETRIEVING ---
// blink blue LED and write to log
uint16_t modbusquery(TRegister* reg, uint16_t val)
{
  blinkblueled();
  writetosyslog(19);
  return val;
}

// blink blue LED and write to log
void httpquery()
{
  blinkblueled();
  writetosyslog(20);
}

// --- WEBPAGES ---
// error 404 page
void handleNotFound()
{
  writetosyslog(27);
  line = DOCTYPEHTML +
         "<html>\n"
         "  <head>\n"
         "    <title>" + MSG[1] + " | Error 404</title>\n"
         "  </head>\n"
         "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
         "    <h2>" + MSG[1] + "</h2>\n"
         "    <br>\n"
         "    " + MSG[9] + mymacaddress + "<br>\n"
         "    " + MSG[10] + myipaddress + "<br>\n"
         "    " + MSG[3] + "v" + SWVERSION + "<br>\n"
         "    " + MSG[15] + String(MB_UID) + "<br>\n"
         "    " + MSG[16] + String(COM_SPEED) + "<br>\n"
         "    <hr>\n"
         "    <h3>ERROR 404!</h3>\n"
         "    No such page!\n"
         "    <br>"
         "    <div align=\"right\"><a href=\"/\">back</a></div>"
         "    <br>\n"
         "    <hr>\n"
         "    <center>" + MSG[2] + " <a href=\"" + MSG[28] + "\">" + MSG[28] + "</a></center>\n"
         "    <br>\n"
         "  </body>\n"
         "</html>\n";
  httpserver.send(404, TEXTHTML, line);
  httpquery();
  delay(100);
}

// help page
void handleHelp()
{
  writetosyslog(21);
  line = DOCTYPEHTML +
         "<html>\n"
         "  <head>\n"
         "    <title>" + MSG[1] + " | Help</title>\n"
         "  </head>\n"
         "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
         "    <h2>" + MSG[1] + "</h2>\n"
         "    <br>\n"
         "    " + MSG[9] + mymacaddress + "<br>\n"
         "    " + MSG[10] + myipaddress + "<br>\n"
         "    " + MSG[3] + "v" + SWVERSION + "<br>\n"
         "    " + MSG[15] + String(MB_UID) + "<br>\n"
         "    " + MSG[16] + String(COM_SPEED) + "<br>\n"
         "    <hr>\n"
         "    <h3>Information and data access</h3>\n"
         "    <table border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n"
         "      <tr><td colspan=\"3\" align=\"center\"><b>Information pages</b></td></tr>\n"
         "      <tr>\n"
         "        <td><a href=\"http://" + myipaddress + "/\">http://" + myipaddress + "/</a></td>\n"
         "        <td>help</td>\n"
         "        <td>" + TEXTHTML + "</td>\n"
         "      </tr>\n"
         "      <tr>\n"
         "        <td><a href=\"http://" + myipaddress + "/summary\">http://" + myipaddress + "/summary</a></td>\n"
         "        <td>summary page</td>\n"
         "        <td>" + TEXTHTML + "</td>\n"
         "      </tr>\n"
         "      <tr>\n"
         "        <td><a href=\"http://" + myipaddress + "/log\">http://" + myipaddress + "/log</a></td>\n"
         "        <td>log</td>\n"
         "        <td>" + TEXTHTML + "</td>\n"
         "      </tr>\n"
         "      <tr><td colspan=\"3\" align=\"center\"><b>Data access with HTTP</b></td>\n"
         "      <tr>\n"
         "        <td>\n"
         "          <a href=\"http://" + myipaddress + "/get/csv\">http://" + myipaddress + "/get/csv</a>"
         "        </td>\n"
         "        <td>all measured values and status in CSV format</td>\n"
         "        <td>" + TEXTPLAIN + "</td>\n"
         "      </tr>\n"
         "      <tr>\n"
         "        <td><a href=\"http://" + myipaddress + "/get/json\">http://" + myipaddress + "/get/json</a></td>\n"
         "        <td>all measured values and status in JSON format</td>\n"
         "        <td>" + TEXTPLAIN + "</td>\n"
         "      </tr>\n"
         "      <tr>\n"
         "        <td><a href=\"http://" + myipaddress + "/get/txt\">http://" + myipaddress + "/get/txt</a></td>\n"
         "        <td>all measured values and status in TXT format</td>\n"
         "        <td>" + TEXTPLAIN + "</td>\n"
         "      </tr>\n"
         "      <tr>\n"
         "        <td><a href=\"http://" + myipaddress + "/get/xml\">http://" + myipaddress + "/get/xml</a></td>\n"
         "        <td>all measured values and status in XML format</td>\n"
         "        <td>" + TEXTPLAIN + "</td>\n"
         "      </tr>\n"
         "      <tr><td colspan=\"3\" align=\"center\"><b>Data access with Modbus</b></td>\n";
  for (int i = 0; i < 3; i++)
  {
    line +=
      "      <tr>\n"
      "        <td>" + String(i + 10001) + "</td>\n"
      "        <td>" + DI_DESC[i] + "</td>\n"
      "        <td>bit</td>\n"
      "      </tr>\n";
  }
  for (int i = 0; i < 3; i++)
  {
    line +=
      "      <tr>\n"
      "        <td>" + String(i + 30001) + "</td>\n"
      "        <td>" + IR_DESC[i] + "</td>\n"
      "        <td>integer</td>\n"
      "      </tr>\n";
  }
  line +=
    "      <tr>\n"
    "        <td>40001-40008</td>\n"
    "        <td>device name</td>\n"
    "        <td>8 ASCII coded char</td>\n"
    "      </tr>\n"
    "      <tr>\n"
    "        <td>40009-40011</td>\n"
    "        <td>software version</td>\n"
    "        <td>3 byte</td>\n"
    "      </tr>\n"
    "      <tr>\n"
    "        <td>40012-40017</td>\n"
    "        <td>MAC address</td>\n"
    "        <td>6 byte</td>\n"
    "      </tr>\n"
    "      <tr>\n"
    "        <td>40018-40021</td>\n"
    "        <td>IP address</td>\n"
    "        <td>4 byte</td>\n"
    "      </tr>\n"
    "      <tr>\n"
    "        <td>40022</td>\n"
    "        <td>Modbus UID</td>\n"
    "        <td>1 byte</td>\n"
    "      </tr>\n"
    "      <tr>\n"
    "        <td>40023-40028</td>\n"
    "        <td>serial port speed</td>\n"
    "        <td>6 ASCII coded char</td>\n"
    "      </tr>\n"
    "    </table>\n"
    "    <br>"
    "    <hr>\n"
    "    <center>" + MSG[2] + " <a href=\"" + MSG[28] + "\">" + MSG[28] + "</a></center>\n"
    "    <br>\n"
    "  </body>\n"
    "</html>\n";
  httpserver.send(200, TEXTHTML, line);
  httpquery();
  delay(100);
};

// summary page
void handleSummary()
{
  writetosyslog(22);
  line = DOCTYPEHTML +
         "<html>\n"
         "  <head>\n"
         "    <title>" + MSG[1] + " | Summary</title>\n"
         "  </head>\n"
         "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
         "    <h2>" + MSG[1] + "</h2>\n"
         "    <br>\n"
         "    " + MSG[9] + mymacaddress + "<br>\n"
         "    " + MSG[10] + myipaddress + "<br>\n"
         "    " + MSG[3] + "v" + SWVERSION + "<br>\n"
         "    " + MSG[15] + String(MB_UID) + "<br>\n"
         "    " + MSG[16] + String(COM_SPEED) + "<br>\n"
         "    <hr>\n"
         "    <h3>All measured values and status</h3>\n"
         "    <table border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n";
  for (int i = 0; i < 3; i++)
  {
    line +=
      "      <tr>\n"
      "        <td>" + IR_DESC[i] + "</td>\n"
      "        <td align=\"right\">" + String(ir_values[i]) + "</td>\n"
      "      </tr>\n";
  }
  for (int i = 0; i < 3; i++)
  {
    line +=
      "      <tr>\n"
      "        <td>" + DI_DESC[i] + "</td>\n"
      "        <td align=\"right\">" + String(di_values[i]) + "</td>\n"
      "      </tr>\n";
  }
  line +=
    "    </table>\n"
    "    <br>"
    "    <hr>\n"
    "    <div align=\"right\"><a href=\"/\">back</a></div>"
    "    <br>\n"
    "    <center>" + MSG[2] + " <a href=\"" + MSG[28] + "\">" + MSG[28] + "</a></center>\n"
    "    <br>\n"
    "  </body>\n"
    "</html>\n";
  httpserver.send(200, TEXTHTML, line);
  httpquery();
  delay(100);
}

// log page
void handleLog()
{
  writetosyslog(23);
  line = DOCTYPEHTML +
         "<html>\n"
         "  <head>\n"
         "    <title>" + MSG[1] + " | Log</title>\n"
         "  </head>\n"
         "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
         "    <h2>" + MSG[1] + "</h2>\n"
         "    <br>\n"
         "    " + MSG[9] + mymacaddress + "<br>\n"
         "    " + MSG[10] + myipaddress + "<br>\n"
         "    " + MSG[3] + "v" + SWVERSION + "<br>\n"
         "    " + MSG[15] + String(MB_UID) + "<br>\n"
         "    " + MSG[16] + String(COM_SPEED) + "<br>\n"
         "    <hr>\n"
         "    <h3>Last 64 lines of system log:</h3>\n"
         "    <table border=\"0\" cellpadding=\"3\" cellspacing=\"0\">\n";
  for (int i = 0; i < 64; i++)
    if (syslog[i] > 0)
      line += "      <tr><td align=right><b>" + String(i) + "</b></td><td>" + MSG[syslog[i]] + "</td></tr>\n";
  line +=
    "    </table>\n"
    "    <br>"
    "    <hr>\n"
    "    <div align=\"right\"><a href=\"/\">back</a></div>"
    "    <br>\n"
    "    <center>" + MSG[2] + " <a href=\"" + MSG[28] + "\">" + MSG[28] + "</a></center>\n"
    "    <br>\n"
    "  </body>\n"
    "</html>\n";
  httpserver.send(200, TEXTHTML, line);
  httpquery();
  delay(100);
}

// get all measured data in CSV format
void handleGetCSV()
{
  writetosyslog(24);
  line = "\"" + HR_NAME[0] + "\",\"" + SWNAME + "\"\n"
         "\"" + HR_NAME[1] + "\",\"" + SWVERSION + "\"\n"
         "\"" + HR_NAME[2] + "\",\"" + mymacaddress + "\"\n"
         "\"" + HR_NAME[3] + "\",\"" + myipaddress + "\"\n"
         "\"" + HR_NAME[4] + "\",\"" + String(MB_UID) + "\"\n"
         "\"" + HR_NAME[5] + "\",\"" + String(COM_SPEED) + "\"\n";
  for (int i = 0; i < 3; i++)
    line += "\"" + IR_NAME[i] + "\",\"" + String(ir_values[i]) + "\"\n";
  for (int i = 0; i < 3; i++)
    line += "\"" + DI_NAME[i] + "\",\"" + String(di_values[i]) + "\"\n";
  httpserver.send(200, TEXTPLAIN, line);
  httpquery();
  delay(100);
};

// get all measured values in JSON format
void handleGetJSON()
{
  writetosyslog(24);
  line = "{\n"
         "  \"software\": {\n"
         "    \"" + HR_NAME[0] + "\": \"" + SWNAME + "\",\n"
         "    \"" + HR_NAME[1] + "\": \"" + SWVERSION + "\"\n"
         "  },\n"
         "  \"hardware\": {\n"
         "    \"" + HR_NAME[2] + "\": \"" + mymacaddress + "\",\n"
         "    \"" + HR_NAME[3] + "\": \"" + myipaddress + "\",\n"
         "    \"" + HR_NAME[4] + "\": \"" + String(MB_UID) + "\",\n"
         "    \"" + HR_NAME[5] + "\": \"" + String(COM_SPEED) + "\"\n"
         "  },\n"
         "  \"integer\": {\n";
  for (int i = 0; i < 3; i++)
  {
    line += "    \"" + IR_NAME[i] + "\": \"" + String(ir_values[i]);
    if (i < 2 ) line += "\",\n"; else  line += "\"\n";
  }
  line +=
    "  },\n"
    "  \"bit\": {\n";
  for (int i = 0; i < 3; i++)
  {
    line += "    \"" + DI_NAME[i] + "\": \"" + String(di_values[i]);
    if (i < 2 ) line += "\",\n"; else  line += "\"\n";
  }
  line +=
    "  }\n"
    "}\n";
  httpserver.send(200, TEXTPLAIN, line);
  httpquery();
  delay(100);
};

// get all measured data in TXT format
void handleGetTXT()
{
  writetosyslog(24);
  line = SWNAME + "\n" +
         SWVERSION + "\n" +
         mymacaddress + "\n" +
         myipaddress + "\n" + \
         String(MB_UID) + "\n" + \
         String(COM_SPEED) + "\n";
  for (int i = 0; i < 3; i++)
    line += String(ir_values[i]) + "\n";
  for (int i = 0; i < 3; i++)
    line += String(di_values[i]) + "\n";
  httpserver.send(200, TEXTPLAIN, line);
  httpquery();
  delay(100);
};

// get all measured values in XML format
void handleGetXML()
{
  writetosyslog(24);
  line = "<xml>\n"
         "  <software>\n"
         "    <" + HR_NAME[0] + ">" + SWNAME + "</" + HR_NAME[0] + ">\n"
         "    <" + HR_NAME[1] + ">" + SWVERSION + "</" + HR_NAME[1] + ">\n"
         "  </software>\n"
         "  <hardware>\n"
         "    <" + HR_NAME[2] + ">" + mymacaddress + "</" + HR_NAME[2] + ">\n"
         "    <" + HR_NAME[3] + ">" + myipaddress + "</" + HR_NAME[3] + ">\n"
         "    <" + HR_NAME[4] + ">" + String(MB_UID) + "</" + HR_NAME[4] + ">\n"
         "    <" + HR_NAME[5] + ">" + String(COM_SPEED) + "</" + HR_NAME[5] + ">\n"
         "  </hardware>\n"
         "  <integer>\n";
  for (int i = 0; i < 3; i++)
    line += "    <" + IR_NAME[i] + ">" + String(ir_values[i]) + "</" + IR_NAME[i] + ">\n";
  line +=
    "  </integer>\n"
    "  <bit>\n";
  for (int i = 0; i < 3; i++)
    line += "    <" + DI_NAME[i] + ">" + String(di_values[i]) + "</" + DI_NAME[i] + ">\n";
  line +=
    "  </bit>\n"
    "</xml>";
  httpserver.send(200, TEXTPLAIN, line);
  httpquery();
  delay(100);
};

// --- MAIN ---
// initializing function
void setup(void)
{
  // set serial port
  Serial.begin(COM_SPEED, SERIAL_8N1);
  // write program information
  Serial.println("");
  Serial.println("");
  Serial.println(MSG[1]);
  Serial.println(MSG[2]);
  Serial.println(MSG[3] + "v" + SWVERSION );
  writetosyslog(4);
  Serial.println(MSG[4]);
  // initialize GPIO ports
  writetosyslog(5);
  Serial.println(MSG[5]);
  pinMode(PRT_DO_BUZZER, OUTPUT);
  pinMode(PRT_DO_LEDBLUE, OUTPUT);
  pinMode(PRT_DO_LEDGREEN, OUTPUT);
  pinMode(PRT_DO_LEDRED, OUTPUT);
  pinMode(PRT_DO_LEDYELLOW, OUTPUT);
  digitalWrite(PRT_DO_LEDBLUE, LOW);
  digitalWrite(PRT_DO_LEDGREEN, LOW);
  digitalWrite(PRT_DO_LEDRED, LOW);
  digitalWrite(PRT_DO_LEDYELLOW, LOW);
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
  writetosyslog(13);
  Serial.println(MSG[13]);
  mbtcp.server();
  // start Modbus/RTU slave
  writetosyslog(14);
  Serial.println(MSG[14]);
  mbrtu.begin(&Serial);
  mbrtu.setBaudrate(COM_SPEED);
  mbrtu.slave(MB_UID);
  Serial.println(MSG[15] + String(MB_UID));
  Serial.println(MSG[16] + String(COM_SPEED));
  // set Modbus registers
  mbtcp.addIsts(0, false, 3);
  mbrtu.addIsts(0, false, 3);
  mbtcp.addIreg(0, 0, 3);
  mbrtu.addIreg(0, 0, 3);
  mbtcp.addHreg(0, 0, 28);
  mbrtu.addHreg(0, 0, 28);
  // set Modbus callback
  mbrtu.onGetIsts(0, modbusquery, 1);
  mbrtu.onGetIreg(0, modbusquery, 1);
  mbrtu.onGetHreg(0, modbusquery, 1);
  // fill Modbus holding registers
  fillholdingregisters();
  // start webserver
  writetosyslog(17);
  Serial.println(MSG[17]);
  httpserver.onNotFound(handleNotFound);
  httpserver.on("/", handleHelp);
  httpserver.on("/summary", handleSummary);
  httpserver.on("/log", handleLog);
  httpserver.on("/get/csv", handleGetCSV);
  httpserver.on("/get/json", handleGetJSON);
  httpserver.on("/get/txt", handleGetTXT);
  httpserver.on("/get/xml", handleGetXML);
  httpserver.begin();
  Serial.println(MSG[18]);
  beep(1);
}

// loop function
void loop(void)
{
  boolean measureerror;
  httpserver.handleClient();
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
