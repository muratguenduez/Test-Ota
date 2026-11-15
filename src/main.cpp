#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#ifndef FW_VERSION
#define FW_VERSION 1
#endif

const int MEVCUT_SURUM = FW_VERSION;
const char *CIHAZ_ID = "esp32-001";
const char *SUPABASE_URL = "https://kufdzhjppthhumsutfkj.supabase.co";
const char *SUPABASE_KEY = "sb_publishable_hUVlPiNRiKpsBrs7QWuOiw_8fbmDHeJ";

const int RESET_PIN = 9;   // BOOT dugmesi
const int LED_PIN = 8;     // SuperMini mavi LED (LOW = yanar)

const unsigned long KONTROL_ARALIGI = 3600000; // 1 saat
unsigned long sonKontrol = 0;
unsigned long sonLed = 0;
unsigned long sonYazdir = 0;

void blink(int n, int ms = 200) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(ms);
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
  }
}

void wifiBaglan() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback([](WiFiManager* w) {
    digitalWrite(LED_PIN, LOW); // kurulum modunda LED surekli yanik
  });

  pinMode(RESET_PIN, INPUT_PULLUP);
  if (digitalRead(RESET_PIN) == LOW) {
    delay(3000);
    if (digitalRead(RESET_PIN) == LOW) wm.resetSettings();
  }

  wm.autoConnect("ESP32-Kurulum");
  digitalWrite(LED_PIN, HIGH);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

void guncellemeKontrol() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi yok, kontrol atlandi");
    blink(6, 100); // 6 hizli = WiFi yok
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/cihazlar?cihaz_id=eq." + CIHAZ_ID +
               "&select=hedef_surum,firmware_url";
  http.begin(client, url);
  http.addHeader("apikey", SUPABASE_KEY);

  int kod = http.GET();
  Serial.printf("Supabase HTTP kodu: %d\n", kod);

  if (kod == 200) {
    blink(2, 300); // 2 yavas = Supabase'e ulasti
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    int hedef = doc[0]["hedef_surum"] | 0;
    String fwUrl = doc[0]["firmware_url"] | "";
    Serial.printf("hedef=%d mevcut=%d\n", hedef, MEVCUT_SURUM);

    if (hedef > MEVCUT_SURUM && fwUrl.length() > 0) {
      Serial.println("Guncelleme basliyor...");
      blink(4, 100); // 4 hizli = guncelleme basliyor
      httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
      t_httpUpdate_return r = httpUpdate.update(client, fwUrl);
      Serial.printf("Guncelleme sonucu: %d\n", (int)r);
      blink(8, 100); // buraya geldiyse guncelleme basarisiz
    }
  } else {
    blink(5, 300); // 5 yavas = Supabase hatasi
  }
  http.end();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  blink(3, 150); // acilis: 3 hizli

  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000) delay(10);
  Serial.printf("\nAcilis, firmware surum: %d\n", MEVCUT_SURUM);

  wifiBaglan();
  Serial.print("WiFi IP: ");
  Serial.println(WiFi.localIP());

  guncellemeKontrol();
  sonKontrol = millis();
}

void loop() {
  if (millis() - sonLed >= 5000) {
    sonLed = millis();
    blink(1, 400); // normal calisma: 5 sn'de bir kisa
  }

  if (millis() - sonYazdir > 5000) {
    sonYazdir = millis();
    Serial.printf("[%lus] surum=%d wifi=%s\n", millis() / 1000, MEVCUT_SURUM,
                  WiFi.status() == WL_CONNECTED ? "bagli" : "yok");
  }

  if (millis() - sonKontrol > KONTROL_ARALIGI) {
    guncellemeKontrol();
    sonKontrol = millis();
  }
}