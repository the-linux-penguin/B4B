/*   B4B - BREATH FOR BEGINNERS
* ================================
* Content: 
*  • WiFi →                          connect, disconnect, Networks 
*  • read() →                        Read analog / digital / EEPROM 
*  • write() →                       Write analog / digital / servo / EEPROM 
*  • serial() →                      Read/write UART (restart() protection) 
*  • bt() →                          Initialize / read / write Bluetooth Serial 
*  • thread() →                      FreeRTOS task management (single thread) 
*  • threadCreate() / threadStop() → Multi-thread support 
*  Dependencies: 
*  WiFi.h · ESP32Servo.h · EEPROM.h · BluetoothSerial.h 
*  Notes: 
*  - Infinite loop protection (Counter++) added 
*  - Serial.begin() recall error fixed 
*  - Servo variable name conflict fixed 
*  - FreeRTOS thread support added 
* ================================
* author:the-linux-penguin
* fully open source, free to use and modify
*/
#include <WiFi.h>
#include <ESP32Servo.h>
#include <EEPROM.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

// ─────────────────────────────────────────────
// WiFi
// mode: "connect" | "disconnect" | "scan"
// ─────────────────────────────────────────────
String wifi(String mode, const char* ssid = "", const char* password = "") {
    if (mode == "connect") {
        WiFi.begin(ssid, password);
        int counter = 0;
        // DÜZELTME: counter++ eklendi, yoksa sonsuz döngüye giriyordu
        while (WiFi.status() != WL_CONNECTED && counter < 20) {
            delay(500);
            counter++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            return WiFi.localIP().toString();
        }
        return "timeout"; // bağlanamadıysa "timeout" döner
    }
    else if (mode == "disconnect") {
        WiFi.disconnect();
        return "disconnected";
    }
    else if (mode == "scan") {
        int n = WiFi.scanNetworks();
        String ssids = "";
        for (int i = 0; i < n; i++) {
            ssids += WiFi.SSID(i) + "\n";
        }
        return ssids;
    }
    return "unknown_mode";
}

// ─────────────────────────────────────────────
// Okuma
// mode: "analog" | "digital" | "eeprom" | "touch"
// ─────────────────────────────────────────────
String read(String mode, uint8_t pin) {
    if (mode == "analog") {
        return String(analogRead(pin));
    }
    if (mode == "touch") {
        return String(touchRead(pin));
    }
    else if (mode == "digital") {
        return String(digitalRead(pin));
    }
    else if (mode == "eeprom") {
        EEPROM.begin(512);
        String data = "";
        int i = 0;
        char ch = EEPROM.read(pin + i);
        while (ch != '\0' && i < 100) {
            data += ch;
            i++;
            ch = EEPROM.read(pin + i);
        }
        EEPROM.end();
        return data;
    }
    return "unknown_mode";
}

// ─────────────────────────────────────────────
// Yazma
// mode: "analog" | "digital" | "servo" | "eeprom"
// DÜZELTME: servo değişken adı çakışması giderildi
// ─────────────────────────────────────────────
void write(String mode, uint8_t pin = 2, int val = 0, int servoPin = -1, String data = "") {
    if (mode == "analog") {
        analogWrite(pin, val);
    }
    else if (mode == "digital") {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, val);
    }
    else if (mode == "servo") {
        // DÜZELTME: parametre ile değişken adı aynıydı, düzeltildi
        Servo myServo;
        myServo.attach(pin);
        myServo.write(val);
    }
    else if (mode == "eeprom") {
        if (val < 0 || val > 511) return;
        EEPROM.begin(512);
        for (int i = 0; i < (int)data.length(); ++i) {
            EEPROM.write(val + i, data[i]);
        }
        EEPROM.write(val + data.length(), '\0');
        EEPROM.commit();
        EEPROM.end();
    }
}

// ─────────────────────────────────────────────
// Seri port
// mode: "read" | "write"
// modeparam: "string" | "int"
// DÜZELTME: her çağrıda begin() çağrısı sorunuydu,
//           serialStarted flag ile önlendi
// ─────────────────────────────────────────────
static bool _serialStarted = false;

String serial(String mode, String modeparam = "string", int baudrate = 115200,
              String msg = "", String endcommand = "no") {
    // DÜZELTME: sadece bir kez başlat
    if (!_serialStarted) {
        Serial.begin(baudrate);
        _serialStarted = true;
    }

    String result = "";

    if (mode == "read") {
        if (modeparam == "string") {
            while (!Serial.available()) { delay(10); }
            result = Serial.readString();
        }
        else if (modeparam == "int") {
            while (!Serial.available()) { delay(10); }
            result = String(Serial.read());
        }
    }
    else if (mode == "write") {
        Serial.println(msg);
        result = "sent";
    }

    if (endcommand == "yes") {
        Serial.end();
        _serialStarted = false;
    }

    return result;
}

// ─────────────────────────────────────────────
// Bluetooth
// mode: "start" | "read" | "write"
// ─────────────────────────────────────────────
String bt(String mode, String name = "ESP32_BT", String msg = "") {
    if (mode == "start") {
        if (!SerialBT.begin(name)) return "error";
        return "started";
    }
    else if (mode == "read") {
        if (SerialBT.available()) {
            return SerialBT.readString();
        }
        return "";
    }
    else if (mode == "write") {
        SerialBT.println(msg);
        return "sent";
    }
    return "unknown_mode";
}

// ─────────────────────────────────────────────────────────────────
//  THREAD (FreeRTOS) — Çok basit kullanım
//
//  Ne yapar?
//    Bir fonksiyonu "arka planda" sürekli çalıştırır.
//    ESP32'de loop() donmadan paralel iş yapabilirsin.
//
//  Nasıl kullanılır?
//
//    void ledYakSon() {          ← böyle bir fonksiyon yaz
//        digitalWrite(2, HIGH);
//        delay(500);
//        digitalWrite(2, LOW);
//        delay(500);
//    }
//
//    thread("start", ledYakSon);  ← setup() içinde çalıştır
//    thread("stop");              ← durdurmak istersen
//
//  Parametreler:
//    mode     → "start" veya "stop"
//    func     → çalıştırmak istediğin fonksiyon (sadece start'ta gerekli)
//    taskName → isteğe bağlı isim (default "myTask")
//    stackSize→ bellek (default 2048, artırabilirsin)
//    priority → 0-5 arası öncelik (default 1)
// ─────────────────────────────────────────────────────────────────

static TaskHandle_t _taskHandle = NULL;

// Kullanıcının fonksiyonunu sarmak için yardımcı sarmalayıcı
struct _ThreadWrapper {
    void (*userFunc)();
};
static _ThreadWrapper _tw;

static void _threadRunner(void* param) {
    _ThreadWrapper* w = (_ThreadWrapper*)param;
    while (true) {
        w->userFunc();     // kullanıcı fonksiyonunu tekrar tekrar çağır
        vTaskDelay(1);     // watchdog için zorunlu kısa bekleme
    }
}

void thread(String mode,
            void (*func)() = nullptr,
            String taskName = "iamahappytask",
            int stackSize = 2048,
            int priority = 1)
{
    if (mode == "start") {
        if (_taskHandle != NULL) {
            // Zaten çalışan bir thread varsa önce durdur
            vTaskDelete(_taskHandle);
            _taskHandle = NULL;
        }
        if (func == nullptr) return; // fonksiyon verilmemişse çık
        _tw.userFunc = func;
        xTaskCreate(
            _threadRunner,          // çalıştırılacak iç fonksiyon
            taskName.c_str(),       // görev adı
            stackSize,              // stack boyutu
            (void*)&_tw,            // kullanıcı fonksiyonunun adresi
            priority,               // öncelik
            &_taskHandle            // handle (durdurma için saklanır)
        );
    }
    else if (mode == "stop") {
        if (_taskHandle != NULL) {
            vTaskDelete(_taskHandle);
            _taskHandle = NULL;
        }
    }
}

// Birden fazla thread çalıştırmak istersen bu versiyonu kullan:
// Döndürdüğü TaskHandle_t'yi kendin sakla ve iptal et
TaskHandle_t threadCreate(void (*func)(),
                          String taskName = "task",
                          int stackSize = 2048,
                          int priority = 1)
{
    // Her çağrıda heap'te yeni bir wrapper oluştur
    _ThreadWrapper* w = new _ThreadWrapper();
    w->userFunc = func;
    TaskHandle_t handle = NULL;
    xTaskCreate(_threadRunner, taskName.c_str(), stackSize, (void*)w, priority, &handle);
    return handle; // bunu bir değişkende tut, durdurmak için kullan
}

void threadStop(TaskHandle_t handle) {
    if (handle != NULL) {
        vTaskDelete(handle);
    }
}
