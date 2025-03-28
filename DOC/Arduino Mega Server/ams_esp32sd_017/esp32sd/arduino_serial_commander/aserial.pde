/*
  Modul Serial Operations
  part of Arduino Serial Commander
  part of Arduino Mega Server project  
*/

// stream markers
String START_TRANSFER_FILENAME = "FPP";
String END_TRANSFER_FILENAME   = "FTT";
String START_TRANSFER_FILE     = "GOO";
String END_TRANSFER_FILE       = "?Z?";
String START_ERROR_PROTECTION  = "E1R";
String END_ERROR_PROTECTION    = "E2R";
String LED_ON                  = "LD1";
String LED_OFF                 = "LD0";
String TIME_SYNC_MARKER        = "T0S";

void serialInit() {
  // Show serial ports
  //println(Serial.list());
  //port = new Serial(this, Serial.list()[0], COM_SPEED);
  port = new Serial(this, COM_PORT, COM_SPEED);
}

void sendStartTransfer() {
  port.write(START_TRANSFER_FILENAME);
  port.write(currentFile);
  port.write(END_TRANSFER_FILENAME);
  port.write(START_TRANSFER_FILE);
  port.write('\n');
  delay(50);
}  

void sendEndTransfer() {
  port.write(END_TRANSFER_FILE);
}

void sendErrorProtection() {
  port.write(START_ERROR_PROTECTION);
  port.write('\n');
  delay(100);  // 140
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 10; j++) {
      port.write(j);
      delay(1);
    }
    port.write('\n');
    delay(100); // 140
  }
  port.write(END_ERROR_PROTECTION);
  port.write('\n');
  delay(40);
  protect = true;
} 

void transfer() {
  int blockBinary = currentBinary + 64;
  if (blockBinary > binarys.length) {blockBinary = binarys.length;}
  
  for (int i = currentBinary; i < blockBinary; i++) {
    if (binarys[i] == 10 && !protect) {sendErrorProtection();}

    port.write(binarys[i]);
    
    // errors protection (optional)
    if (currentBinary % 2 == 0) {
      delay(1);
    }
    
    currentBinary++;
  }
}

void sendTime(long time) {  
  String timeStr = String.valueOf(time);
  port.write(TIME_SYNC_MARKER);
  port.write(timeStr);
  port.write('\n');  
}

