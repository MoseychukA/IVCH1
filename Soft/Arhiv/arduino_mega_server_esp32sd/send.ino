/* ----------------------------------------------
  Module Send HTTP requests
  part of Arduino Mega Server project
------------------------------------------------- */

#ifdef FEATURE_SEND

WiFiClient tclient;

void initSend() {
  moduleSend = ENABLE;
  started(F("Send"), true);
}

void sendHttpRequest(byte ip[], int port, WiFiClient cl) {
  String s = "";
  if (cl.connect(ip, port)) { 
    timeStamp();
    Serial.print(F("Host "));
    Serial.print(stringIp(SELF_IP));
    Serial.print(F(" "));
    Serial.println(buf);
    
    s += String(buf);
    s += '\n';
    s += "Host: ";
    s += stringIp(SELF_IP);
    s += '\n';
    cl.println(s); 
    delay(100);
    cl.stop();
  } else {
      timeStamp();
      Serial.print(F("Host "));
      Serial.print(stringIp(SELF_IP));
      Serial.print(F(" not connected ("));
      Serial.print(buf);
      Serial.println(F(")"));
    }
}

void sendRequest(byte ip[], int port, char object[], int value, WiFiClient cl) {
  sprintf(buf, "GET /?%s=%d", object, value); 
  sendHttpRequest(ip, port, cl);
}

#endif // FEATURE_SEND
