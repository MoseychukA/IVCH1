/*
  Module Abstract
  part of Arduino Mega Server project
*/

void initAbstract() {

}

void workAbstract() {
  if (cycle5s) {
    //yellowControl();
    //greenControl();
  }
  if (cycle1s) {
    wellowOff();
    selfControl();
    setLifer();
    //hallSensorWorks();
    //Serial.print(F("Self temp: ")); Serial.print(selfTemp(), 1);  Serial.println(" C");
  }
  if (cycle1m) {
    //timeStamp(); printFreeMem("");
  }
  //analogShow();
}

void workLoopEnd() {
  cyclosInSecWork();
  cyclosDelayWork();
  eraseCyclos();
}

