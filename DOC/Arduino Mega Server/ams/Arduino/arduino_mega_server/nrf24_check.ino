/*
  Module nRF24 Check
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

void resetSender(struct packet *p) {
  p->active   = false;
  p->required = true;
  p->success  = true;
  p->tryouts  = 0;
  p->type     = NOT_DEF;
  setNextNumber(p);
}

// Echo

bool matchEcho(struct packet *p) {
  if (rxNumber != p->number) {
    return false;
  }
  return true;
}

void checkEcho(struct packet *p) {
  if (matchEcho(p)) {
    resetSender(p);
    successPackets++;
    Serial.print(F("ECHO: "));
    Serial.print(p->command);
    Serial.println(F(" DONE"));
  }
}

void parseEcho() {
  switch (rxCommand) {
    case GET_NAME:      checkEcho(&pktGetName);     break;
    case GET_ID:        checkEcho(&pktGetId);       break;
    case GET_ADDR:      checkEcho(&pktGetAddress);  break;
    case GET_FREE_MEM:  checkEcho(&pktGetFreeMem);  break;
    case GET_BATTERY:   checkEcho(&pktGetBattery);  break;
    case GET_SELF_TEMP: checkEcho(&pktGetSelfTemp); break;
    case GET_HEALTH:    checkEcho(&pktGetHealth);   break;
    case GET_TEMP_1:    checkEcho(&pktGetTemp1);    break;    
  }
}

// Answers

void checkAnswer(struct packet *p) {
  resetSender(p);
  successPackets++;
  Serial.print(F("ANSWER: "));
  Serial.print(p->command);
  Serial.println(F(" DONE"));
}

void parseAnswers() {
  switch (rxCommand) {
    case DTA_NAME:
      checkAnswer(&pktGetName);
      nrf1Name = pktGetName.result;
      break;
    case DTA_ID:
      checkAnswer(&pktGetId);
      nrf1Id = pktGetId.result;
      break;
    case DTA_ADDR:
      checkAnswer(&pktGetAddress);
      nrf1Addr = pktGetAddress.result.toInt();
      break;
    case DTA_FREE_MEM:
      checkAnswer(&pktGetFreeMem);
      nrf1FreeMem = pktGetFreeMem.result.toInt();
    case DTA_BATTERY:
      checkAnswer(&pktGetBattery);
      nrf1Battery = pktGetBattery.result.toInt();
      break;
    case DTA_SELF_TEMP:
      checkAnswer(&pktGetSelfTemp);
      nrf1SelfTemp = pktGetSelfTemp.result.toFloat();
      break;
    case DTA_HEALTH:
      checkAnswer(&pktGetHealth);
      nrf1Health = pktGetHealth.result.toInt();
      break;
    case DTA_TEMP_1:
      checkAnswer(&pktGetTemp1);
      nrf1Temp1 = pktGetTemp1.result.toFloat();
      break;
  }
}

#endif // NRF24_FEATURE
