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
const int     PRT_DO_BUZZER     = 14;
const int     PRT_DO_LEDBLUE    = 2;
const int     PRT_DO_LEDGREEN   = 0;
const int     PRT_DO_LEDRED     = 5;
const int     PRT_DO_LEDYELLOW  = 4;
const int     PRT_DI_SENSOR1    = 12;
const int     PRT_AI_SENSOR2    = 0;

// output data
const String  I_DESC[3]         = {"internal relative humidity in %",
                                   "internal temperature in &deg;C",
                                   "external temperature in &deg;C"
                                  };
const String  B_DESC[3]         = {"status of the green LED",
                                   "status of the yellow LED",
                                   "status of the red LED"
                                  };
const String  I_NAME[3]         = {"rhint", "tint", "text"};
const String  B_NAME[3]         = {"ledg", "ledy", "ledr"};
boolean       b_values[3]       = {false, false, false};
int           i_values[3]       = {0, 0, 0};

// other constants
const int     MAXADCVALUE       = 1024;
const long    INTERVAL          = 10000;
const String  SWNAME            = "MM17D";
const String  SWVERSION         = "0.1";
const String  TEXTHTML          = "text/html";
const String  TEXTPLAIN         = "text/plain";

// other variables
int           syslog[64]        = {};
String        line;
String        myipaddress;
String        mymacaddress;
unsigned long prevtime          = 0;

// messages
const String MSG[35]            =
{
  /*  0 */  "",
  /*  1 */  "MM17D * T/RH measuring device",
  /*  2 */  "Copyright (C) 2023 Pozsar Zsolt",
  /*  3 */  "http://www.pozsarzs.hu/",
  /*  4 */  "Starting device...",
  /*  5 */  "* Initializing GPIO ports",
  /*  6 */  "* Initializing sensors",
  /*  7 */  "* Connecting to wireless network",
  /*  8 */  "done",
  /*  9 */  "  my MAC address:         ",
  /* 10 */  "  my IP address:          ",
  /* 11 */  "  subnet mask:            ",
  /* 12 */  "  gateway IP address:     ",
  /* 13 */  "* Starting webserver",
  /* 14 */  "* HTTP query received ",
  /* 15 */  "* Modbus/TCP query received ",
  /* 16 */  "* Modbus/RTU query received ",
  /* 17 */  "* E01: Failed to read T/RH sensor!",
  /* 18 */  "* E02: Failed to read PT100!",
  /* 19 */  "* E03:",
  /* 20 */  "* E04:",
  /* 21 */  "* Ready, the serial console is off.",
  /* 22 */  "* Starting Modbus/TCP server",
  /* 23 */  "* Starting Modbus/RTU slave",
  /* 24 */  "  my Modbus UID:          ",
  /* 25 */  "* Green",
  /* 26 */  "* Red",
  /* 27 */  " LED is switched ",
  /* 28 */  "on.",
  /* 29 */  "off.",
  /* 30 */  "serial port speed: ",
  /* 31 */  "  get summary page",
  /* 32 */  "  get help page",
  /* 33 */  "  get log page",
  /* 34 */  "  get all measured data"
};

DHT dht(PRT_DI_SENSOR1, TYP_SENSOR1, 11);
ESP8266WebServer server(80);
ModbusIP mbtcp;
ModbusRTU mbrtu;

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

// switch on/off blue LED
void blueled(boolean b)
{
  digitalWrite(PRT_DO_LEDBLUE, b);
}

// switch on/off green LED
void greenled(boolean b)
{
  digitalWrite(PRT_DO_LEDGREEN, b);
  b_values[0] = b;
  mbtcp.Ists(0, b_values[0]);
  mbrtu.Ists(0, b_values[0]);
}

// switch on/off yellow LED
void yellowled(boolean b)
{
  digitalWrite(PRT_DO_LEDYELLOW, b);
  b_values[1] = b;
  mbtcp.Ists(1, b_values[1]);
  mbrtu.Ists(1, b_values[1]);
}

// switch on/off red LED
void redled(boolean b)
{
  digitalWrite(PRT_DO_LEDRED, b);
  b_values[2] = b;
  mbtcp.Ists(2, b_values[2]);
  mbrtu.Ists(2, b_values[2]);
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
    i_values[0] = (int)fh;
    i_values[1] = (int)ft;
    mbtcp.Ireg(0, i_values[0]);
    mbtcp.Ireg(1, i_values[1]);
    mbrtu.Ireg(0, i_values[0]);
    mbrtu.Ireg(1, i_values[1]);
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
    writetosyslog(18);
    return false;
  } else
  {
    r2 = u1 / ((U0 - u1) / R1);
    t = (((((r2 * C4 + C3) * r2 + C2) * r2 + C1) * r2) / ((((r2 * C7 + C6) * r2 + C5) * r2) + 1)) + C0;

#ifdef PT100_SIMULATION
    Serial.println("Tcalc:  " + String(int(t)) + " °C\n");
#endif

    i_values[2] = (int)t;
    mbtcp.Ireg(2, i_values[2]);
    mbrtu.Ireg(2, i_values[2]);
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

// error 404 page
void handleNotFound()
{
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

// initializing function
void setup(void)
{
  // set serial port
  Serial.begin(COM_SPEED, SERIAL_8N1);
  // write program information
  Serial.println("");
  Serial.println("");
  Serial.println(MSG[1] + " * v" + SWVERSION );
  Serial.println(MSG[2] +  " <" + MSG[3] + ">");
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
  writetosyslog(22);
  Serial.println(MSG[22]);
  mbtcp.server();
  // start Modbus/RTU slave
  writetosyslog(23);
  Serial.println(MSG[23]);
  mbrtu.begin(&Serial);
  mbrtu.setBaudrate(COM_SPEED);
  mbrtu.slave(MB_UID);
  Serial.println(MSG[24] + String(MB_UID));
  // set Modbus registers
  for (int i = 0; i < 3; i++)
  {
    mbtcp.addIsts(i, b_values[i]);
    mbrtu.addIsts(i, b_values[i]);
    mbtcp.addIreg(i, i_values[i]);
    mbrtu.addIreg(i, i_values[i]);
  }
  // set Modbus callback
  mbtcp.onGetIsts(0, modbustcpquery, 3);
  mbtcp.onGetIreg(0, modbustcpquery, 3);
  mbrtu.onGetIsts(0, modbustcpquery, 3);
  mbrtu.onGetIreg(0, modbusrtuquery, 3);
  // start webserver
  writetosyslog(13);
  Serial.println(MSG[13]);
  server.onNotFound(handleNotFound);
  // help page
  server.on("/", []()
  {
    writetosyslog(32);
    line =
      "<html>\n"
      "  <head>\n"
      "    <title>" + MSG[1] + " | Help</title>\n"
      "  </head>\n"
      "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
      "    <h2>" + MSG[1] + "</h2>\n"
      "    <br>\n"
      "    " + MSG[9] + mymacaddress + "<br>\n"
      "    " + MSG[10] + myipaddress + "<br>\n"
      "    " + MSG[24] + String(MB_UID) + "<br>\n"
      "    " + MSG[30] + String(COM_SPEED) + " baud<br>\n"
      "    software version: v" + SWVERSION + "<br>\n"
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
        "        <td>" + B_DESC[i] + "</td>\n"
        "        <td>bit</td>\n"
        "      </tr>\n";
    }
    for (int i = 0; i < 3; i++)
    {
      line +=
        "      <tr>\n"
        "        <td>" + String(i + 30001) + "</td>\n"
        "        <td>" + I_DESC[i] + "</td>\n"
        "        <td>integer</td>\n"
        "      </tr>\n";
    }
    line +=
      "    </table>\n"
      "    <br>\n"
      "    <hr>\n"
      "    <center>" + MSG[2] + " <a href=\"" + MSG[3] + "\">" + MSG[3] + "</a></center>\n"
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
      "    <title>" + MSG[1] + " | Summary</title>\n"
      "  </head>\n"
      "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
      "    <h2>" + MSG[1] + "</h2>\n"
      "    <br>\n"
      "    " + MSG[9] + mymacaddress + "<br>\n"
      "    " + MSG[10] + myipaddress + "<br>\n"
      "    " + MSG[24] + String(MB_UID) + "<br>\n"
      "    " + MSG[30] + String(COM_SPEED) + " baud<br>\n"
      "    software version: v" + SWVERSION + "<br>\n"
      "    <hr>\n"
      "    <h3>Measured values</h3>\n"
      "    <table border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n";
    for (int i = 0; i < 3; i++)
    {
      line +=
        "      <tr>\n"
        "        <td>" + I_DESC[i] + "</td>\n"
        "        <td align=\"right\">" + String(i_values[i]) + "</td>\n"
        "      </tr>\n";
    }
    for (int i = 0; i < 3; i++)
    {
      line +=
        "      <tr>\n"
        "        <td>" + B_DESC[i] + "</td>\n"
        "        <td align=\"right\">" + String(b_values[i]) + "</td>\n"
        "      </tr>\n";
    }
    line +=
      "    </table>\n"
      "    <br>\n"
      "    <hr>\n"
      "    <center>" + MSG[2] + " <a href=\"" + MSG[3] + "\">" + MSG[3] + "</a></center>\n"
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
      "    <title>" + MSG[1] + " | Log</title>\n"
      "  </head>\n"
      "  <body bgcolor=\"#e2f4fd\" style=\"font-family:\'sans\'\">\n"
      "    <h2>" + MSG[1] + "</h2>\n"
      "    <br>\n"
      "    " + MSG[9] + mymacaddress + "<br>\n"
      "    " + MSG[10] + myipaddress + "<br>\n"
      "    " + MSG[24] + String(MB_UID) + "<br>\n"
      "    " + MSG[30] + String(COM_SPEED) + " baud<br>\n"
      "    software version: v" + SWVERSION + "<br>\n"
      "    <hr>\n"
      "    <h3>Last 64 lines of system log:</h3>\n"
      "    <table border=\"0\" cellpadding=\"3\" cellspacing=\"0\">\n";
    for (int i = 0; i < 64; i++)
      if (syslog[i] > 0)
        line = line + "      <tr><td><pre>" + String(i) + "</pre></td><td><pre>" + MSG[syslog[i]] + "</pre></td></tr>\n";
    line +=
      "    </table>\n"
      "    <br>\n"
      "    <hr>\n"
      "    <center>" + MSG[2] + " <a href=\"" + MSG[3] + "\">" + MSG[3] + "</a></center>\n"
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
    line = "\"name\",\"" + SWNAME + "\"\n"
           "\"version\",\"" + SWVERSION + "\"\n";
    for (int i = 0; i < 3; i++)
      line = line + "\"" + I_NAME[i] + "\",\"" + String(i_values[i]) + "\"\n";
    for (int i = 0; i < 3; i++)
      line = line + "\"" + B_NAME[i] + "\",\"" + String(b_values[i]) + "\"\n";
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  // get all measured values in JSON format
  server.on("/get/json", []()
  {
    writetosyslog(34);
    line = "{\n"
           "  \"name\": \"" + SWNAME + "\",\n"
           "  \"version\": \"" + SWVERSION + "\",\n"
           "  \"integer\": {\n";
    for (int i = 0; i < 3; i++)
    {
      line += "    \"" + I_NAME[i] + "\": \"" + String(i_values[i]);
      if (i < 2 ) line = line + "\",\n"; else  line = line + "\"\n";
    }
    line +=
      "  },\n"
      "  \"bit\": {\n";
    for (int i = 0; i < 3; i++)
    {
      line += "    \"" + B_NAME[i] + "\": \"" + String(b_values[i]);
      if (i < 2 ) line = line + "\",\n"; else  line = line + "\"\n";
    }
    line +=
      "  }\n"
      "}\n";
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  // get all measured data in TXT format
  server.on("/get/txt", []()
  {
    writetosyslog(34);
    line = SWNAME + "\n" +
           SWVERSION + "\n";
    for (int i = 0; i < 3; i++)
      line = line + String(i_values[i]) + "\n";
    for (int i = 0; i < 3; i++)
      line = line + String(b_values[i]) + "\n";
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
           "    <name>" + SWNAME + "</name>\n"
           "    <version>" + SWVERSION + "</version>\n"
           "  </software>\n"
           "  <integer>\n";
    for (int i = 0; i < 3; i++)
      line += "    <" + I_NAME[i] + ">" + String(i_values[i]) + "</" + I_NAME[i] + ">\n";
    line +=
      "  </integer>\n"
      "  <bit>\n";
    for (int i = 0; i < 3; i++)
      line += "    <" + B_NAME[i] + ">" + String(b_values[i]) + "</" + B_NAME[i] + ">\n";
    line +=
      "  </bit>\n"
      "</xml>";
    server.send(200, TEXTPLAIN, line);
    httpquery();
    delay(100);
  });
  server.begin();
  Serial.println(MSG[21]);
  beep(1);
}
