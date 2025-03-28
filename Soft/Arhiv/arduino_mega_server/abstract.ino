/*
  Module Abstract
  part of Arduino Mega Server project
*/

void abstractInit() {
  //moduleAbstract = MODULE_ENABLE;
  started("Abstract");
}

// Print Init

void printStart() {
  Serial.println();
  Serial.print(SELF_NAME); Serial.print(F(" (")); Serial.print(VERSION); Serial.println(F(") started..."));
}

void printInitDone() {
  timeStamp(); Serial.print(F("GLOBAL Init DONE (")); Serial.print(millis() / 1000); Serial.println(F("s)"));
  Serial.println();
  timeStamp(); Serial.print(F("AMS WORK ")); printFreeMem(); Serial.println();
}

// Abstract Works

void abstractWorks() {
  if (cycle1m) {
    timeStamp(); printFreeMem(); Serial.println();
  }
  if (serialDynamic) {print1s();}
}

// End Loop Works

void endLoopWorks() {
  cyclosInSecWork();
  eraseCyclos();
  profCalc();
  cyclosDelayWork();
}

