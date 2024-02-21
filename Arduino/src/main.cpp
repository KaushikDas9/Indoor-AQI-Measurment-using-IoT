#include <Arduino.h>
#include <math.h>
#include <MQ135.h>
#include <DHT.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_Sensor.h>
#include <SparkFun_SGP30_Arduino_Library.h>
#include <Wire.h>
#define pin A3
#define dust A0
#define DHTPIN 2
#define out 7
#define DHTTYPE DHT11
MQ135 mq135_sensor(pin);
DHT dht(DHTPIN, DHTTYPE);
SGP30 sgp;
float Ro = 4.78;

double co2, co, nh4, pm25;
float temperature, humidity, voltage, calVoltage, dustDensity;

double Resistance(int raw)
{
  double vlaue = (double)raw;
  return (10 * (1023 - vlaue) / vlaue);
}

double corrResistance(int raw)
{
  return Resistance(raw) / mq135_sensor.getCorrectionFactor(temperature, humidity);
}
double Mqread()
{
  double rs = 0.0;
  for (int i = 0; i < 5; i++)
  {
    rs += corrResistance(analogRead(pin));
    delay(50);
  }
  return (rs / (float)5);
}

double GetPercentage(double *pcurve)
{
  double rs = Mqread();
  // Serial.println(rs);

  double ratio = rs / Ro;
  // Serial.println(ratio);
  double ppm = pow(10.0, ((log(ratio) - pcurve[0]) / pcurve[1]));
  return ppm;
}
double readPM25()
{
  voltage = analogRead(dust);
  calVoltage = voltage * (5.0 / 1024);
  dustDensity = 0.17 * calVoltage - 0.1;
  return dustDensity;
}
double readCO()
{
  double COcurve[2] = {1.6, -0.25};
  return GetPercentage(COcurve);
}
double readNH4()
{
  double NH4curve[2] = {1.9, -0.4};
  return GetPercentage(NH4curve);
}
double aqiCO()
{
  double aqi[6][4] = {{0, 1.0, 0, 50.0},
                      {1.1, 2.0, 51.0, 100.0},
                      {2.1, 10.0, 101.0, 200.0},
                      {10.1, 17.0, 201.0, 300},
                      {17.1, 34.0, 301.0, 400.0},
                      {34, 1e9, 401.0, 500}};
  int ind;
  double BPh, BPl, Ih, Il;
  double Cp = co;
  for (int i = 0; i < 6; i++)
  {
    if (Cp >= aqi[i][0] && Cp <= aqi[i][1])
    {
      BPh = aqi[i][1];
      BPl = aqi[i][0];
      Ih = aqi[i][3];
      Il = aqi[i][2];
      break;
    }
  }
  double ppm = ((Ih - Il) / (BPh - BPl)) * (Cp - BPl) + (Il);
  return ppm;
}
double aqiNH4()
{
  double aqi[6][4] = {{0, 200.0, 0, 50.0},
                      {201.0, 400.0, 51.0, 100.0},
                      {401.0, 800.0, 101.0, 200.0},
                      {801.0, 1200.0, 201.0, 300},
                      {1201.1, 1800.0, 301.0, 400.0},
                      {1800, 1e9, 401.0, 500}};
  int ind;
  double BPh, BPl, Ih, Il;
  double Cp = nh4;
  for (int i = 0; i < 6; i++)
  {
    if (Cp >= aqi[i][0] && Cp <= aqi[i][1])
    {
      BPh = aqi[i][1];
      BPl = aqi[i][0];
      Ih = aqi[i][3];
      Il = aqi[i][2];
      break;
    }
  }
  double ppm = ((Ih - Il) / (BPh - BPl)) * (Cp - BPl) + (Il);
  return ppm;
}
double aqiPM25()
{
  double aqi[6][4] = {{0, 30.0, 0, 50.0},
                      {31.0, 60.0, 51.0, 100.0},
                      {61.0, 90.0, 101.0, 200.0},
                      {91.0, 120.0, 201.0, 300},
                      {121.1, 250.0, 301.0, 400.0},
                      {251, 1e9, 401.0, 500}};
  int ind;
  double BPh, BPl, Ih, Il;
  double Cp = pm25;
  for (int i = 0; i < 6; i++)
  {
    if (Cp >= aqi[i][0] && Cp <= aqi[i][1])
    {
      BPh = aqi[i][1];
      BPl = aqi[i][0];
      Ih = aqi[i][3];
      Il = aqi[i][2];
      break;
    }
  }
  double ppm = ((Ih - Il) / (BPh - BPl)) * (Cp - BPl) + (Il);
  return ppm;
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wire.begin();
  // SD.begin();
  pinMode(out, OUTPUT);
  // Ro = 4.78;
  dht.begin();
  if (sgp.begin() == false)
  {
    // Serial.println("No SGP30 Detected. Check connections.");
    while (1)
      ;
  }
  if (!SD.begin())
  {
    // Serial.println("No SD Detected. Check connections.");
    while (1)
      ;
  }
  sgp.initAirQuality();
  // Serial.print("ok");
}

void loop()
{
  // put your main code here, to run repeatedly:
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  sgp.measureAirQuality();
  if (isnan(humidity) || isnan(temperature))
  {
    // Serial.println(F("Failed to read from DHT sensor!"));
    // return;
  }
  co2 = sgp.CO2;
  co = readCO();
  nh4 = readNH4();
  // pm25 = readPM25();
  double tvoc = sgp.TVOC;
  double Coaqi = aqiCO();
  double NH4aqi = aqiNH4();
  // double PM25aqi = aqiPM25();
  double AQI = max(Coaqi, NH4aqi);
  File dataFile = SD.open("room1.txt", FILE_WRITE);
  if (AQI > 200)
  {
    digitalWrite(out, HIGH);
  }
  else
  {
    digitalWrite(out, LOW);
  }
  if (dataFile)
  {
    dataFile.print(co2);
    dataFile.print("\t");
    dataFile.print(co);
    dataFile.print("\t");
    // dataFile.print(pm25);
    // dataFile.print("\t");
    dataFile.print(nh4);
    dataFile.print("\t");
    dataFile.print(tvoc);
    dataFile.print("\t");
    dataFile.print(AQI);
    dataFile.print("\t");
    dataFile.print(temperature);
    dataFile.print("\t");
    dataFile.print(humidity);
    dataFile.println();
    dataFile.close();
    // Serial.print(co2);
    // Serial.print(",");
    // Serial.print(co);
    // Serial.print(",");
    // // Serial.print(pm25);
    // // Serial.print(",");
    // Serial.print(nh4);
    // Serial.print(",");
    // Serial.print(tvoc);
    // Serial.print(",");
    // Serial.print(AQI);
    // Serial.print(",");
    // Serial.print(humidity);
    // Serial.print(",");
    // Serial.print(temperature);
    // Serial.println(" ");
  }
  delay(500000);
}
