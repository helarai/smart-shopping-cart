#include <Wire.h>           
#include <Adafruit_GFX.h>    
#include <Adafruit_SSD1306.h> 
#include <SPI.h>             
#include <MFRC522.h>         
#define BLYNK_TEMPLATE_ID "TMPL3de9Q8UHz"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#include <ESP8266WiFi.h>     
#include <BlynkSimpleEsp8266.h> 

#define SCREEN_WIDTH 128  
#define SCREEN_HEIGHT 64  
#define OLED_RESET    -1    
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SS_PIN D8         
#define RST_PIN D3        
MFRC522 rfid(SS_PIN, RST_PIN); 

// WiFi & Blynk Credentials
char auth[] = "TBQ0cT6E0ZEFLpUDNtezQckvD2eswiGV";
char ssid[] = "OnePlus Nord CE 2 Lite 5G";
char pass[] = "get lost";

struct Item {
  byte uid[4];        
  const char* name;
  float price;
};

Item items[] = {
  {{0xFB, 0x5D, 0xE8, 0x3B}, "Jeans", 10},
  {{0xEB, 0xBB, 0x9C, 0x3B}, "T-shirt", 8},
  {{0xEB, 0x7D, 0x62, 0x3B}, "iphone", 50},
  {{0xFB, 0x06, 0x88, 0x3B}, "Toothbrush", 3},
  {{0xEB, 0x3F, 0xBE, 0x3B}, "Laptop", 100},
  {{0x13, 0x1C, 0x79, 0x22}, "PANJAMI", 0}  
};

const int numItems = sizeof(items) / sizeof(items[0]);
bool itemState[numItems];   

#define BUTTON_PIN 2  

float totalAmount = 0.0;  
float walletBalance = 200.0; 

bool lastButtonState = HIGH;
bool buttonPressed = false;
bool scanningEnabled = true;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

BlynkTimer timer;
String billDetails = ""; 

bool compareUID(byte *uid, byte *storedUID) {
  for (byte i = 0; i < 4; i++) {
    if (uid[i] != storedUID[i]) return false;
  }
  return true;
}

void updateBlynkBill() {
  String bill = "📜 *BILL SUMMARY* 📜\n";
  bill += "---------------------------\n";
  bill += billDetails;  // Add scanned items
  bill += "---------------------------\n";
  bill += "💰 *Total:* $" + String(totalAmount, 2) + "\n";
  bill += "💳 *Wallet:* $" + String(walletBalance, 2) + "\n";
  bill += "---------------------------\n";
  
  Blynk.virtualWrite(V10, bill); // Send to Blynk Terminal
}

void setup() {
  Serial.begin(115200);
  
  // Reset item states
  for (int i = 0; i < numItems; i++) {
    itemState[i] = false;
  }

  // Reset total amount and wallet balance
  totalAmount = 0.0;
  walletBalance = 200.0; // Reset to initial wallet balance
  billDetails = ""; // Clear bill details

  Blynk.begin(auth, ssid, pass);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }

  SPI.begin();
  rfid.PCD_Init();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("RFID Ready!");
  display.display();
  Serial.println("RFID & OLED Initialized");

  // ✅ *Clear Blynk LCD Display (if using Virtual Pin V9)*
  Blynk.virtualWrite(V9, " "); 

  // ✅ *Clear Blynk Terminal (if using Virtual Pin V10 for bill details)*
  Blynk.virtualWrite(V10, " "); 

  // ✅ *Reset Wallet Balance in Blynk (Virtual Pin V8)*
  Blynk.virtualWrite(V8, walletBalance);

  // ✅ *Clear Last Scanned Item Price (Virtual Pin V5)*
  Blynk.virtualWrite(V5, " "); 
  

  // ✅ *Reset Total Amount in Blynk (Optional)*
  Blynk.virtualWrite(V6, totalAmount); 
}


void loop() {
  Blynk.run();
  timer.run();
  
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH && lastButtonState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
    buttonPressed = true;
    scanningEnabled = false;
    lastDebounceTime = millis();
  } else if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressed = false;
    scanningEnabled = true;
  }
  lastButtonState = buttonState;
  
  if (scanningEnabled) {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
    
    Serial.print("RFID Tag UID: ");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    const char* itemName = "Unknown Item";
    float itemPrice = 0.0;
    bool itemFound = false;
    int foundIndex = -1;
    
    for (int i = 0; i < numItems; i++) {
      if (compareUID(rfid.uid.uidByte, items[i].uid)) {
        itemName = items[i].name;
        itemPrice = items[i].price;
        itemFound = true;
        foundIndex = i;
        break;
      }
    }
    
    if (itemFound && foundIndex != -1) {
      if (!itemState[foundIndex]) {
        totalAmount += itemPrice;
        walletBalance -= itemPrice;
        itemState[foundIndex] = true;
        
        billDetails += "🛒 " + String(itemName) + " - $" + String(itemPrice, 2) + "\n";
        
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 10);
        display.println("Added:");
        display.setCursor(10, 35);
        display.print(itemName);
        display.display();
        
        Serial.print("Added Item: ");
        Serial.println(itemName);
      } else {
        totalAmount -= itemPrice;
        walletBalance += itemPrice;
        itemState[foundIndex] = false;

        billDetails.replace("🛒 " + String(itemName) + " - $" + String(itemPrice, 2) + "\n", "❌ " + String(itemName) + " removed\n");
        
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 10);
        display.println("Removed:");
        display.setCursor(10, 35);
        display.print(itemName);
        display.display();
        
        Serial.print("Removed Item: ");
        Serial.println(itemName);
      }
      
      Blynk.virtualWrite(V9, itemName);
      Blynk.virtualWrite(V5, itemPrice);
      Blynk.virtualWrite(V8, walletBalance);
      updateBlynkBill();  // Send bill update to Blynk
    }
    
    rfid.PICC_HaltA();
  }
  
  if (buttonPressed) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println("Total Amount");
    display.setTextSize(3);
    display.setCursor(10, 40);
    display.print("$");
    display.println(totalAmount, 2);
    display.display();
    
    updateBlynkBill(); // Show the bill in Blynk Terminal
    delay(2000);
  }
  
  delay(500);
}