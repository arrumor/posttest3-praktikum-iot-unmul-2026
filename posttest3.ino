#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// --- KONFIGURASI WIFI & MQTT ---
const char* ssid = "Redmi Note 13";
const char* password = "rumahorbo";
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// --- DEFINISI PIN ---
const int pinSensor = 4;   
const int pinBuzzer = 6;   
const int pinServo  = 20;  

// --- VARIABEL PENAMPUNG ---
int nilaiAir = 0;
Servo pintuAir;

// --- VARIABEL KONTROL & MODE ---
bool isAuto = true;
int manualServoPos = 0;
bool manualBuzzer = false;

// --- VARIABEL TIMER ---
unsigned long waktuSebelumnya = 0;
const long intervalPublish = 2000; // 2 detik untuk kirim data ke Kodular

// Fungsi menerima pesan dari Kodular
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Menerima [");
  Serial.print(topic);
  Serial.print("] : ");
  Serial.println(message);

  // Menerima Perintah Kontrol (Servo & Buzzer)
  if (String(topic) == "prak5/iot/control") {
    if (message == "0" || message == "90" || message == "180") {
      manualServoPos = message.toInt();
    } else if (message == "ON") {
      manualBuzzer = true;
    } else if (message == "OFF") {
      manualBuzzer = false;
    }
  }
  
  // Menerima Perintah Mode (Auto/Manual)
  else if (String(topic) == "prak5/iot/mode") {
    if (message == "AUTO") isAuto = true;
    else if (message == "MANUAL") isAuto = false;
  }
}

void setup_wifi() {
  Serial.begin(115200); 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Dam_Client_Baru")) { 
      Serial.println("MQTT Connected");
      client.subscribe("prak5/iot/control");
      client.subscribe("prak5/iot/mode");
    } else {
      delay(2000);
    }
  }
}

// Fungsi penentu status teks
String getStatusAir(int value) {
  if (value <= 800) return "Aman";
  else if (value <= 1500) return "Waspada";
  else return "Bahaya";
}

void setup() {
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  pinMode(pinBuzzer, OUTPUT);

  // --- INISIALISASI SERVO ---
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  pintuAir.setPeriodHertz(50);
  pintuAir.attach(pinServo, 500, 2400); 
  pintuAir.write(0);
  
  Serial.println("Sistem Bendungan Pintar Aktif...");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop(); // Jaga koneksi MQTT tetap hidup

  // Baca sensor real-time
  nilaiAir = analogRead(pinSensor);
  String statusAir = getStatusAir(nilaiAir);
  unsigned long waktuSekarang = millis();

  int currentServo = 0;
  String currentBuzzer = "OFF";

  // --- LOGIKA KENDALI UTAMA ---
  if (isAuto) {
    // --- MODE OTOMATIS (Sesuai Referensi Terbaru) ---
    if (nilaiAir <= 800) {
      currentServo = 0;
      pintuAir.write(currentServo);
      digitalWrite(pinBuzzer, LOW);
      currentBuzzer = "OFF";
      
    } else if (nilaiAir > 800 && nilaiAir <= 1500) {
      currentServo = 90;
      pintuAir.write(currentServo);
      digitalWrite(pinBuzzer, LOW);
      currentBuzzer = "OFF";
      
    } else { // Kondisi Bahaya (> 1500)
      currentServo = 180;
      pintuAir.write(currentServo);
      currentBuzzer = "BERKEDIP";
      
      // Alarm tetap berkedip cepat saat bahaya menggunakan delay
      digitalWrite(pinBuzzer, HIGH);
      delay(100); 
      digitalWrite(pinBuzzer, LOW);
      delay(100);
    }
  } else {
    // --- MODE MANUAL ---
    currentServo = manualServoPos;
    pintuAir.write(currentServo);
    
    if (manualBuzzer) {
      digitalWrite(pinBuzzer, HIGH);
      currentBuzzer = "ON";
    } else {
      digitalWrite(pinBuzzer, LOW);
      currentBuzzer = "OFF";
    }
  }

  // --- BAGIAN TAMPILAN & PENGIRIMAN KE KODULAR (Tiap 2 Detik) ---
  if (waktuSekarang - waktuSebelumnya >= intervalPublish) {
    waktuSebelumnya = waktuSekarang; 
    
    // Tampilan di Serial Monitor
    Serial.print("--- Laporan Berkala (2s) ---");
    Serial.print(" Ketinggian Air: ");
    Serial.print(nilaiAir);
    Serial.printf(" [%s] | Mode: %s\n", statusAir.c_str(), isAuto ? "AUTO" : "MANUAL");

    // Kirim data ke Kodular
    client.publish("prak5/iot/SensorAir", String(nilaiAir).c_str());
    client.publish("prak5/iot/StatusLevel", statusAir.c_str());
    client.publish("prak5/iot/NilaiServo", String(currentServo).c_str());
    client.publish("prak5/iot/StatusBuzzer", currentBuzzer.c_str());
  }
}