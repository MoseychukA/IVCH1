/*
  Module Server Commands
  part of Arduino Mega Server project

  Key ON:  rele4=1
  Key OFF: rele4=.

  Night mode ON:  night=1
  Night mode OFF: night=.

  Color LEDS: color=white (black, red, blue, green, yellow, magenta, cyan)
*/

String command;
String parameter;

void setKey() {
  if (command.indexOf(F("rele4")) >= 0) {
    if (parameter.indexOf(F("1")) >= 0) {
      //setGoLightON();
    } else {
        //setGoLightOFF();
      }
  }
}

#ifdef FEATURE_LEDS
  void setNightLed() {
    if (command.indexOf(F("night")) >= 0) {
      if (parameter.indexOf(F("1")) >= 0) {
        modeLed = LED_EMPTY;
      } else {
           modeLed = LED_PIR;
        }
    }
  }

  void setColorLed() {
    if (command.indexOf(F("color")) >= 0) {
      if (parameter.indexOf(F("black"))   >= 0) {black  ();}
      if (parameter.indexOf(F("white"))   >= 0) {white  (50);}
      if (parameter.indexOf(F("red"))     >= 0) {red    (50);}
      if (parameter.indexOf(F("blue"))    >= 0) {blue   (50);}
      if (parameter.indexOf(F("green"))   >= 0) {green  (50);}                            
      if (parameter.indexOf(F("yellow"))  >= 0) {yellow (50);}                            
      if (parameter.indexOf(F("magenta")) >= 0) {magenta(50);}                            
      if (parameter.indexOf(F("cyan"))    >= 0) {cyan   (50);}
    }
  }
#endif // FEATURE_LEDS

void parseCommands(WiFiClient cl) {
  int posBegin;
  int posEnd;
  int posParam;
  
  if (request.indexOf("?") >= 0) {
    posBegin = request.indexOf("?") + 1;
    posEnd = request.indexOf("HTTP");

    if (request.indexOf("=") >= 0) {
       posParam = request.indexOf("=");
       command = request.substring(posBegin, posParam);              
       parameter = request.substring(posParam + 1, posEnd - 1);              
    } else {
        command = request.substring(posBegin, posEnd - 1);              
        parameter = "";
      }

    Serial.print(F("command:   ")); Serial.println(command);
    Serial.print(F("parameter: ")); Serial.println(parameter);
          
    setKey();
    
    #ifdef FEATURE_LEDS
      setNightLed();
      setColorLed();
    #endif
    
    // erase request
    request = "";
  } // if (request.indexOf("?") >= 0)
} // parseCommands

