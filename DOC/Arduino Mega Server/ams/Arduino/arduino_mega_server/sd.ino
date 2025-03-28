/*
  Module SD
  part of Arduino Mega Server project
*/

#define SD_CHIP_SELECT 4

File sdroot;
int countRoot  = 0;
int emptyFiles = 0;
long sizeRoot  = 0;

void sdCardInit() {
  Serial.println("Module SD...");
  
  Serial.print(F(" SD init... "));
  if (SD.begin(SD_CHIP_SELECT)) {
    Serial.println(F("OK"));
    
    if (SD.exists("index.htm")) {
      Serial.println(F(" index.htm... found"));
    } else {
        Serial.println(F(" Can't find index.htm"));
        return;      
      }
  } else {
      Serial.println(F("failed"));
      return;
    }
 
  sdroot = SD.open("/");
  checkDir(sdroot);
  Serial.print(F(" Files: ")); Serial.println(countRoot);
  Serial.print(F(" Size:  ")); Serial.println(sizeRoot);
  if (emptyFiles) {
    Serial.print(F(" Empty: ")); Serial.println(emptyFiles);
  }

  initDone();
} // sdCardInit()

void checkDir(File dir) {
  countRoot  = 0;
  sizeRoot   = 0;
  emptyFiles = 0;
  Serial.print(F(" Check"));
  dir.rewindDirectory();
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {break;}
    countRoot++;
    if (countRoot %  10 == 0) {Serial.print(F("."));}
    if (countRoot % 400 == 0) {Serial.println();}
    if (entry.size() == 0) {emptyFiles++;}
    sizeRoot += entry.size();
    entry.close();
  }
 Serial.println();
}

