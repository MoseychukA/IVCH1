/*
  Module Contacts
  part of Arduino Mega Server project
*/

#ifdef CONTACTS_FEATURE

#define CONT1_PIN 27
#define CONT2_PIN 31

#define OPEN  1
#define CLOSE 0

char cont1object[] = "cont1";
char cont2object[] = "cont2";

char cont1openMess[] = "Open"; char cont1closeMess[] = "Closed";
char cont2openMess[] = "Down"; char cont2closeMess[] = "Norm";

// states
// 0 - open
// 1 - close
// 2 - not defined
byte cont1state = 2; byte old_cont1state = 2; byte cont1objectState = 2;
byte cont2state = 2; byte old_cont2state = 2; byte cont2objectState = 2;

void contactsInit() {
  pinMode(CONT1_PIN, INPUT);
  pinMode(CONT2_PIN, INPUT);
  moduleContacts = MODULE_ENABLE;
  started("Contacts");
}

void printContMess(char obj[], char mess[]) {
  timeStamp(); Serial.print(obj); Serial.print(F(": ")); Serial.println(mess);
}

void contactsWorks() {
  // cont1
  cont1state = digitalRead(CONT1_PIN);
  if (cont1state != old_cont1state) {
      old_cont1state = cont1state;
      if (cont1state == 0) {
        cont1objectState = OPEN;
        printContMess(cont1object, cont1openMess);
      } else {
        cont1objectState = CLOSE;
        printContMess(cont1object, cont1closeMess);
      }
  }
  
  // cont2
  cont2state = digitalRead(CONT2_PIN);
  if (cont2state != old_cont2state) {
    old_cont2state = cont2state;
    if (cont2state == 1) {
      cont2objectState = OPEN;
      printContMess(cont2object, cont2openMess);
    } else {
        cont2objectState = CLOSE;
        printContMess(cont2object, cont2closeMess);
      }
  }
} // contactsWorks()

#endif // CONTACTS_FEATURE
