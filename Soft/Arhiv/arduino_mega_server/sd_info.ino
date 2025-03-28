/*
  Module SD / Info
  part of Arduino Mega Server project
*/

#ifdef SD_INFO_FEATURE

Sd2Card  card;
SdVolume volume;
SdFile   root;

void sdInfoInit() {
  Serial.println(F("Module SD Info... "));
  if (card.init(SPI_FULL_SPEED, SD_CHIP_SELECT)) {
    printSdType();
    printFsType();
    printSdSize();
    //printSdRoot();
    moduleSdInfo = MODULE_ENABLE;
    initDone();
  } else {
      Serial.println(F("failed"));
    }
}

// SD info
  
byte sdType() {
  switch(card.type()) {
    case SD_CARD_TYPE_SD1:  return 1; break;
    case SD_CARD_TYPE_SD2:  return 2; break;
    case SD_CARD_TYPE_SDHC: return 3; break;
                   default: return 0;
  }
}

byte sdFileSystem() {
  if (volume.init(card)) {return volume.fatType();}
                    else {return 0;}
}

int sdVolumeSize() {
  int k = 1000; // 1000 or 1024
  if (volume.init(card)) {
    double volumesize;
    volumesize = volume.blocksPerCluster();
    volumesize *= volume.clusterCount();
    volumesize *= 512;      // size in bytes
    volumesize /= k;        // size in Kbytes
    return volumesize /= k; // size in Mbytes
  } else {
      return 0;
    }
}

// Print

bool sdIsInit() {
  // try to open the partition FAT16 or FAT32
  if (volume.init(card)) {
    return true;
  } else {
      Serial.println(F("Not find partition"));
      return false;
    }
}
  
void printSdType() {
  Serial.print(F(" SD type:    "));
  switch(card.type()) {
    case SD_CARD_TYPE_SD1:  Serial.print(F("SD1")); break;
    case SD_CARD_TYPE_SD2:  Serial.print(F("SD2")); break;
    case SD_CARD_TYPE_SDHC: Serial.print(F("SDHC"));break;
                   default: Serial.print(F("Unknown"));
  }
  Serial.println();
}

void printFsType() {
  // type of the first FAT-type volume
  if (sdIsInit()) {
    Serial.print(F(" Filesystem: FAT"));
    Serial.println(volume.fatType(), DEC);
  }
}
  
void printSdSize() {
  int k = 1000; // 1000 or 1024
  // try to open the partition FAT16 or FAT32
  if (sdIsInit()) {
    double volumesize;
    volumesize = volume.blocksPerCluster();
    volumesize *= volume.clusterCount();
    volumesize *= 512; // SD card blocks are always 512 bytes

    Serial.print(F(" Cluster:    "));
    Serial.println(volume.blocksPerCluster() * 512);
     
    //Serialprint("Volume size (B):  "); Serial.println(volumesize);
      
    volumesize /= k;
    //Serialprint("Volume size (KB): "); Serial.println(volumesize);
      
    volumesize /= k;
    //Serialprint("Volume size (MB): "); Serial.println(volumesize);

    int intSize = volumesize;
      
    Serial.print(F(" SD size:    ")); Serial.print(intSize); Serial.println(F(" MB"));
  } // if (sdIsInit())
} // printSdSize()

void printSdRoot() {
  // try to open the partition FAT16 or FAT32
  if (sdIsInit()) {
    Serial.println(F("Found files:"));
    root.openRoot(volume);
    root.ls(LS_R | LS_DATE | LS_SIZE);
  }
}

#endif // SD_INFO_FEATURE
