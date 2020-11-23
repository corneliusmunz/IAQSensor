
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <ESPmDNS.h>
#include <EEPROM.h>
#include "bsec.h"
#include <SD.h>
#include <SPI.h>
#include <FastLED.h>
#include "SSD1306.h"


// Replace with your network credentials
const char *ssid = "Fuchshof";
const char *password = "Luftqualitaet";
File myFile;
CRGB leds[1];
SSD1306 display(0x3c, 4, 15);


const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

#define STATE_SAVE_PERIOD UINT32_C(60 * 60 * 1000) // 360 minutes - 4 times a day

// Helper functions declarations
void checkIaqSensorStatus(void);
void loadState(void);
void updateState(void);

// Create an object of the class Bsec
Bsec iaqSensor;
String output;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // send readings timer

String processor(const String &var)
{
  if (var == "TEMPERATURE")
  {
    return String(iaqSensor.temperature);
  }
  else if (var == "HUMIDITY")
  {
    return String(iaqSensor.humidity);
  }
  else if (var == "PRESSURE")
  {
    return String(iaqSensor.pressure / 100);
  }
  else if (var == "GAS")
  {
    return String(iaqSensor.iaq);
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>FUCHSHOFSCHULE</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <!--<link rel="icon" href="data:,">-->
  <link href="http://www.fuchshofschule.de/templates/school/favicon.ico" rel="shortcut icon" type="image/vnd.microsoft.icon">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #c0b5a1; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .card.temperature { color: #0e7c7b; }
    .card.humidity { color: #17bebb; }
    .card.pressure { color: #3fca6b; }
    .card.iaq { color: #d62246; }
    .header-wrap { padding: 10px 0 0 0; height: auto; position: relative; background: url(http://www.fuchshofschule.de/templates/school/images/header-w.png) 0 0 repeat-x;
}
  </style>
</head>
<body>
  <div class="header-wrap" class="clr">  </div>
  <div class="topnav">
    <h3>Fuchshofschule Luftqualit&auml;t</h3>
  </div>
  <div class="header-wrap" class="clr">  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATUR</h4><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> LUFTFEUCHTIGKEIT</h4><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card pressure">
        <h4><i class="fas fa-angle-double-down"></i> LUFTDRUCK</h4><p><span class="reading"><span id="pres">%PRESSURE%</span> hPa</span></p>
      </div>
      <div class="card iaq">
        <h4><i class="fas fa-wind"></i> LUFTQUALIT&Auml;T</h4><p><span class="reading"><span id="gas">%GAS%</span></span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('pressure', function(e) {
  console.log("pressure", e.data);
  document.getElementById("pres").innerHTML = e.data;
 }, false);
 
 source.addEventListener('gas', function(e) {
  console.log("gas", e.data);
  document.getElementById("gas").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";

static void InitOledDisplay()
{
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  delay(50);
  digitalWrite(16, HIGH);
  display.init();
  display.clear();
  display.display();
}

void setup()
{
  Serial.begin(115200);

  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);

  InitOledDisplay();

  // SD Card Initialization
  if (SD.begin(SS))
  {
    Serial.println("SD card is ready to use.");
  }
  else
  {
    Serial.println("SD card initialization failed");
    return;
  }

  pinMode(13, OUTPUT);
  delay(50);
  FastLED.addLeds<WS2812B, 13, GRB>(leds, 1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.setBrightness(25); //0..255
  FastLED.setTemperature(Tungsten40W);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.print("locl ip: ");
  Serial.println(WiFi.localIP());
  if (!MDNS.begin("fuchshof"))
  {
    Serial.println("mDNS failed");
  }
  else
  {
    Serial.println("fuchshof.local successfully applied");
  }

  Wire.begin(4, 15);

  iaqSensor.begin(BME680_I2C_ADDR_PRIMARY, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();
  iaqSensor.setConfig(bsec_config_iaq);
  checkIaqSensorStatus();
  loadState();

  bsec_virtual_sensor_t sensorList[10] = {
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_STATIC_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId())
    {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}

void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE)
  {
    // Existing state in EEPROM
    Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
      bsecState[i] = EEPROM.read(i + 1);
      Serial.println(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  }
  else
  {
    // Erase the EEPROM with zeroes
    Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

void updateState(void)
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0)
  {
    if (iaqSensor.iaqAccuracy >= 1)
    {
      update = true;
      stateUpdateCounter++;
    }
  }
  else
  {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis())
    {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update)
  {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
      EEPROM.write(i + 1, bsecState[i]);
      Serial.println(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}

// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK)
  {
    if (iaqSensor.status < BSEC_OK)
    {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
    }
    else
    {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme680Status != BME680_OK)
  {
    if (iaqSensor.bme680Status < BME680_OK)
    {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
    else
    {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}

void setOledStatus() {

  String oledOutputIaq = "IAQ: ";
  oledOutputIaq += String(iaqSensor.iaq);
  String oledOutputTemp = "TEMP: ";
  oledOutputTemp += String(iaqSensor.temperature);
  String oledOutputHumidity = "HUM: ";
  oledOutputHumidity += String(iaqSensor.humidity);
  display.clear();
  display.drawString(0, 0, oledOutputTemp);
  display.drawString(0, 10, oledOutputHumidity);
  display.drawString(0, 20, oledOutputIaq);
  display.display();
}

void setLedStatus()
{
  if (iaqSensor.iaq > 151)
  {
    leds[0] = CRGB::Red;
  }
  else if (iaqSensor.iaq > 101)
  {
    leds[0] = CRGB::Orange;
  }
  else if (iaqSensor.iaq > 51)
  {
    leds[0] = CRGB::Yellow;
  }
  else
  {
    leds[0] = CRGB::Green;
  }    
  FastLED.show();
}

void loop()
{

  unsigned long time_trigger = millis();
  if (iaqSensor.run())
  { // If new data is available
    output = String(time_trigger);
    output += "; " + String(iaqSensor.rawTemperature);
    output += "; " + String(iaqSensor.pressure);
    output += "; " + String(iaqSensor.rawHumidity);
    output += "; " + String(iaqSensor.gasResistance);
    output += "; " + String(iaqSensor.iaq);
    output += "; " + String(iaqSensor.iaqAccuracy);
    output += "; " + String(iaqSensor.temperature);
    output += "; " + String(iaqSensor.humidity);
    output += "; " + String(iaqSensor.staticIaq);
    output += "; " + String(iaqSensor.co2Equivalent);
    output += "; " + String(iaqSensor.breathVocEquivalent);
    std::replace(output.begin(), output.end(), '.', ',');
    Serial.println(output);

    // store state in EEPROM
    updateState();

    // set Led status
    setLedStatus();
    setOledStatus();

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping", NULL, millis());
    events.send(String(iaqSensor.temperature).c_str(), "temperature", millis());
    events.send(String(iaqSensor.humidity).c_str(), "humidity", millis());
    events.send(String(iaqSensor.pressure / 100).c_str(), "pressure", millis());
    events.send(String(iaqSensor.iaq).c_str(), "gas", millis());

    if (!SD.exists("/iaqMeasurements.csv"))
    {
      //write header
      myFile = SD.open("/iaqMeasurements.csv", FILE_APPEND);
      myFile.println("Time[ms]; rawTemperature; pressure; rawHumidity; gasResitance; iaq; iaqAccuracy; temperature; humidity; staticIaq; co2Equivalent; breathVocEquivalent");
      myFile.close(); // close the file
    }

    // Create/Open file
    myFile = SD.open("/iaqMeasurements.csv", FILE_APPEND);

    // if the file opened okay, write to it:
    if (myFile)
    {
      myFile.println(output);
      myFile.close(); // close the file
    }
    // if the file didn't open, print an error:
    else
    {
      Serial.println("error opening iaqMeasurements.csv");
    }
  }
  else
  {
    checkIaqSensorStatus();
  }
}
