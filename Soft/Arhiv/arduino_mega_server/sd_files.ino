/*
  Module SD / Files
  part of Arduino Mega Server project
*/

#ifdef SD_FILES_FEATURE

void sdFilesInit() {
  moduleSdFiles = MODULE_ENABLE;
  started("SD Files");
}

// Print

void printSdContent(File dir, int numTabs) {
  while(true) {
    File entry = dir.openNextFile();
    if (! entry) {
      dir.rewindDirectory();
      break;
    } // no more files
    for (uint8_t i = 0; i < numTabs; i++) {Serial.print('\t');}
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
} // printSdContent( )

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

// String

String stringDirectory(File dir) {
  String str = "";
  File entry;
  dir.rewindDirectory();
  while(true) {
    entry = dir.openNextFile();
    if (! entry) { // no more files
      dir.rewindDirectory();
      return str;
      break;
    }
    if (!entry.isDirectory()) {
      str += entry.name();
      str += " ";
    } 
    entry.close();
  }
}

#endif // SD_FILES_FEATURE
