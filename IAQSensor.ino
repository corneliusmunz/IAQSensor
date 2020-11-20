
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "M5Atom.h"
#include <ESPmDNS.h>

// Replace with your network credentials
const char *ssid = "Fuchshof";
const char *password = "Luftqualitaet";

//Adafruit_BME680 bme; // I2C
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);

float temperature;
float humidity;
float pressure;
float gasResistance;

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // send readings timer

void getBME680Readings()
{
  // Tell BME680 to begin measurement.
  /*
  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }
  */
  temperature = random(0, 100);   //bme.temperature;
  pressure = random(0, 100);      //bme.pressure / 100.0;
  humidity = random(0, 100);      //bme.humidity;
  gasResistance = random(0, 100); //bme.gas_resistance / 1000.0;

  if (gasResistance < 33)
  {
    M5.dis.fillpix(CRGB::Green);
    M5.update();
  }
  else if (gasResistance < 66)
  {
    M5.dis.fillpix(CRGB::Orange);
    M5.update();
  }
  else
  {
    M5.dis.fillpix(CRGB::Red);
    M5.update();
  }
}

String processor(const String &var)
{
  getBME680Readings();
  //Serial.println(var);
  if (var == "TEMPERATURE")
  {
    return String(temperature);
  }
  else if (var == "HUMIDITY")
  {
    return String(humidity);
  }
  else if (var == "PRESSURE")
  {
    return String(pressure);
  }
  else if (var == "GAS")
  {
    return String(gasResistance);
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
    .card.gas { color: #d62246; }
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
      <div class="card gas">
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

void setup()
{
  Serial.begin(115200);

  M5.begin(true, false, true);
  delay(50);
  M5.dis.fillpix(CRGB::Red);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.print("locl ip: ");
  Serial.println(WiFi.localIP());
  if (!MDNS.begin("fuchshof")) {
    Serial.println("mDNS failed");
  }else{
    Serial.println("fuchshof.local successfully applied");
  }


  // Init BME680 sensor
  /*
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    while (1);
  }
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
*/
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

void loop()
{
  if ((millis() - lastTime) > timerDelay)
  {
    getBME680Readings();
    Serial.printf("Temperature = %.2f ºC \n", temperature);
    Serial.printf("Humidity = %.2f % \n", humidity);
    Serial.printf("Pressure = %.2f hPa \n", pressure);
    Serial.printf("Gas Resistance = %.2f KOhm \n", gasResistance);
    Serial.println();

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping", NULL, millis());
    events.send(String(temperature).c_str(), "temperature", millis());
    events.send(String(humidity).c_str(), "humidity", millis());
    events.send(String(pressure).c_str(), "pressure", millis());
    events.send(String(gasResistance).c_str(), "gas", millis());

    lastTime = millis();
  }
}
