/*
  Module SD
  part of Arduino Mega Server project
*/

#define SD_CHIP_SELECT 5

int countRoot  = 0;
int emptyFiles = 0;
long sizeRoot  = 0;

void initSd() {
  initStart(F("SD"), false);

  Serial.print(F(" Init:  "));
  if (SD.begin(SD_CHIP_SELECT)) {
    Serial.println(F("OK"));
  } else {
      Serial.println(F("failed"));
      return;
    }

  printMeasure(F(" Type:  "), makeSdType(), "");
  printMeasure(F(" Size:  "), makeSdSize(), F(" MB"));

  File sdroot = SD.open("/");
  checkDir(sdroot);
  printMeasure(F(" Files: "), String(countRoot), "");
  printMeasure(F(" Total: "), String(sizeRoot), F(" B"));
  if (emptyFiles) {
  printMeasure(F(" Empty: "), String(emptyFiles), "");
  }
  Serial.print(F(" Index: "));
  if (SD.exists(F("/index.htm"))) {
    Serial.println(F("found"));
  } else {
      Serial.println(F("can't find"));
      return;      
    }

  moduleSd = ENABLE;
  initDone(false);
} // sdInit()


String makeSdType() {
  switch(SD.cardType()) {
    case CARD_NONE: return F("Not attached"); break;
    case CARD_MMC:  return F("MMC");          break;
    case CARD_SD:   return F("SDSC");         break;
    case CARD_SDHC: return F("SDHC");         break;
           default: return F("Unknown");
  } 
}

String makeSdSize() {
  uint64_t size = SD.cardSize() / (1024 * 1024);
  return String((long)size);
}

void printSdSize() {
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void checkDir(File dir) {
  countRoot  = 0;
  sizeRoot   = 0;
  emptyFiles = 0;
  Serial.print(F(" Check: "));
  dir.rewindDirectory();
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {break;}
    countRoot++;
    if (countRoot %  10 == 0) {Serial.print(F("."));}
    if (countRoot % 400 == 0) {Serial.print(F("\n"));}
    if (entry.size() == 0) {emptyFiles++;}
    sizeRoot += entry.size();
    entry.close();
  }
 Serial.print(F("\n"));
}

void printSdContent(File dir, int numTabs) {
   while(true) {
     File entry = dir.openNextFile();
     if (!entry) {
       dir.rewindDirectory();
       break;
     } // no more files
     for (uint8_t i = 0; i < numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printSdContent(entry, numTabs + 1);
     } else {
         Serial.print("\t");
         Serial.println(entry.size(), DEC);
       }
     entry.close();
   }
}  // printSdContent( )

void printDirectory(File dir) {
   while(true) {
     File entry = dir.openNextFile();
     if (!entry) {break;} // no more files
     if (!entry.isDirectory()) {
       Serial.println(entry.name());
     } 
     entry.close();
   }
}

String makeDirectory(File dir) {
  String s = "";
  File entry;
  dir.rewindDirectory();
  while(true) {
    entry = dir.openNextFile();
    if (!entry) { // no more files
      dir.rewindDirectory();
      return s;
      break;
    }
    if (! entry.isDirectory()) {
    s += entry.name();
      s += " ";
    } 
    entry.close();
  }
} // makeDirectory( )

/*
// Files Manipulations

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels -1);
      }
    } else {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.println(file.size());
      }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
      Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
      Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
      Serial.println("Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
    Serial.println("Message appended");
  } else {
      Serial.println("Append failed");
    }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
      Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
      Serial.println("Failed to open file for reading");
    }


  file = fs.open(path, FILE_WRITE);
  if(!file){
      Serial.println("Failed to open file for writing");
      return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

void filesManipulation() {
  listDir(SD, "/", 0);
  createDir(SD, "/mydir");
  listDir(SD, "/", 0);
  removeDir(SD, "/mydir");
  listDir(SD, "/", 2);

  writeFile(SD, "/hello.txt", "Hello ");
  appendFile(SD, "/hello.txt", "World!\n");
  readFile(SD, "/hello.txt");
  deleteFile(SD, "/foo.txt");

  renameFile(SD, "/hello.txt", "/foo.txt");
  readFile(SD, "/foo.txt");
  
  testFileIO(SD, "/test.txt");

  listDir(SD, "/", 0);
}
*/
