#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h> 
#include <Wire.h>
#include <Adafruit_INA219.h>

// --- 1. WI-FI CONFIGURATION ---
#define WIFI_SSID "Arya"
#define WIFI_PASSWORD "Arya0212"

// --- 2. SUPABASE SECURE CREDENTIALS ---
#define SUPABASE_URL "https://aodfguenuwdpymkbwzwn.supabase.co"
#define SUPABASE_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFvZGZndWVudXdkcHlta2J3enduIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Nzc5NDYyMzksImV4cCI6MjA5MzUyMjIzOX0.M4_I7a0--FaVN1RPoNVrGb6iIFYz24xM-_jOaosysIk"
#define HARDWARE_ID "rvce_hardware" 

// --- 3. HARDWARE PIN DEFINITIONS ---
// RELAYS
#define RELAY_1_PIN 25 // Load Source (NC = Grid, NO = Battery) -> Fail-Safe: Grid Power to House
#define RELAY_2_PIN 26 // Solar Route (NC = Battery, NO = Grid) -> Fail-Safe: Charge Battery

// ANALOG SENSORS (Use ADC1 Pins only, ADC2 fails with WiFi)
#define LM35_PIN 34           // Battery Temperature
#define GRID_VOLTAGE_PIN 35   // Anti-Islanding Grid Sensor (0-25V Module)

// STRATEGY INDICATOR LEDS
#define LED_RED_PIN 32        // Import & Charge (Battery < 40%)
#define LED_ORANGE_PIN 33     // Balanced Cycle (Normal Operation)
#define LED_YELLOW_PIN 27     // Self Sufficient (Load on Battery)
#define LED_GREEN_PIN 14      // Aggressive Export (Solar to Grid)
#define LED_GRID_FAULT_PIN 15 // ANTI-ISLANDING FAULT LED (5th LED - Blinks on Grid Drop)

// --- 4. HARDWARE OBJECTS ---
WiFiClientSecure secureClient;

// INA219 I2C Addresses
Adafruit_INA219 inaSolar(0x40);
Adafruit_INA219 inaBattery(0x41);
Adafruit_INA219 inaLoad(0x44);

unsigned long readDataPrevMillis = 0;
unsigned long sendDataPrevMillis = 1500; 
unsigned long lastEnergyCalcMillis = 0;

double totalImportedWh = 0.0;
double totalExportedWh = 0.0;

// Global Variables for Continuous Loop Access
float currentGridVoltage = 0.0;
bool isGridActive = false;
int batteryPct = 0; // Promoted to global so LEDs can access it continuously

// =====================================================================
// FREERTOS DUAL-CORE TASK: SAFETY & LED DRIVER (RUNS ON CORE 0)
// This guarantees the Strobe and Anti-Islanding react instantly,
// completely immune to Wi-Fi delays or HTTP blocking!
// =====================================================================
void safetyAndLedTask(void * pvParameters) {
  for(;;) {
    unsigned long currentMillis = millis();

    // 1. Instant Anti-Islanding Check
    long sumGridADC = 0;
    for(int i = 0; i < 10; i++) sumGridADC += analogRead(GRID_VOLTAGE_PIN);
    currentGridVoltage = ((sumGridADC / 10.0) / 4095.0) * 3.3 * 5.0; 
    
    // Threshold set to 8.0V for a 12V Grid Simulation
    isGridActive = (currentGridVoltage > 8.0);

    // If grid drops, instantly disconnect Solar Export (Force R2 to NC/Battery)
    if (!isGridActive && digitalRead(RELAY_2_PIN) == LOW) {
        digitalWrite(RELAY_2_PIN, HIGH);
        Serial.println("\n[CRITICAL FAULT] GRID DOWN! Anti-Islanding engaged. Severed solar export.");
    }

    // 2. Drive LEDs Seamlessly (Immune to Wi-Fi Latency)
    bool loadFromBattery = (digitalRead(RELAY_1_PIN) == LOW); // R1 NO
    bool solarToGrid     = (digitalRead(RELAY_2_PIN) == LOW); // R2 NO
    
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_ORANGE_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, LOW);

    if (!isGridActive) {
      digitalWrite(LED_GRID_FAULT_PIN, HIGH); // Solid Grid Fault
      // True 1-Second Strobe perfectly timed
      digitalWrite(LED_RED_PIN, (currentMillis / 1000) % 2 == 0 ? HIGH : LOW);
    } else {
      digitalWrite(LED_GRID_FAULT_PIN, LOW); // Grid is healthy
      if (!loadFromBattery && !solarToGrid) {
        if (batteryPct < 40) digitalWrite(LED_RED_PIN, HIGH);     // Import & Charge
        else digitalWrite(LED_ORANGE_PIN, HIGH);                  // Balanced Cycle
      } else if (loadFromBattery && !solarToGrid) {
        digitalWrite(LED_YELLOW_PIN, HIGH);                       // Self-Sufficient
      } else if (solarToGrid) {
        digitalWrite(LED_GREEN_PIN, HIGH);                        // Aggressive Export
      }
    }
    
    // Yield to the RTOS watchdog, loop runs 20 times a second
    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n--- Solar Enerlytics ESP32 Booting (ANTI-ISLANDING Masterpiece with 5 LEDs) ---");

  // Initialize Relays (Active LOW, NC Default)
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  digitalWrite(RELAY_1_PIN, HIGH); // Load -> Grid
  digitalWrite(RELAY_2_PIN, HIGH); // Solar -> Battery

  // Initialize LEDs
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_ORANGE_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_GRID_FAULT_PIN, OUTPUT);

  // Startup LED Sequence
  digitalWrite(LED_RED_PIN, HIGH); delay(150); digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_ORANGE_PIN, HIGH); delay(150); digitalWrite(LED_ORANGE_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, HIGH); delay(150); digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH); delay(150); digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_GRID_FAULT_PIN, HIGH); delay(150); digitalWrite(LED_GRID_FAULT_PIN, LOW);

  // Configure ESP32 Analog for Sensors
  analogReadResolution(12); 
  analogSetAttenuation(ADC_11db); 

  // Initialize I2C and Sensors
  Wire.begin();
  if (!inaSolar.begin()) Serial.println("Failed to find INA219 Solar!");
  if (!inaBattery.begin()) Serial.println("Failed to find INA219 Battery!");
  if (!inaLoad.begin()) Serial.println("Failed to find INA219 Load!");
  
  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(500);
  }
  Serial.println("\nConnected to Wi-Fi.");

  secureClient.setInsecure();

  // Sync Time
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); 
  time_t now = time(nullptr);
  while (now < 100000) { delay(500); now = time(nullptr); }
  Serial.println("Time Synchronized.");

  // Launch the Safety and LED task on Core 0!
  xTaskCreatePinnedToCore(
    safetyAndLedTask,   /* Task function. */
    "SafetyTask",       /* name of task. */
    2048,               /* Stack size of task */
    NULL,               /* parameter of the task */
    1,                  /* priority of the task */
    NULL,               /* Task handle to keep track of created task */
    0);                 /* pin task to core 0 */

  // --- RECOVER BILLING DATA ---
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/devices?id=eq." + HARDWARE_ID + "&select=billing_imported,billing_exported";
  http.begin(secureClient, url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  
  if (http.GET() == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    if(doc.size() > 0) {
      totalImportedWh = doc[0]["billing_imported"].as<double>() * 1000.0;
      totalExportedWh = doc[0]["billing_exported"].as<double>() * 1000.0;
    }
  }
  http.end();
  
  lastEnergyCalcMillis = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  // =====================================================================
  // SUPABASE TELEMETRY ENGINE (Runs on Core 1)
  // Contains only blocking HTTP requests and slow sensor queries.
  // =====================================================================
  if (WiFi.status() == WL_CONNECTED) {
    
    // TASK 1: READ RELAY COMMANDS
    if (currentMillis - readDataPrevMillis >= 3000 || readDataPrevMillis == 0) {
      readDataPrevMillis = currentMillis;
      if (readDataPrevMillis == 0) readDataPrevMillis = 1; 
      
      HTTPClient http;
      String url = String(SUPABASE_URL) + "/rest/v1/devices?id=eq." + HARDWARE_ID + "&select=relay_r1,relay_r2";
      http.begin(secureClient, url);
      http.addHeader("apikey", SUPABASE_KEY);
      http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      
      if (http.GET() == 200) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, http.getString());

        if(doc.size() > 0) {
          bool dbR1 = doc[0]["relay_r1"].as<bool>();
          bool dbR2 = doc[0]["relay_r2"].as<bool>();

          // ANTI-ISLANDING SECURITY CHECK: Refuse grid export command if grid is dead
          if (!isGridActive) {
             dbR2 = false; 
          }

          digitalWrite(RELAY_1_PIN, dbR1 ? LOW : HIGH);
          digitalWrite(RELAY_2_PIN, dbR2 ? LOW : HIGH);
        }
      }
      http.end();
    }

    // TASK 2: SENSORS & TELEMETRY PUSH
    if (currentMillis - sendDataPrevMillis >= 3000) {
      sendDataPrevMillis = currentMillis;
      
      // Read Sensors
      float solarV = inaSolar.getBusVoltage_V();
      float solarI = inaSolar.getCurrent_mA() / 1000.0;
      float solarP = (solarV * solarI) > 0 ? (solarV * solarI) : 0;

      float batteryV = inaBattery.getBusVoltage_V();
      float loadV = inaLoad.getBusVoltage_V();
      float loadI = inaLoad.getCurrent_mA() / 1000.0;
      float loadP = (loadV * loadI) > 0 ? (loadV * loadI) : 0;

      // Read LM35 Temp
      long sumADC = 0;
      for(int i = 0; i < 20; i++) { sumADC += analogRead(LM35_PIN); delay(2); }
      float batteryTemp = ((sumADC / 20.0) / 4095.0) * 3.3 * 100.0;
      if (batteryTemp <= 0 || batteryTemp > 100) batteryTemp = 29.0; 

      // Update Global Battery Pct for LED Logic
      batteryPct = constrain(map(batteryV * 100, 1100, 1280, 0, 100), 0, 100);

      // Grid Math
      bool loadFromBattery = (digitalRead(RELAY_1_PIN) == LOW); 
      bool solarToGrid     = (digitalRead(RELAY_2_PIN) == LOW); 

      float gridExport = 0.0;
      if (!loadFromBattery) gridExport += loadP; 
      if (solarToGrid) gridExport -= solarP;     

      // Accumulate Billing
      unsigned long timeNow = millis();
      float deltaHours = (timeNow - lastEnergyCalcMillis) / 3600000.0; 
      lastEnergyCalcMillis = timeNow;

      if (gridExport > 0) totalImportedWh += (gridExport * deltaHours);
      else if (gridExport < 0) totalExportedWh += (abs(gridExport) * deltaHours);

      time_t epochNow = time(nullptr); 

      // JSON Push
      HTTPClient httpLive;
      httpLive.begin(secureClient, String(SUPABASE_URL) + "/rest/v1/devices?on_conflict=id");
      httpLive.addHeader("apikey", SUPABASE_KEY);
      httpLive.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      httpLive.addHeader("Content-Type", "application/json");
      httpLive.addHeader("Prefer", "resolution=merge-duplicates, return=minimal");

      DynamicJsonDocument liveDoc(1024);
      liveDoc["id"] = HARDWARE_ID;
      liveDoc["timestamp"] = (int)epochNow;
      liveDoc["solar_power"] = solarP;
      liveDoc["solar_voltage"] = solarV;
      liveDoc["solar_current"] = solarI;
      liveDoc["battery_percentage"] = batteryPct;
      liveDoc["battery_voltage"] = batteryV;
      liveDoc["battery_temp"] = batteryTemp;
      liveDoc["load_power"] = loadP;
      liveDoc["grid_import_export"] = gridExport;
      liveDoc["billing_imported"] = totalImportedWh / 1000.0;
      liveDoc["billing_exported"] = totalExportedWh / 1000.0;
      
      // Send new Anti-Islanding parameters to Web App
      liveDoc["grid_voltage"] = currentGridVoltage;
      liveDoc["grid_active"] = isGridActive;

      String livePayload;
      serializeJson(liveDoc, livePayload);
      if (httpLive.POST(livePayload) >= 200) {
        Serial.printf("[LIVE] Bat: %d%% | Grid Volts: %.1fV | Active: %s\n", batteryPct, currentGridVoltage, isGridActive ? "YES" : "NO");
      }
      httpLive.end();

      HTTPClient httpHist;
      httpHist.begin(secureClient, String(SUPABASE_URL) + "/rest/v1/history");
      httpHist.addHeader("apikey", SUPABASE_KEY);
      httpHist.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      httpHist.addHeader("Content-Type", "application/json");
      httpHist.addHeader("Prefer", "return=minimal");

      DynamicJsonDocument histDoc(1024);
      histDoc["id"] = String(epochNow) + "000";
      histDoc["device_id"] = HARDWARE_ID;
      histDoc["solarV"] = solarV;
      histDoc["solarI"] = solarI;
      histDoc["solarP"] = solarP;
      histDoc["batteryPct"] = batteryPct;
      histDoc["loadP"] = loadP;
      histDoc["gridExport"] = gridExport;

      String histPayload;
      serializeJson(histDoc, histPayload);
      httpHist.POST(histPayload);
      httpHist.end();
    }
  }
}
