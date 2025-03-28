/*
  Module Serial Upload
  part of Arduino Mega Server project
*/

#ifdef FEATURE_UPLOAD

// Serial stream markers
#define START_FILENAME_MARKER         "FPP"
#define STOP_FILENAME_MARKER          "FTT"
#define START_TRANSFER_MARKER         "GOO"
#define STOP_TRANSFER_MARKER          "?Z?"
#define START_ERROR_PROTECTION_MARKER "E1R"
#define STOP_ERROR_PROTECTION_MARKER  "E2R"
#define LED_ON                        "LD1"
#define LED_OFF                       "LD0"
#define TIME_SYNC_MARKER              "T0S"

char buffer[4]        = ""; // for markers
char buffer2[4]       = ""; // for clear buffer (!)
char buffer_spaces[4] = {' ', ' ', ' '};
char fn[20]           = "";
char fn_temp[20]      = "";
char fn_empty[20]     = "";
long countFirst       = 0;
File saveFile;

// logic
boolean name         = false;
boolean start        = false;
boolean done         = false;
boolean answer       = false;
boolean answer_short = false;
boolean skipTransfer = false;

void initUpload() {
  moduleUpload = ENABLE;
  started(F("Upload"), true);
}

void addFN(char c) {
  fn[19] = fn[18];
  fn[18] = fn[17];
  fn[17] = fn[16];
  fn[16] = fn[15];  
  fn[15] = fn[14];
  fn[14] = fn[13];
  fn[13] = fn[12];
  fn[12] = fn[11];
  fn[11] = fn[10];
  fn[10] = fn[9];
  fn[9] = fn[8];
  fn[8] = fn[7];
  fn[7] = fn[6];
  fn[6] = fn[5];
  fn[5] = fn[4];
  fn[4] = fn[3];
  fn[3] = fn[2];
  fn[2] = fn[1];
  fn[1] = fn[0];
  fn[0] = c;
}

void correctFN() {
  fn[19] = fn[18];
  fn[18] = fn[17];
  fn[17] = fn[16];
  fn[16] = fn[15];  
  fn[15] = fn[14];
  fn[14] = fn[13];
  fn[13] = fn[12];
  fn[12] = fn[11];
  fn[11] = fn[10];
  fn[10] = fn[9];
  fn[9] = fn[8];
  fn[8] = fn[7];
  fn[7] = fn[6];
  fn[6] = fn[5];
  fn[5] = fn[4];
  fn[4] = fn[3];
  fn[3] = fn[2];
  fn[2] = fn[1];
  fn[1] = fn[0];
  fn[0] = '/';
}

void copyFN() {
  for (int i = 0; i < 20; i++) {
    fn_temp[i] = fn[i];
  }
}

void reversFN() {
  for (int i = 18; i >= 0; i--) {
    if (fn[i] == fn[19]) {
      continue;
    } else {
        for (int j = 0; j <= i; j++) {
          fn[j] = fn_temp[i - j];
        }
        break;
      }
  }
}

void clearFN() {
  for (int i = 0; i < 20; i++) {
    fn[i] = fn_empty[i];
    fn_temp[i] = fn_empty[i];
  }
}

void clearFNonly() {
  for (int i = 0; i < 20; i++) {
    fn[i] = fn_empty[i];
  }
}

// Markers

void addBuffer(char c) {
  buffer[2] = buffer[1];
  buffer[1] = buffer[0];
  buffer[0] = c;
}

void clearBuffer() {
  buffer[0] = buffer2[0];
  buffer[1] = buffer2[1];
  buffer[2] = buffer2[2];
}

void clearBuffer_spaces() {
  buffer[0] = buffer_spaces[0];
  buffer[1] = buffer_spaces[1];
  buffer[2] = buffer_spaces[2];
}

boolean checkBuffer(char s[]) {
  if (buffer[2] == s[0] && buffer[1] == s[1] && buffer[0] == s[2]) {
    return true;
  } else {
      return false;
    }
}

void replaceBuffer(char s[]) {
  buffer[2] = s[0];
  buffer[1] = s[1];
  buffer[0] = s[2];
}

char getBuffer(char c) {
  char temp = buffer[2];
  
  buffer[2] = buffer[1];
  buffer[1] = buffer[0];
  buffer[0] = c;
  
  return temp;
}

void workUpload() {
  char incomingByte; // for read incoming serial data
  
  // Time Sync
  if (checkBuffer(TIME_SYNC_MARKER)) {
    clearBuffer();
    unsigned long pctime = 0L;
    const unsigned long DEFAULT_TIME = 1443685000; // 01.10.15
    pctime = Serial.parseInt();
    if (pctime > DEFAULT_TIME) {
      Serial.print(F("OK, received: "));
      Serial.println(pctime);
      time_t t = pctime;
      //RTC.set(t);
      ////////////setSyncProvider(RTC.get); 

      if (timeStatus() != timeSet) {
        Serial.println(F("Unable to sync with the RTC"));
      } else {
          Serial.println(F("RTC has set the system time"));    
        }
    } else {
        Serial.print(F("ERROR, received: "));
        Serial.println(pctime);
      }
  }
  
  if (checkBuffer(LED_ON)) {
    clearBuffer();
    digitalWrite(6, HIGH);
    Serial.print(F("OK, received: "));
    Serial.println(LED_ON);
  }
  
  if (checkBuffer(LED_OFF)) {
    clearBuffer();
    digitalWrite(6, LOW);
    Serial.print(F("OK, received: "));
    Serial.println(LED_OFF);
  }

  // check START_FILENAME_MARKER and set mode "name"
  if (checkBuffer(START_FILENAME_MARKER)) {
    clearBuffer();
    name = true;
    blue(50);
    modeWork = MODE_UPDATE;
  } 
    
  // check STOP_FILENAME_MARKER and open file on SD-card to write
  if (checkBuffer(STOP_FILENAME_MARKER)) {
    clearBuffer();
    name = false;
    copyFN();
    reversFN();
    correctFN();
    if (SD.exists(fn)) {SD.remove(fn);}
    saveFile = SD.open(fn, FILE_WRITE);
  }

  // check START_ERROR_PROTECTION_MARKER and set mode "skipTransfer"
  if (checkBuffer(START_ERROR_PROTECTION_MARKER)) {
    clearBuffer();
    skipTransfer = true;
    start = false;
  }  

  // check STOP_ERROR_PROTECTION_MARKER and continue transfer
  if (checkBuffer(STOP_ERROR_PROTECTION_MARKER)) {
    clearBuffer_spaces();
    skipTransfer = false;
    start = true;
  }   
 
  if (checkBuffer(START_TRANSFER_MARKER)) {
    clearBuffer_spaces();
    start = true;
    countFirst = 0;
    done = false;
    modeWork = MODE_UPDATE;
  }
  
  if (checkBuffer(STOP_TRANSFER_MARKER)) {
    clearBuffer();
    done = true;
    start = false;
  }  
  
  if (Serial.available() > 0) {
    incomingByte = getBuffer(Serial.read());
    //Serial.write(incomingByte);
    if (name) {addFN(incomingByte);}
    
    if (start) {
      if (saveFile) {
        countFirst++;
        if (countFirst > 4) {
          if (!skipTransfer) {
            saveFile.print(incomingByte);
          }
        }
      } else {
         //Serial.println("error opening file");
        }
    } // if (start)
  } // if (Serial.available() > 0) 
  
  if (done) {
    saveFile.close();
    copyFN();
    clearFNonly();
    done = false;
    skipTransfer = false;
    answer_short = true;
    black();
  }
  
  if (answer_short) {
    Serial.print(F("\n[")); Serial.print(fn_temp); Serial.println(F("]")); Serial.println(F("OK"));
    answer_short = false;
    modeWork = MODE_SERVER;
  }
} // uploadWorks()

#endif // FEATURE_UPLOAD
