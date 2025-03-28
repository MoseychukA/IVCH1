/*
  Modul Contacts
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

#ifdef CONTACTS_FEATURE

byte const CONT_1_PIN = 5;

#ifdef SERIAL_PRINT
  // strings
  char CONT_1_NAME[]  = "contact1";
  char CONT_1_OPEN[]  = "Open";
  char CONT_1_CLOSE[] = "Closed";
#endif

// states
byte const CLOSE       = 0;
byte const OPEN        = 1;
byte const NOT_DEFINED = 2;

byte cont1state     = NOT_DEFINED;
byte old_cont1state = NOT_DEFINED;

void contactsInit() {
  pinMode(CONT_1_PIN, INPUT);
}

#ifdef SERIAL_PRINT
  void printContact(char obj[], char mess[]) {
    Serial.print(obj);
    Serial.print(F(": "));
    Serial.println(mess);
  }
#endif
  
void contactsWorks() {
  cont1state = digitalRead(CONT_1_PIN);

  if (cont1state != old_cont1state) {
      old_cont1state = cont1state;
      
      if (cont1state == 0) {
        #ifdef SERIAL_PRINT
          printContact(CONT_1_NAME, CONT_1_OPEN);
        #endif
      } else {
          #ifdef SERIAL_PRINT
            printContact(CONT_1_NAME, CONT_1_CLOSE);
          #endif
        }
  }
} // contactsWorks()

#endif // CONTACTS_FEATURE
