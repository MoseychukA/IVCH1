/*
  Module Server Ajax
  part of Arduino Mega Server project
*/

// design
#define DEFAULT_DESIGN 0
#define HOME_DESIGN    1
#define MODERN_DESIGN  2
#define HACKER_DESIGN  3
#define PART1_DESIGN   4
#define PART2_DESIGN   5
#define PART3_DESIGN   6
#define PART4_DESIGN   7
byte currentDesign = DEFAULT_DESIGN;

// pages
#define UNKNOWN_PAGE   0
#define THEMES_PAGE    1
#define SETTINGS_PAGE  2
#define PORTS_PAGE     3
#define SUPPLY_PAGE    4
#define ELECTRO_PAGE   5
byte currentPage = UNKNOWN_PAGE;

// strings
char basePir[]      = "pir";
char baseContact[]  = "cont";
char basePower[]    = "p";
char baseLine[]     = "line";
char baseMAC[]      = "mac";
char baseIP[]       = "ip";
char strChecked[]   = "checked";
char strUnChecked[] = "unchecked";

// for sample Ajax
boolean LED_state[4] = {0};

// Server answers

String makeAnswer(String content) {
  String s = "";
  s += F("HTTP/1.1 200 OK\n");
  s += F("Content-Type: ");
  s += content;
  s += F("\n");
  s += F("Connnection: close\n");
  return s;
}

void sendHtmlAnswer(WiFiClient cl) {cl.println(makeAnswer("text/html"));}
void sendCssAnswer (WiFiClient cl) {cl.println(makeAnswer("text/css"));}
void sendJsAnswer  (WiFiClient cl) {cl.println(makeAnswer("application/javascript"));}
void sendImgAnswer (WiFiClient cl) {cl.println(makeAnswer("image/png"));}
void sendXmlAnswer (WiFiClient cl) {cl.println(makeAnswer("text/xml"));}

void sendErrorAnswer(char mess[], WiFiClient cl) {
  cl.print(mess);
  cl.println(F(" ERROR"));
  cl.println(F("Connnection: close"));
  cl.println();
}

void sendXmlVersion(WiFiClient cl) {
  cl.println(F("<?xml version = \"1.0\" ?>"));
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

// Parse request

void parseRequest(WiFiClient cl) {
  allowMarkers = false;
  char *fileName;

  // Files requests
        
  if (StrContains(HTTP_req, "GET / ") || StrContains(HTTP_req, "GET /index.htm")) {
    switch (currentDesign) {
      case PART1_DESIGN: webFile = SD.open(F("/index-p1.htm")); break;
      case PART2_DESIGN: webFile = SD.open(F("/index-p2.htm")); break;
      case PART3_DESIGN: webFile = SD.open(F("/index-p3.htm")); break;
      case PART4_DESIGN: webFile = SD.open(F("/index-p4.htm")); break;
                default: webFile = SD.open(F("/index.htm"));
    }
    
    if (webFile) {sendHtmlAnswer(cl);}
            else {webFile = SD.open(F("/404.htm"));}
    allowMarkers = true;
  }
  else if (StrContains(HTTP_req, "GET /") && StrContains(HTTP_req, ".htm")) {
    fileName = strtok(HTTP_req, "GET /");
    webFile = SD.open("/" + (String)fileName);
    if (webFile) {sendHtmlAnswer(cl);}
            else {webFile = SD.open(F("/404.htm"));}
    allowMarkers = true;
  }               
  else if (StrContains(HTTP_req, "GET /") && StrContains(HTTP_req, ".css")) {
    fileName = strtok(HTTP_req, "GET /");
    webFile = SD.open("/" + (String)fileName);
    if (webFile) {sendCssAnswer(cl);}
            else {sendErrorAnswer(fileName, cl);}
  }
  else if (StrContains(HTTP_req, "GET /") && StrContains(HTTP_req, ".js")) {
    fileName = strtok(HTTP_req, "GET /");
    webFile = SD.open("/" + (String)fileName);
    if (webFile) {sendJsAnswer(cl);}
            else {sendErrorAnswer(fileName, cl);}
  }
  else if (StrContains(HTTP_req, "GET /") && StrContains(HTTP_req, ".pde")) {
    fileName = strtok(HTTP_req, "GET /");
    webFile = SD.open("/" + (String)fileName);
    if (webFile) {sendJsAnswer(cl);}
            else {sendErrorAnswer(fileName, cl);}
  }  
  else if (StrContains(HTTP_req, "GET /") && (StrContains(HTTP_req, ".gif") ||
                                              StrContains(HTTP_req, ".jpg") ||
                                              StrContains(HTTP_req, ".png"))) {
    fileName = strtok(HTTP_req, "GET /");
    webFile = SD.open("/" + (String)fileName);
    if (webFile) {sendImgAnswer(cl);}
            else {sendErrorAnswer(fileName, cl);}
  }
  
  // Ajax XML requests
  if      (StrContains(HTTP_req, "request_dash"))    {sendXmlAnswer(cl); checkPage();         responseDash(cl);}
  else if (StrContains(HTTP_req, "request_generic")) {sendXmlAnswer(cl); SetLEDs();           responseGeneric(cl);}
  else if (StrContains(HTTP_req, "request_settings")){sendXmlAnswer(cl); setSettings();       responseSettings(cl);}
  else if (StrContains(HTTP_req, "request_themes"))  {sendXmlAnswer(cl); setThemes();         responseThemes(cl);}
  else if (StrContains(HTTP_req, "request_control")) {sendXmlAnswer(cl); setControl();        responseControl(cl);}
  else if (StrContains(HTTP_req, "req_el_control"))  {sendXmlAnswer(cl); setElectroControl(); responseElectroControl(cl);}
  else if (StrContains(HTTP_req, "req_el_freq"))     {sendXmlAnswer(cl); checkOscill();       responseElectroFreq(cl);}
  else if (StrContains(HTTP_req, "request_sdcard"))  {sendXmlAnswer(cl);                      responseSDcard(cl);}
  else if (StrContains(HTTP_req, "request_network")) {sendXmlAnswer(cl);                      responseNetwork(cl);}
  #ifdef FEATURE_NOO
  else if (StrContains(HTTP_req, "request_noo"))     {sendXmlAnswer(cl); setNoo();            responseNoo(cl);}
  #endif
  
} // parseRequest ( )

#ifdef FEATURE_CONTACTS
  String makeContacts() {
    String s = "";
    s += makeTag(baseContact, "1", String(cont1ObjState));
    s += makeTag(baseContact, "2", String(cont2ObjState));
    return s;
  }
#endif

#ifdef FEATURE_PIRS
  String makePirs() {
    String s = "";
    s += makeTag(basePir, "1", String(pir1));
    s += makeTag(basePir, "2", String(pir2));
    return s;
  }
#endif

void sendElectro(WiFiClient cl) {
  #ifdef FEATURE_ELECTRO
    char digits[14][3] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"};
    
    sendTagFloat("volt", "", ajaxUPrms[0], cl);

    for (int i = 1; i < MAX_UI_SENSORS; i++) {
      sendTagFloat(basePower, digits[i], ajaxUPrms[i], cl);
    }
  #endif
}

String makeModules() {
  String s = "";
  s += makeTag(F("mRtc"),      "", String(moduleTime));
  s += makeTag(F("mEthernet"), "", String(moduleEthernet));
  s += makeTag(F("mSd"),       "", String(moduleSd));
  s += makeTag(F("mServer"),   "", String(moduleServer));
  s += makeTag(F("mMajor"),    "", String(moduleMajor));
  s += makeTag(F("mUpload"),   "", String(moduleUpload));
  s += makeTag(F("mPirs"),     "", String(modulePirs));
  s += makeTag(F("mContacts"), "", String(moduleContacts));
  s += makeTag(F("mTemp"),     "", String(moduleTemp));
  s += makeTag(F("mElectro"),  "", String(moduleElectro));
  s += makeTag(F("mLeds"),     "", String(moduleLeds));
  s += makeTag(F("mKeys"),     "", String(moduleKeys));
  s += makeTag(F("mNoo"),      "", String(moduleNoo));
  return s;
}

String makeDigitalPorts() {
  String s = "";
  byte pins[] = {1,2,3,4,5, 12,13,14,15,16,17,18,19,21,22,23, 25,26,27, 32,33,34,35,36, 39};
  for (byte i = 0; i < sizeof(pins); i++) {
    s += makeTag(F("D"), String(pins[i]), String(digitalRead(pins[i])));
  }
  return s;
}

#ifdef FEATURE_TIME

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

#endif

String makeCpuLoad() {
  String s = "";
  s += makeTag("cycDelay", "", String(cyclosDelayP()));
  s += makeTag("cycInSec", "", String(cyclosInSecP));
  return s;
}

// HTTP Request

String makeHttpReq() {
  String s = "";
  for (int i = 0; i < reqIndex; i++) {
    if (HTTP_req[i] == '&') {s += ' ';}
                       else {s += HTTP_req[i];}
  }
  return makeTag("httpReq", "", s);
}

#ifdef FEATURE_PING
  void sendDevicesOnline(WiFiClient cl) {
    for (byte i = 0; i < TOTAL_NET_DEVICES; i++) {
      sendTagByte(netDevicesNames[i], "", online[i], cl);
    }
  }
#endif

/* Generic
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void SetLEDs() {
  if (StrContains(HTTP_req, "LED1=1")) {
    LED_state[0] = 1;
  }
  else if (StrContains(HTTP_req, "LED1=0")) {
    LED_state[0] = 0;
  }

  if (StrContains(HTTP_req, "LED2=1")) {
    LED_state[1] = 1;
  }
  else if (StrContains(HTTP_req, "LED2=0")) {
    LED_state[1] = 0;
  }

  if (StrContains(HTTP_req, "LED3=1")) {
    LED_state[2] = 1;
  }
  else if (StrContains(HTTP_req, "LED3=0")) {
    LED_state[2] = 0;
  }

  if (StrContains(HTTP_req, "LED4=1")) {
    LED_state[3] = 1;
  } 
  else if (StrContains(HTTP_req, "LED4=0")) {
    LED_state[3] = 0;
  }
} // SetLEDs()

void responseGeneric(WiFiClient cl) {
  int sw_arr[] = {12, 13, 14};

  String s = tagXmlVersion();
  s += "\n<inputs>\n";  
    s += makeHttpReq();
 
    for (byte i = 0; i < 6; i++) {
      s += makeTag("analog", "", String(analogRead(0)));
    }
    for (byte count = 0; count < 3; count++) {
      s += "<switch>";
        if (digitalRead(sw_arr[count])) {s += "ON";}
                                   else {s += "OFF";}
      s += "</switch>\n";
    }
    
    s += "<LED>"; if (LED_state[0]) {s += strChecked;} else {s += strUnChecked;} s += "</LED>\n";
    s += "<LED>"; if (LED_state[1]) {s += strChecked;} else {s += strUnChecked;} s += "</LED>\n";

    s += "<LED>"; if (LED_state[2]) {s += "on";} else {s += "off";} s += "</LED>\n";
    s += "<LED>"; if (LED_state[3]) {s += "on";} else {s += "off";} s += "</LED>\n";
  s += "</inputs>";
  cl.println(s);
} // responseGeneric( )

/* Network
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// WiFi.softAPmacAddress()
// WiFi.macAddress()
// WiFi.softAPIP().toString()
// WiFi.localIP()

void responseNetwork(WiFiClient cl) {
  String mac = WiFi.macAddress();
  IPAddress ip  = WiFi.localIP();
  String s = tagXmlVersion();
  s += "\n<inputs>\n";
    s += makeTag(baseMAC, "1", mac.substring(0, 2));
    s += makeTag(baseMAC, "2", mac.substring(3, 5));
    s += makeTag(baseMAC, "3", mac.substring(6, 8));
    s += makeTag(baseMAC, "4", mac.substring(9, 11));
    s += makeTag(baseMAC, "5", mac.substring(12, 14));
    s += makeTag(baseMAC, "6", mac.substring(15, 17));
    s += makeTag(baseIP, "1", String(ip[0]));
    s += makeTag(baseIP, "2", String(ip[1]));
    s += makeTag(baseIP, "3", String(ip[2]));
    s += makeTag(baseIP, "4", String(ip[3]));
    #ifdef FEATURE_PING
      sendDevicesOnline(cl);
    #endif    
  s += "</inputs>";
  cl.println(s);
}

/* Electro
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setElectroControl() {
  // Module & button Electro ON/OFF
/*  if (moduleElectro != MODULE_NOT_COMPILLED) {
    if (StrContains(HTTP_req, "btn1=1")) {
      buttonElectro = 1;
      moduleElectro = MODULE_ENABLE;
    } else if (StrContains(HTTP_req, "btn1=0")) {
      buttonElectro = 0;
      moduleElectro = MODULE_DISABLE;
    }
  }*/
}

void responseElectroControl(WiFiClient cl) {
 /* sendXmlVersion(cl);
  cl.println("<inputs>");
    sendTagByte("moduleElectro", "", moduleElectro, cl);
    
    cl.print("<LED>");
      if (buttonElectro) {cl.print("on");}
                    else {cl.print("off");}
    cl.println("</LED>");
    
  cl.println("</inputs>");*/
}

/* Settings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setSettings() {
  /* 
  // ...
  if (StrContains(HTTP_req, "LED1=1")) {
    //modeNetWork = MODE_MAJOR;
  } 
  else if (StrContains(HTTP_req, "LED1=0")) {
    //modeNetWork = MODE_ONE;
  }
  */
  
  // Set authMode
  if (StrContains(HTTP_req, "LED2=1")) {
    authMode = AUTH_ON;
  } 
  else if (StrContains(HTTP_req, "LED2=0")) {
    authMode = AUTH_OFF;
  }
} // setSettings()

void responseSettings(WiFiClient cl) {
  String s = tagXmlVersion();
  s += "\n<inputs>\n";
    /*
    cl.print("<LED>");
      if (modeNetWork == MODE_MAJOR) {cl.print(strChecked);} 
                                else {cl.print(strUnChecked);}
    cl.println("</LED>");
    */
    
    s += "<LED2>";
      if (authMode == AUTH_ON) {s += strChecked;} 
                          else {s += strUnChecked;}
    s += "</LED2>\n";
  s += "</inputs>";
  cl.println(s);
} // responseSettings( )

/* Themes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setThemes() {
  if      (StrContains(HTTP_req, "RD2=5")) {currentDesign = HOME_DESIGN;} 
  else if (StrContains(HTTP_req, "RD2=6")) {currentDesign = MODERN_DESIGN;}
  else if (StrContains(HTTP_req, "RD2=7")) {currentDesign = DEFAULT_DESIGN;}
  else if (StrContains(HTTP_req, "RD2=8")) {currentDesign = HACKER_DESIGN;}
  else if (StrContains(HTTP_req, "RD2=2")) {currentDesign = PART1_DESIGN;}
  else if (StrContains(HTTP_req, "RD2=1")) {currentDesign = PART2_DESIGN;}
  else if (StrContains(HTTP_req, "RD2=3")) {currentDesign = PART3_DESIGN;}
  else if (StrContains(HTTP_req, "RD2=4")) {currentDesign = PART4_DESIGN;}  
  else {}
}

void responseThemes(WiFiClient cl) {
  String s = tagXmlVersion();
  s += "\n<inputs>\n";  

    s += "<RD2>";
    switch (currentDesign) {
      case PART1_DESIGN:   s += '2'; break;
      case PART2_DESIGN:   s += '1'; break;
      case PART3_DESIGN:   s += '3'; break;
      case PART4_DESIGN:   s += '4'; break;
      case HOME_DESIGN:    s += '5'; break;
      case MODERN_DESIGN:  s += '6'; break;
      case DEFAULT_DESIGN: s += '7'; break;
      case HACKER_DESIGN:  s += '8'; break;
    }
    s += "</RD2>";
    
  s += "</inputs>";
  cl.println(s);
} // responseThemes( )

/* Control
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

byte light1 = 0;
byte light2 = 0;
byte light3 = 0;
byte light4 = 0;
byte light5 = 0;

void setControl() {
  if      (StrContains(HTTP_req, "light1=1")) {light1 = 1; red(50);} 
  else if (StrContains(HTTP_req, "light1=0")) {light1 = 0; black();}

  if      (StrContains(HTTP_req, "light2=1")) {light2 = 1; blue(50);} 
  else if (StrContains(HTTP_req, "light2=0")) {light2 = 0; black();}

  if      (StrContains(HTTP_req, "light3=1")) {light3 = 1; green(50);} 
  else if (StrContains(HTTP_req, "light3=0")) {light3 = 0; black();}

  if      (StrContains(HTTP_req, "light4=1")) {light4 = 1; magenta(50);} 
  else if (StrContains(HTTP_req, "light4=0")) {light4 = 0; black();}

  if      (StrContains(HTTP_req, "light5=1")) {light5 = 1; yellow(50);} 
  else if (StrContains(HTTP_req, "light5=0")) {light5 = 0; black();}     
}

void responseControl(WiFiClient cl) {
  String s = tagXmlVersion();
  s += F("\n<inputs>\n");
    s += makeTag(F("lite1"), "", String(light1));
    s += makeTag(F("lite2"), "", String(light2));
    s += makeTag(F("lite3"), "", String(light3));
    s += makeTag(F("lite4"), "", String(light4));
    s += makeTag(F("lite5"), "", String(light5));
  s += F("</inputs>\n");
  cl.print(s);
}

/* nooLite
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifdef FEATURE_NOO

byte const STATE_OFF = 0;
byte const STATE_ON = 1;
byte const STATE_UNKNOWN = 2;

byte const UNBIND = 0;
byte const BIND = 1;
byte const BIND_UNKNOWN = 2;

byte noo1state = STATE_OFF;
byte noo2state = STATE_OFF;

byte noo2multiCommand;

char *channelNum;

void setNoo() {
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
    Serial.print("command: ");
    Serial.println(command);
    Serial.print("action: ");
    Serial.println(bind);
  }

  byte commandCorrect;

  // noo2multi
  if (StrContains(HTTP_req, "noo2multi=")) {
    pos = request.indexOf("noo2multi=") + 10;
    commandStr = request.substring(pos, pos + 2);
    noo2multiCommand = commandStr.toInt();

    if (noo2multiCommand >= 10 && noo2multiCommand <= 90) {
      commandCorrect = byte(noo2multiCommand * 1.2 + 34);
      nooValue(NOO_CHANNEL_2, commandCorrect);
      Serial.print("(Multi) command: ");
      Serial.println(noo2multiCommand);
      Serial.print("(Multi) command correct: ");
      Serial.println(commandCorrect);      
    }    
  }
  
} // setNoo()

void responseNoo(WiFiClient cl) {
  sendXmlVersion(cl);
  cl.println("<inputs>");
    sendTagByte("noo1", "state", noo1state, cl);
    sendTagByte("noo2", "state", noo2state, cl);
    sendTagByte("noo2multi", "", noo2multiCommand, cl);
  cl.println("</inputs>");
}

#endif // FEATURE_NOO

/* Check page
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void checkPage() {
  currentPage = UNKNOWN_PAGE;
  if      (StrContains(HTTP_req, "settings-page")) {currentPage = SETTINGS_PAGE;}
  else if (StrContains(HTTP_req, "themes-page"))   {currentPage = THEMES_PAGE;}
  else if (StrContains(HTTP_req, "ports-page"))    {currentPage = PORTS_PAGE;}
  else if (StrContains(HTTP_req, "supply-page"))   {currentPage = SUPPLY_PAGE;}
  else if (StrContains(HTTP_req, "electro-page"))  {currentPage = ELECTRO_PAGE;}
}

/* Oscill
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void checkOscill() {
/*  if (StrContains(HTTP_req, "oscill")) {
    oscill = true;
  } else {
      oscill = false;
    }*/
}

void responseElectroFreq(WiFiClient cl) {
  sendXmlVersion(cl);

  cl.println("<inputs>");
    #ifdef FEATURE_ELECTRO
    for (int i = 0; i < MAX_FORM_POINTS; i++) {
      sendTagInt("u", "", pointsU[i], cl);
    }
    for (int i = 0; i < MAX_FORM_POINTS; i++) {
      sendTagInt("i", "", pointsI[i], cl);
    }    
    #endif 
  cl.println("</inputs>");
}

/* SD card
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
void responseSDcard(WiFiClient cl) {
  String s = tagXmlVersion();
  s += openInputs();
    s += makeTag("sdType",   "", String(SD.cardType()));
    s += makeTag("sdSize",   "", makeSdSize());
    //s += makeTag("sdRootDir","", s); // s.substring(0, 300)
  s += closeInputs();
  cl.println(s);
}

/* Dash
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void responseDash(WiFiClient cl) {
  String s = tagXmlVersion();
  s += "\n<inputs>\n";  
    s += makeModules();
    s += makeTag("auth",    "", String(authMode));
    s += makeTag("uptime",  "", uptimeString());
    s += makeTag("freeRAM", "", String(freeMem()));
    s += makeCpuLoad();
    s += makeTag("lifer",   "", String(lifer));
    s += makeTime();
    s += makeTag("day",     "", String(day()));
    s += makeTag("month",   "", String(month()));
    s += makeTag("weekday", "", String(weekday()));
    #ifdef FEATURE_TEMP
      for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
        s += makeTag(objSens[i], "", String(current_temp[i]));
      }
      //s += makeTag("mdTempHealth", "", String(tempModuleHealth));
    #endif
    #ifdef FEATURE_CONTACTS
      s += makeContacts();
    #endif
    #ifdef FEATURE_PIRS
      s += makePirs();
    #endif
    
    //sendTagByte("timeProv", "", timeProvider, cl);
    
    if (currentPage == PORTS_PAGE) {
      s += makeHttpReq();
      s += makeDigitalPorts();
      s += makeTag("A0",  "", String(analogRead(0)));
    }

    /*
    #ifdef FEATURE_ELECTRO
      sendTagFloat("voltage", "", ajaxUPrms[0], cl);
      sendTagFloat("power",   "", ajaxUPrms[1], cl);
    #endif  
    
    if (currentPage == SUPPLY_PAGE) {
      #ifdef FEATURE_ELECTRO
        sendTagFloat("voltage", "", ajaxUPrms[0],   cl);
        sendTagFloat("power",   "", ajaxUPrms[1],   cl);
        sendTagLong("period",   "", periodInMicros, cl);
      #endif 
    }

    if (currentPage == ELECTRO_PAGE) {
      #ifdef FEATURE_ELECTRO
        sendTagByte("moduleElectro", "", moduleElectro, cl);
        sendElectro(cl);
      #endif 
    }
    */
  s += "</inputs>";
  cl.println(s);
}

