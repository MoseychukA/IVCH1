/*
  Module Wi-Fi
  part of Arduino Mega Server project
*/

char ssid[] = "ssid";
char pass[] = "password";

IPAddress ip = SELF_IP;
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void initWifi() {
  initStart(F("Wi-Fi"), false);
  Serial.print(F(" Connecting to ")); Serial.print(ssid); Serial.print(F(" "));
  WiFi.mode(WIFI_STA);
  delay(10);
  WiFi.begin(ssid, pass);
  WiFi.config(ip, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();
  Serial.println(F(" WiFi:       connected"));
  Serial.print(F(" IP address: "));
  Serial.println(WiFi.localIP());
  initDone(false);
}

