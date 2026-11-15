#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// GitHub Actions derlerken FW_VERSION'u verir, lokalde 1 olur
#ifndef FW_VERSION
#define FW_VERSION 1
#endif

const int MEVCUT_SURUM = FW_VERSION;
const char *CIHAZ_ID = "esp32-001";
const char *SUPABASE_URL = "https://kufdzhjppthhumsutfkj.supabase.co";
const char *SUPABASE_KEY = "sb_publishable_hUVlPiNRiKpsBrs7QWuOiw_8fbmDHeJ";

const int RESET_PIN = 0;                       // BOOT dugmesi
const unsigned long KONTROL_ARALIGI = 3600000; // 1 saat
unsigned long sonKontrol = 0;

const int LED_PIN = 8;
unsigned long sonLed = 0;

void wifiBaglan() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // 3 dk sonra vazgec, loop'a gec
  wm.setAPCallback([](WiFiManager* w) {
    digitalWrite(LED_PIN, LOW); // kurulum modunda LED surekli yanik
  });

  pinMode(RESET_PIN, INPUT_PULLUP);
  if (digitalRead(RESET_PIN) == LOW) {
    delay(3000);
    if (digitalRead(RESET_PIN) == LOW) wm.resetSettings();
  }
  wm.autoConnect("ESP32-Kurulum");
  digitalWrite(LED_PIN, HIGH); // kurulum bitti, LED sonsun
}

void guncellemeKontrol()
{
  WiFiClientSecure client;
  client.setInsecure(); // canlida sertifika dogrula

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/cihazlar?cihaz_id=eq." + CIHAZ_ID +
               "&select=hedef_surum,firmware_url";
  http.begin(client, url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  if (http.GET() == 200)
  {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    int hedef = doc[0]["hedef_surum"] | 0;
    String fwUrl = doc[0]["firmware_url"] | "";

    if (hedef > MEVCUT_SURUM && fwUrl.length() > 0)
    {
      httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
      httpUpdate.update(client, fwUrl); // basarili olursa cihaz yeniden baslar
    }
  }
  http.end();
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_PIN, LOW);
    delay(150);
    digitalWrite(LED_PIN, HIGH);
    delay(150);
  }
  digitalWrite(LED_PIN, HIGH); // LED sonuk (ters mantik)
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000)
    delay(10); // monitor baglanana kadar bekle (en fazla 3 sn)

  Serial.printf("\nAcilis, firmware surum: %d\n", MEVCUT_SURUM);
  wifiBaglan();
  Serial.print("WiFi baglandi, IP: ");
  Serial.println(WiFi.localIP());
  guncellemeKontrol();
  sonKontrol = millis();
}

unsigned long sonYazdir = 0;

void loop()
{
  if (millis() - sonLed >= 5000)
  {
    sonLed = millis();
    digitalWrite(LED_PIN, LOW); // yan
    delay(100);
    digitalWrite(LED_PIN, HIGH); // son
  }
  if (millis() - sonYazdir > 5000)
  { // 5 saniyede bir
    sonYazdir = millis();
    Serial.printf("[%lus] surum=%d wifi=%s rssi=%d\n",
                  millis() / 1000, MEVCUT_SURUM,
                  WiFi.status() == WL_CONNECTED ? "bagli" : "yok",
                  WiFi.RSSI());
  }

  if (millis() - sonKontrol > KONTROL_ARALIGI)
  {
    guncellemeKontrol();
    sonKontrol = millis();
  }
}