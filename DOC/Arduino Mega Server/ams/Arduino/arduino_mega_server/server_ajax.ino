/*
  Module Server / Ajax
  part of Arduino Mega Server project
*/

// Design
#define DESIGN_DEFAULT 0
#define DESIGN_HOME    1
#define DESIGN_MODERN  2
#define DESIGN_HACKER  3
#define DESIGN_PART1   4
#define DESIGN_PART2   5
#define DESIGN_PART3   6
#define DESIGN_PART4   7
byte currentDesign = DESIGN_DEFAULT;

// Pages
#define UNKNOWN_PAGE   0
#define THEMES_PAGE    4
#define SETTINGS_PAGE  5
#define PORTS_PAGE     6
#define SUPPLY_PAGE    7
#define ELECTRO_PAGE   8
byte currentPage = UNKNOWN_PAGE;

#define GET           "GET /"
#define INDEX_STR     "index"
#define HTM_EXT       ".htm"

#define BASE_PIR      "pir"
#define BASE_CONTACT  "cont"
#define BASE_LITE     "lite"
#define BASE_MAC      "mac"
#define BASE_IP       "ip"
#define CHECKED_STR   "checked"
#define UNCHECKED_STR "unchecked"

// Server Answers

String makeAnswer(String content) {
  String s = "";
  s += F("HTTP/1.1 200 OK\n");
  s += F("Content-Type: ");
  s += content;
  s += F("\n");
  s += F("Connnection: close\n"); // "Connection: keep-alive\n"
  return s;
}

void sendHtmlAnswer(EthernetClient cl) {cl.println(makeAnswer(F("text/html")));}
void sendCssAnswer (EthernetClient cl) {cl.println(makeAnswer(F("text/css")));}
void sendJsAnswer  (EthernetClient cl) {cl.println(makeAnswer(F("application/javascript")));}
void sendPngAnswer (EthernetClient cl) {cl.println(makeAnswer(F("image/png")));}
void sendJpgAnswer (EthernetClient cl) {cl.println(makeAnswer(F("image/jpeg")));}
void sendGifAnswer (EthernetClient cl) {cl.println(makeAnswer(F("image/gif")));}
void sendXmlAnswer (EthernetClient cl) {cl.println(makeAnswer(F("text/xml")));}
void sendIcoAnswer (EthernetClient cl) {cl.println(makeAnswer(F("image/x-icon")));}

//void sendDownAnswer(EthernetClient cl) {cl.println(makeAnswer(F("application/octet-stream")));}
//void sendPdfAnswer (EthernetClient cl) {cl.println(makeAnswer(F("application/x-pdf")));}
//void sendZipAnswer (EthernetClient cl) {cl.println(makeAnswer(F("application/x-zip")));}
//void sendGzAnswer  (EthernetClient cl) {cl.println(makeAnswer(F("application/x-gzip")));}
//void sendElseAnswer(EthernetClient cl) {cl.println(makeAnswer(F("text/plain")));}

void sendErrorAnswer(char mess[], EthernetClient cl) {
  cl.print(mess);
  cl.println(F(" ERROR"));
  cl.println(F("Connnection: close"));
  cl.println();
}

String tagXmlVersion() {return F("<?xml version = \"1.0\" ?>");}
String openInputs()    {return F("\n<inputs>\n");}
String closeInputs()   {return F("</inputs>");}

// Tags

String makeTag(String tagBase, String tagCount, String value) {
  String s = "";
  s += "<"; s += tagBase; s += tagCount; s += ">";
  s += value;
  s += "</"; s += tagBase; s += tagCount; s += ">\n";
  return s;
}

// File

bool openWebFile() {
  char *fileName;
  fileName = strtok(HTTP_req, GET);
  webFile = SD.open(fileName);
  if (webFile) {return true;}
          else {return false;}
}

bool openIndexFile(String s) {
  webFile = SD.open(INDEX_STR + s + HTM_EXT);
  if (webFile) {return true;}
          else {return false;}
}

String partSuffix(byte design) {
  switch (design) {
    case DESIGN_PART1:  return F("-p1"); break;
    case DESIGN_PART2:  return F("-p2"); break;
    case DESIGN_PART3:  return F("-p3"); break;
    case DESIGN_PART4:  return F("-p4"); break;
               default: return "";
  }
}

// Parse Request

void parseRequest(EthernetClient cl) {
  allowMarkers = false;
  
  // index request
  if (StrContains(HTTP_req, "GET / ") || StrContains(HTTP_req, "GET /index.htm")) {
    if (openIndexFile(partSuffix(currentDesign))) {sendHtmlAnswer(cl);} else {webFile = SD.open(F("404.htm"));}
    allowMarkers = true;
  }
  else if (StrContains(HTTP_req, GET)) {
    // files requests
    if      (StrContains(HTTP_req, HTM_EXT)) {if (openWebFile()) {sendHtmlAnswer(cl);} else {webFile = SD.open(F("404.htm"));} allowMarkers = true;}
    else if (StrContains(HTTP_req, ".css"))  {if (openWebFile()) {sendCssAnswer(cl);}  else {sendErrorAnswer("", cl);}}
    else if (StrContains(HTTP_req, ".js"))   {if (openWebFile()) {sendJsAnswer(cl);}   else {sendErrorAnswer("", cl);}}
    else if (StrContains(HTTP_req, ".pde"))  {if (openWebFile()) {sendJsAnswer(cl);}   else {sendErrorAnswer("", cl);}}
    else if (StrContains(HTTP_req, ".png"))  {if (openWebFile()) {sendPngAnswer(cl);}  else {sendErrorAnswer("", cl);}}
    else if (StrContains(HTTP_req, ".jpg"))  {if (openWebFile()) {sendJpgAnswer(cl);}  else {sendErrorAnswer("", cl);}}
    else if (StrContains(HTTP_req, ".gif"))  {if (openWebFile()) {sendGifAnswer(cl);}  else {sendErrorAnswer("", cl);}}
    else if (StrContains(HTTP_req, ".ico"))  {if (openWebFile()) {sendIcoAnswer(cl);}  else {sendErrorAnswer("", cl);}}
    // Ajax requests
    else if (StrContains(HTTP_req, "request_dash"))    {sendXmlAnswer(cl); checkPage();      respDash(cl);}
    else if (StrContains(HTTP_req, "reqIndicators"))   {sendXmlAnswer(cl);                   respIndicators(cl);}
    else if (StrContains(HTTP_req, "reqGeneric"))      {sendXmlAnswer(cl); setGeneric();     respGeneric(cl);}
    else if (StrContains(HTTP_req, "reqSettings"))     {sendXmlAnswer(cl); setSettings();    respSettings(cl);}
    else if (StrContains(HTTP_req, "request_themes"))  {sendXmlAnswer(cl); setTheme();       respThemes(cl);}
    else if (StrContains(HTTP_req, "request_control")) {sendXmlAnswer(cl); setControl();     respControl(cl);}
    else if (StrContains(HTTP_req, "request_noo"))     {sendXmlAnswer(cl); setMt1132();      respMt1132(cl);}
    else if (StrContains(HTTP_req, "request_mr1132"))  {sendXmlAnswer(cl); setMr1132();      respMr1132(cl);}
    else if (StrContains(HTTP_req, "req_el_control"))  {sendXmlAnswer(cl); setElectroCtrl(); respElectroCtrl(cl);}
    else if (StrContains(HTTP_req, "req_el_freq"))     {sendXmlAnswer(cl); checkOscill();    respElectroFreq(cl);}
    else if (StrContains(HTTP_req, "request_sdcard"))  {sendXmlAnswer(cl);                   respSd(cl);}
    else if (StrContains(HTTP_req, "request_network")) {sendXmlAnswer(cl);                   respNetwork(cl);}
    else if (StrContains(HTTP_req, "request_nrf24"))   {sendXmlAnswer(cl);                   respNrf24(cl);}
  } // else if (StrContains(HTTP_req, GET))
} // parseRequest ( )

// Make Response

#ifdef CONTACTS_FEATURE
  String makeContacts() {
    String s = "";
    s += makeTag(BASE_CONTACT, "1", String(cont1objectState));
    s += makeTag(BASE_CONTACT, "2", String(cont2objectState));
    return s;
  }
#endif

#ifdef PIRS_FEATURE
  String makePirs() {
    String s = "";
    s += makeTag(BASE_PIR, "1", String(pir1));
    s += makeTag(BASE_PIR, "2", String(pir2));
    s += makeTag(BASE_PIR, "3", String(pir3));
    s += makeTag(BASE_PIR, "4", String(pir4));
    s += makeTag(BASE_PIR, "5", String(pir5));
    return s;
  }
#endif

#ifdef ELECTRO_FEATURE
  String makeElectro() {
    String s = "";
    byte digits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    for (byte i = 1; i < MAX_UI_SENSORS; i++) {
      s += makeTag("p", String(digits[i]), String(ajaxUPrms[i]));
    }
    s += makeTag("volt", "", String(ajaxUPrms[0]));
    return s;
  }
#endif

String makeModules() {
  String s = "";
  s += makeTag(F("mSdInfo"),  "", String(moduleSdInfo));
  s += makeTag(F("mSdFiles"), "", String(moduleSdFiles));
  s += makeTag(F("mNtp"),     "", String(moduleNtp));
  s += makeTag(F("mSend"),    "", String(moduleSend));
  s += makeTag(F("mMajor"),   "", String(moduleMajor));
  s += makeTag(F("mLaurent"), "", String(moduleLaurent));
  s += makeTag(F("mUpload"),  "", String(moduleUpload));
  s += makeTag(F("mPirs"),    "", String(modulePirs));
  s += makeTag(F("mConts"),   "", String(moduleContacts));
  s += makeTag(F("mDs18b20"), "", String(moduleDs18b20));
  s += makeTag(F("mPm"),      "", String(modulePm));
  s += makeTag(F("mLeds"),    "", String(moduleLeds));
  s += makeTag(F("mKeys"),    "", String(moduleKeys));
  s += makeTag(F("mNooMr"),   "", String(moduleMr1132));
  s += makeTag(F("mNooMt"),   "", String(moduleMt1132));
  s += makeTag(F("mNrf24"),   "", String(moduleNrf24));
  return s;
}

String makeDigitalPorts() {
  String s = "";
  byte pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 22, 23, 24, 25, 26, 27, 30, 31};
  for (byte i = 0; i < sizeof(pins); i++) {
    s += makeTag("D", String(pins[i]), String(digitalRead(pins[i])));
  }
  return s;
}

String makeAnalogPorts() {
  String s = "";
  for (byte i = 0; i < MAX_ANALOG_PORTS; i++) {
    s += makeTag("A", String(i), String(analogRead(i)));
  }
  return s;
}

String makeDigits(int digits) {
  String s = "";
  if (digits < 10) {s += '0';}
  s += String(digits);
  return s;
}

String makeTime() {
  String s = "";
  s += "<time>";
    s += makeDigits(hour());
    if (second() % 2 == 0) {s += " ";}
                      else {s += ":";}
    s += makeDigits(minute());
  s += "</time>\n";
  return s;
}

String makeCpuLoad() {
  String s = "";
  s += makeTag("cycDelay", "", String(cyclosDelay));
  s += makeTag("cycInSec", "", String(cyclosInSec));
  return s;
}

String makeHttpReq() {
  String s = "";
  for (int i = 0; i < reqIndex; i++) {
    if (HTTP_req[i] == '&') {s += ' ';}
                       else {s += HTTP_req[i];}
  }
  return makeTag("httpReq", "", s);
}

// Indicators

void respIndicators(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    for (byte i = 0; i < 6; i++) { // A2 to A5
      s += makeTag("analog", "", String(analogRead(i)));
    }
  s += closeInputs();
  cl.println(s);
}

// Generic

void setGeneric() {
  // D3
  if (StrContains(HTTP_req, "LED4=1")) {
    LED_state[3] = 1;
    digitalWrite(3, HIGH);
  } else if (StrContains(HTTP_req, "LED4=0")) {
      LED_state[3] = 0;
      digitalWrite(3, LOW);
    }
  // D5
  if (StrContains(HTTP_req, "LED3=1")) {
    LED_state[2] = 1;
    digitalWrite(5, HIGH);
  } else if (StrContains(HTTP_req, "LED3=0")) {
      LED_state[2] = 0;
      digitalWrite(5, LOW);
    }
  // D6
  if (StrContains(HTTP_req, "LED1=1")) {
    LED_state[0] = 1;  // save LED state
    digitalWrite(6, HIGH);
  } else if (StrContains(HTTP_req, "LED1=0")) {
      LED_state[0] = 0;  // save LED state
      digitalWrite(6, LOW);
    }

  // D7
  if (StrContains(HTTP_req, "LED2=1")) {
    LED_state[1] = 1;
    digitalWrite(7, HIGH);
  } else if (StrContains(HTTP_req, "LED2=0")) {
      LED_state[1] = 0;
      digitalWrite(7, LOW);
    }
} // SetGeneric()

void respGeneric(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    // HTTP_req
    s += makeHttpReq();
    
    // Analog
    for (byte i = 0; i < 6; i++) { // A2 to A5
      s += makeTag("analog", "", String(analogRead(i)));
    }
    
    // Switches
    byte sw_arr[] = {42, 43, 45};
    for (byte i = 0; i < 3; i++) {
      s += makeTag("switch", "", digitalRead(sw_arr[i]) ? "ON" : "OFF");
    }
    
    // Checkboxes
    s += makeTag("LED", "", LED_state[0] ? CHECKED_STR : UNCHECKED_STR);
    s += makeTag("LED", "", LED_state[1] ? CHECKED_STR : UNCHECKED_STR);
    // Buttons
    s += makeTag("LED", "", LED_state[2] ? "on" : "off");
    s += makeTag("LED", "", LED_state[3] ? "on" : "off");
  s += closeInputs();
  cl.println(s);
} // respGeneric( )

// Network

void respNetwork(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeTag(BASE_MAC, "1", String(SELF_MAC[0], HEX));
    s += makeTag(BASE_MAC, "2", String(SELF_MAC[1], HEX));
    s += makeTag(BASE_MAC, "3", String(SELF_MAC[2], HEX));
    s += makeTag(BASE_MAC, "4", String(SELF_MAC[3], HEX));
    s += makeTag(BASE_MAC, "5", String(SELF_MAC[4], HEX));
    s += makeTag(BASE_MAC, "6", String(SELF_MAC[5], HEX));
    s += makeTag(BASE_IP,  "1", String(SELF_IP[0]));
    s += makeTag(BASE_IP,  "2", String(SELF_IP[1]));
    s += makeTag(BASE_IP,  "3", String(SELF_IP[2]));
    s += makeTag(BASE_IP,  "4", String(SELF_IP[3]));
  s += closeInputs();
  cl.println(s);
}

// Electro

void setElectroCtrl() {

}

void respElectroCtrl(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    
  s += closeInputs();
  cl.println(s);
}

// Settings

void setSettings() {
  if      (StrContains(HTTP_req, "box1=1")) {authMode = ON;} 
  else if (StrContains(HTTP_req, "box1=0")) {authMode = OFF;}
}

void respSettings(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeTag("box1", "", authMode ? CHECKED_STR : UNCHECKED_STR);
  s += closeInputs();
  cl.println(s);
}

// Themes

void setTheme() {
  if      (StrContains(HTTP_req, "theme=1")) {currentDesign = DESIGN_PART1;}
  else if (StrContains(HTTP_req, "theme=2")) {currentDesign = DESIGN_PART2;}
  else if (StrContains(HTTP_req, "theme=3")) {currentDesign = DESIGN_PART3;}
  else if (StrContains(HTTP_req, "theme=4")) {currentDesign = DESIGN_PART4;}  
  else if (StrContains(HTTP_req, "theme=5")) {currentDesign = DESIGN_DEFAULT;}
  else if (StrContains(HTTP_req, "theme=6")) {currentDesign = DESIGN_HOME;} 
  else if (StrContains(HTTP_req, "theme=7")) {currentDesign = DESIGN_MODERN;}
  else if (StrContains(HTTP_req, "theme=8")) {currentDesign = DESIGN_HACKER;}
}

String getThemeCode() {
  switch (currentDesign) {
    case DESIGN_PART1:   return "1"; break;
    case DESIGN_PART2:   return "2"; break;
    case DESIGN_PART3:   return "3"; break;
    case DESIGN_PART4:   return "4"; break;
    case DESIGN_DEFAULT: return "5"; break;
    case DESIGN_HOME:    return "6"; break;
    case DESIGN_MODERN:  return "7"; break;
    case DESIGN_HACKER:  return "8"; break;
                default: return "";
  }
}

void respThemes(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeTag("theme", "", getThemeCode());
  s += closeInputs();
  cl.println(s);
}

// Control

byte light1 = 0;
byte light2 = 0;
byte light3 = 0;
byte light4 = 0;
byte light5 = 0;

void setControl() {
 #ifdef LEDS_FEATURE
  if (StrContains(HTTP_req, "light1=1")) {
    light1 = 1;
    red(led1, 127);
  } 
  else if (StrContains(HTTP_req, "light1=0")) {
    light1 = 0;
    black(led1);
  }

  if (StrContains(HTTP_req, "light2=1")) {
    light2 = 1;
    blue(led1, 127);
  } 
  else if (StrContains(HTTP_req, "light2=0")) {
    light2 = 0;
    black(led1);
  }

  if (StrContains(HTTP_req, "light3=1")) {
    light3 = 1;
    green(led1, 127);
  } 
  else if (StrContains(HTTP_req, "light3=0")) {
    light3 = 0;
    black(led1);
  }

  if (StrContains(HTTP_req, "light4=1")) {
    light4 = 1;
    magenta(led1, 127);
  } 
  else if (StrContains(HTTP_req, "light4=0")) {
    light4 = 0;
    black(led1);
  }

  if (StrContains(HTTP_req, "light5=1")) {
    light5 = 1;
    yellow(led1, 127);
  } 
  else if (StrContains(HTTP_req, "light5=0")) {
    light5 = 0;
    black(led1);
  }
 #endif // LEDS_FEATURE
} // setControl()

void respControl(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeTag(BASE_LITE, "1", String(light1));
    s += makeTag(BASE_LITE, "2", String(light2));
    s += makeTag(BASE_LITE, "3", String(light3));
    s += makeTag(BASE_LITE, "4", String(light4));
    s += makeTag(BASE_LITE, "5", String(light5));
  s += closeInputs();
  cl.print(s);
}

// nooLite MT1132

#define STATE_OFF     0
#define STATE_ON      1
#define STATE_UNKNOWN 2

#define UNBIND        0
#define BIND          1
#define BIND_UNKNOWN  2

byte noo1state = STATE_OFF;
byte noo2state = STATE_OFF;
byte noo3state = STATE_OFF;

byte noo2multiCommand;
byte noo3multiCommand;

char *channelNum;

void setMt1132() {
 #ifdef MT1132_FEATURE
  // noo1
  if (StrContains(HTTP_req, "noo1state=1")) {
    noo1state = STATE_ON;
    nooOn(NOO_CHANNEL_1);
  } 
  else if (StrContains(HTTP_req, "noo1state=0")) {
    noo1state = STATE_OFF;
    nooOff(NOO_CHANNEL_1);
  }

  // noo2
  if (StrContains(HTTP_req, "noo2state=1")) {
    noo2state = STATE_ON;
    nooOn(NOO_CHANNEL_2);
  } 
  else if (StrContains(HTTP_req, "noo2state=0")) {
    noo2state = STATE_OFF;
    nooOff(NOO_CHANNEL_2);
  }  

  // noo3
  if (StrContains(HTTP_req, "noo3state=1")) {
    noo3state = STATE_ON;
    //nooOn(NOO_CHANNEL_3);
    nooSetLedLastState(NOO_CHANNEL_3);
  } 
  else if (StrContains(HTTP_req, "noo3state=0")) {
    noo3state = STATE_OFF;
    nooOff(NOO_CHANNEL_3);
  } 

  // bind/unbind
  byte pos;
  String commandStr;
  byte command;
  byte bind;

  if (StrContains(HTTP_req, "nooChannel=")) {
    pos = request.indexOf("nooChannel=") + 11;
    commandStr = request.substring(pos, pos + 2);
    command = commandStr.toInt();
  }

  if (StrContains(HTTP_req, "nooBind=1")) {
    bind = 1;
  } 
  else if (StrContains(HTTP_req, "nooBind=0")) {
    bind = 0;
  }
  
  if (command >= 0 && command <= 31) {
    if (bind == 1) {
      nooBind(command);
    } else {
        nooUnbind(command);
      }
    Serial.print("command: "); Serial.println(command);
    Serial.print("action: ");  Serial.println(bind);
  }

  byte commandCorrect;

  // noo2multi
  if (StrContains(HTTP_req, "noo2multi=")) {
    pos = request.indexOf("noo2multi=") + 10;
    commandStr = request.substring(pos, pos + 2);
    noo2multiCommand = commandStr.toInt();

    if (noo2multiCommand >= 10 && noo2multiCommand <= 90) {
      commandCorrect = byte(noo2multiCommand * 1.2 + 34);
      nooLed(NOO_CHANNEL_1, 0, 0, 0);
      nooValue(NOO_CHANNEL_2, commandCorrect);
      Serial.print("(Multi) command: ");
      Serial.println(noo2multiCommand);
      Serial.print("(Multi) command correct: ");
      Serial.println(commandCorrect);      
    }    
  }

  // noo3multi
  if (StrContains(HTTP_req, "noo3multi=")) {
    pos = request.indexOf("noo3multi=") + 10;
    commandStr = request.substring(pos, pos + 2);
    noo3multiCommand = commandStr.toInt();

    if (noo3multiCommand >= 10 && noo3multiCommand <= 90) {
      switch (noo3multiCommand) {
        case 10: nooRed    (NOO_CHANNEL_3, 127); break;
        case 20: nooGreen  (NOO_CHANNEL_3, 127); break;
        case 30: nooBlue   (NOO_CHANNEL_3, 127); break;
        case 40: nooYellow (NOO_CHANNEL_3, 127); break;
        case 50: nooMagenta(NOO_CHANNEL_3, 127); break;
        case 60: nooCyan   (NOO_CHANNEL_3, 127); break;
        case 70: nooGray   (NOO_CHANNEL_3, 127); break;
        case 80: nooGray   (NOO_CHANNEL_3, 127); break;
        case 90: nooGray   (NOO_CHANNEL_3, 127); break;
        default: ;
      }
    }    
  }
 #endif // MT1132_FEATURE
} // setMt1132()

void respMt1132(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeTag("noo1", "state", String(noo1state));
    s += makeTag("noo2", "state", String(noo2state));
    s += makeTag("noo3", "state", String(noo3state));
    s += makeTag("noo2multi", "", String(noo2multiCommand));
    s += makeTag("noo3multi", "", String(noo3multiCommand));
  s += closeInputs();
  cl.println(s);
}

// nooLite MR1132

void setMr1132() {
  // bind/unbind
  byte pos;
  String commandStr;
  byte command;
  byte bind;

  if (StrContains(HTTP_req, "nooChannel=")) {
    pos = request.indexOf("nooChannel=") + 11;
    commandStr = request.substring(pos, pos + 2);
    command = commandStr.toInt();
  }

  if (StrContains(HTTP_req, "nooBind=1")) {
    bind = 1;
  } 
  else if (StrContains(HTTP_req, "nooBind=0")) {
    bind = 0;
  }

  #ifdef MR1132_FEATURE
    if (command >= 0 && command <= 31) {
      if (bind == 1) {
        mrBind(command);
      } else {
          mrUnbind(command);
        }
    }
  #endif
} // setMr1132()

void respMr1132(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
  #ifdef MR1132_FEATURE
    s += makeTag("ns1", "Batt",     String(ns1BatteryState));
    s += makeTag("ns1", "Temp",     String(ns1Temp));
    s += makeTag("ns2", "Batt",     String(ns2BatteryState));
    s += makeTag("ns2", "Humidity", String(ns2Humidity));
    s += makeTag("ns2", "Temp",     String(ns2Temp));
    s += makeTag("ns1", "State",    String(np1State));
  #endif
  s += closeInputs();
  cl.println(s);
}

// nRF24

void respNrf24(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    #ifdef NRF24_FEATURE
      s += makeTag("c1", "Name",            nrf1Name);
      s += makeTag("c1", "Id",              nrf1Id);
      s += makeTag("c1", "Addr",     String(nrf1Addr));
      s += makeTag("c1", "Batt",     String(nrf1Battery));
      s += makeTag("c1", "Health",   String(nrf1Health));
      s += makeTag("c1", "FreeMem",  String(nrf1FreeMem));
      s += makeTag("c1", "SelfTemp", String(nrf1SelfTemp));
      s += makeTag("c1", "Temp1",    String(nrf1Temp1));
    #endif
  s += closeInputs();
  cl.println(s);
}

// Check page

void checkPage() {
  currentPage = UNKNOWN_PAGE;
  if      (StrContains(HTTP_req, "settings-page")) {currentPage = SETTINGS_PAGE;}
  else if (StrContains(HTTP_req, "themes-page"))   {currentPage = THEMES_PAGE;}
  else if (StrContains(HTTP_req, "ports-page"))    {currentPage = PORTS_PAGE;}
  else if (StrContains(HTTP_req, "supply-page"))   {currentPage = SUPPLY_PAGE;}
  else if (StrContains(HTTP_req, "electro-page"))  {currentPage = ELECTRO_PAGE;}  
}

// Oscill

void checkOscill() {
  if (StrContains(HTTP_req, "oscill")) {
    oscill = true;
  } else {
      oscill = false;
    }
}

void respElectroFreq(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    #ifdef ELECTRO_FEATURE
      for (int i = 0; i < MAX_FORM_POINTS; i++) {s += makeTag("u", "", String(pointsU[i]));}
      for (int i = 0; i < MAX_FORM_POINTS; i++) {s += makeTag("i", "", String(pointsI[i]));}
    #endif 
   s += closeInputs();
  cl.println(s);
}

// SD card

void respSd(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    #ifdef SD_INFO_FEATURE
      s += makeTag("sdType",   "", String(sdType()));
      s += makeTag("sdSystem", "", String(sdFileSystem()));
      s += makeTag("sdSize",   "", String(sdVolumeSize()));
    #endif
    #ifdef SD_FILES_FEATURE
      //s += makeTag("sdRootDir","", s); // s.substring(0, 300)
    #endif    
  s += closeInputs();
  cl.println(s);
}

// Dash

void respDash(EthernetClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeModules();
    s += makeTag("uptime",   "", stringUptime());
    s += makeTag("freeRAM",  "", String(freeMem()));
    s += makeCpuLoad();
    s += makeTag("lifer",    "", String(getLifer()));
    s += makeTime();
    s += makeTag("day",      "", String(day()));
    s += makeTag("month",    "", String(month()));
    s += makeTag("weekday",  "", String(weekday()));

    if (currentPage == PORTS_PAGE) {
      s += makeDigitalPorts();
      s += makeAnalogPorts();
    }
    #ifdef PIRS_FEATURE
      s += makePirs();
    #endif
    #ifdef CONTACTS_FEATURE
      s += makeContacts();
    #endif  
    #ifdef LAURENT_FEATURE
      s += makeTag("laurent", "", lrequest);
    #endif
    #ifdef DS18B20_FEATURE
      for (byte i = 0; i < getMaxDs18b20Sensors(); i++) {
        s += makeTag(getDs18b20Name(i), "", String(getDs18b20Value(i), 1));
      }
    #endif
    #ifdef ELECTRO_FEATURE
      s += makeTag("voltage", "", String(ajaxUPrms[0]));
      s += makeTag("power",   "", String(ajaxUPrms[1]));
      if (currentPage == ELECTRO_PAGE) {
        s += makeElectro();
        s += makeTag("moduleElectro", "", String(modulePm));
      }
      if (currentPage == SUPPLY_PAGE) {
        s += makeTag("period", "", String(periodInMicros));
      }
    #endif
   
    s += makeTag("timeProv", "", String(timeProvider));
    s += makeTag("auth",     "", String(authMode));

  s += closeInputs();
  cl.println(s);
} // respDash( )

