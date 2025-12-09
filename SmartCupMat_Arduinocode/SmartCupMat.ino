#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MLX90640.h>

// --- Define Screen and Sensor ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C 

// --- Pin Config ---
// MKR 1310: Do not use Pin 6 (Used by LED)
#define PIN_PRESSURE  5   // Sensor pin
#define PIN_BUZZER    4   // Buzzer pin
#define ALERT_TIME    5000 // Time limit: 5000ms (5 seconds)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MLX90640 mlx;
float frame[32 * 24]; 

// --- Global Variables ---
unsigned long currentPressStartTime = 0; 
unsigned long currentDuration = 0;       
bool isPressed = false;           
bool wasPressed = false;          

// --- Check if buzzer beeped already ---
bool hasBeeped = false; 

void setup() {
  // MKR WAN 1310 Built-in LED is Pin 6
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Use internal resistor (No press = HIGH, Press = LOW)
  pinMode(PIN_PRESSURE, INPUT_PULLUP); 
  
  // --- Setup Buzzer ---
  pinMode(PIN_BUZZER, OUTPUT); 
  noTone(PIN_BUZZER); // Start silent

  Serial.begin(115200);
  
  long startWait = millis();
  while (!Serial && (millis() - startWait < 2000)); 

  Serial.println("System Start (MKR1310 - Double Beep)");

  Wire.begin();
  Wire.setClock(400000); 

  // Setup Screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 failed"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Init MLX90640...");
  display.display();

  // Setup Thermal Sensor
  if (! mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 not found!");
    while (1);
  }
  
  mlx.setMode(MLX90640_INTERLEAVED); 
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_2_HZ);
}

void loop() {
  // --- Read Sensor Data ---
  digitalWrite(LED_BUILTIN, HIGH);
  if (mlx.getFrame(frame) != 0) {
    Serial.println("Frame read fail");
    digitalWrite(LED_BUILTIN, LOW); 
    return; 
  }
  digitalWrite(LED_BUILTIN, LOW);

  float centerTemp = frame[11 * 32 + 15];
  float maxTemp = -100.0;
  for (int i = 0; i < 768; i++) {
    if (frame[i] > maxTemp) maxTemp = frame[i];
  }

  // --- Timer Logic ---
  // Read pin (Low means pressed)
  isPressed = (digitalRead(PIN_PRESSURE) == LOW);

  if (isPressed) {
    // === Case: Cup is on sensor ===
    
    if (!wasPressed) {
      currentPressStartTime = millis(); 
      hasBeeped = false; 
      noTone(PIN_BUZZER); 
      Serial.println(">>> Cup Placed.");
    }
    
    currentDuration = millis() - currentPressStartTime;
    wasPressed = true;

    // --- Check Time ---
    if (currentDuration >= ALERT_TIME) {
      currentDuration = ALERT_TIME; 
      
      // --- Double Beep Logic ---
      if (!hasBeeped) {
        Serial.println("[ALARM] Double Beep!");
        
        // Beep 1 (4000Hz, 0.2 sec)
        tone(PIN_BUZZER, 4000); 
        delay(200); 
        noTone(PIN_BUZZER);
        
        // Wait (0.1 sec)
        delay(100); 
        
        // Beep 2 (4000Hz, 0.2 sec)
        tone(PIN_BUZZER, 4000);
        delay(200);
        noTone(PIN_BUZZER);
        
        hasBeeped = true; // Stop beeping
      }
      
    } else {
      // Not time yet, stay silent
      noTone(PIN_BUZZER);
    }

  } else {
    // === Case: Cup is gone ===
    
    if (wasPressed) {
      Serial.println(">>> Cup Lifted. Reset.");
    }

    // Reset everything
    noTone(PIN_BUZZER); 
    currentDuration = 0; 
    wasPressed = false;
    hasBeeped = false; 
  }

  // --- Update Screen ---
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (currentDuration >= ALERT_TIME) {
    display.print("DRINK WATER!"); 
  } else if (isPressed) {
    display.print("Timing...");    
  } else {
    display.print("Ready");        
  }
  
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 15); display.print("Center");
  display.setTextSize(2);
  display.setCursor(0, 28); display.print(centerTemp, 1);
  
  display.setTextSize(1);
  display.setCursor(80, 15); display.print("Max");
  display.setCursor(80, 28); display.print(maxTemp, 0);
  
  display.drawRect(0, 58, 128, 6, SSD1306_WHITE); 

  if (isPressed) {
      long remainingMs = ALERT_TIME - currentDuration;
      if (remainingMs < 0) remainingMs = 0;

      display.setTextSize(1);
      display.setCursor(50, 48); 
      
      if (remainingMs == 0) {
          display.print("DONE!"); 
      } else {
          display.print((float)remainingMs / 1000.0, 1); 
          display.print("s");
      }

      int barWidth = map(currentDuration, 0, ALERT_TIME, 0, 128);
      display.fillRect(0, 58, barWidth, 6, SSD1306_WHITE);
  } else {
      display.setTextSize(1);
      display.setCursor(40, 48); 
      display.print("No Cup"); 
  }
  
  display.display();
}