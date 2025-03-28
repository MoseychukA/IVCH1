/*
  Modul nRF24 Check
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

void resetSender(struct Sender *p) {
  p->active   = false;
  p->required = true;
  p->success  = true;
  p->tryouts  = 0;
  p->type     = NOT_DEF;
  setNextNumber(p);
}

// Answers

void checkAnswer(struct Sender *p) {
  resetSender(p);
  successPackets++;
  #ifdef SERIAL_PRINT
    Serial.print(F("ANSWER: "));
    Serial.print(p->command);
    Serial.println(F(" DONE"));
  #endif
}

void parseAnswers() {
  switch (rxCommand) {
    case DTA_NAME:       checkAnswer(&sendName);     break;
    case DTA_ID:         checkAnswer(&sendId);       break;
    case DTA_ADDR:       checkAnswer(&sendAddress);  break;
    case DTA_FREE_MEM:   checkAnswer(&sendFreeMem);  break;
    case DTA_BATTERY:    checkAnswer(&sendBattery);  break;
    case DTA_SELF_TEMP:  checkAnswer(&sendSelfTemp); break;
    case DTA_HEALTH:     checkAnswer(&sendHealth);   break;
    case DTA_TEMP_1:     checkAnswer(&sendTemp1);    break;
  }
}

// Echo

boolean matchEcho(struct Sender *p) {
  if (rxNumber != p->number) {
    return false;
  }
  return true;
}

void checkEcho(struct Sender *p) {
  if (matchEcho(p)) {
    resetSender(p);
    successPackets++;
    #ifdef SERIAL_PRINT
      Serial.print(F("ECHO: "));
      Serial.print(p->command);
      Serial.println(F(" DONE"));
    #endif
  }
}

void parseEcho() {
  switch (rxCommand) {
    case BAK_ECHO:      checkEcho(&sendEcho);     break;
    case DTA_NAME:      checkEcho(&sendName);     break;
    case DTA_ID:        checkEcho(&sendId);       break;
    case DTA_ADDR:      checkEcho(&sendAddress);  break;
    case DTA_FREE_MEM:  checkEcho(&sendFreeMem);  break;
    case DTA_BATTERY:   checkEcho(&sendBattery);  break;
    case DTA_SELF_TEMP: checkEcho(&sendSelfTemp); break;
    case DTA_HEALTH:    checkEcho(&sendHealth);   break;
    case DTA_TEMP_1:    checkEcho(&sendTemp1);    break; 
  }
}

#endif // NRF24_FEATURE

