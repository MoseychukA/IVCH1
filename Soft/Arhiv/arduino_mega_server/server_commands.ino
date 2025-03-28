/*
  Module Netwotk Commands
  part of Arduino Mega Server project

  Key ON:  rele4=1
  Key OFF: rele4=.

  Night mode ON:  night=1
  Night mode OFF: night=.

  Color LEDS: color=white, black, red, blue, green, yellow, magenta, cyan
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

#ifdef LEDS_FEATURE
  void setNightLed() {
    if (command.indexOf(F("night")) >= 0) {
      if (parameter.indexOf(F("1")) >= 0) {
        modeLED = EMPTY;
      } else {
           modeLED = LED_PIR;
        }
    }
  }

  void setColorLed() {
    if (command.indexOf(F("color")) >= 0) {
      if (parameter.indexOf(F("black"))   >= 0) {black   (led1);}
      if (parameter.indexOf(F("white"))   >= 0) {white   (led1, 255);}
      if (parameter.indexOf(F("red"))     >= 0) {red     (led1, 255);}
      if (parameter.indexOf(F("blue"))    >= 0) {blue    (led1, 255);}
      if (parameter.indexOf(F("green"))   >= 0) {green   (led1, 255);}                            
      if (parameter.indexOf(F("yellow"))  >= 0) {yellow  (led1, 255);}                            
      if (parameter.indexOf(F("magenta")) >= 0) {magenta (led1, 255);}                            
      if (parameter.indexOf(F("cyan"))    >= 0) {cyan    (led1, 255);}
    }
  }
#endif // LEDS_FEATURE

/* ----------------------------------------
  Function parseCommands(EthernetClient cl)
    Parse network commands
------------------------------------------- */

void parseCommands(EthernetClient cl) {
  int posBegin;
  int posEnd;
  int posParam;
  
  if (request.indexOf(F("?")) >= 0) {
    posBegin = request.indexOf(F("?")) + 1;
    posEnd = request.indexOf(F("HTTP"));

    if (request.indexOf(F("=")) >= 0) {
       posParam = request.indexOf(F("="));
       command = request.substring(posBegin, posParam);              
       parameter = request.substring(posParam + 1, posEnd - 1);              
    } else {
        command = request.substring(posBegin, posEnd - 1);              
        parameter = "";
      }

    Serial.print(F("command: "));   Serial.println(command);
    Serial.print(F("parameter: ")); Serial.println(parameter);
          
    setKey();
    
    #ifdef LEDS_FEATURE
      setNightLed();
      setColorLed();
    #endif
    
    // erase request
    request = "";
  } // if (url.indexOf(F("?")) >= 0)
} // parseCommands

