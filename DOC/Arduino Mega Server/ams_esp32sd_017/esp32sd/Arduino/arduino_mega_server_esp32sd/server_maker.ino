/*
  Module Page Maker
  part of Arduino Mega Server project
*/

#define OPEN_BLOCK_STYLE "\n<style>\n"
#define CLOSE_BLOCK_STYLE "</style>\n"
#define CLOSE_STYLE "  }\n"

#define HTM_EXT ".htm"

File insertFile;
#define MAX_BUFFER_INSERT 256
uint16_t sizeInsert;
uint8_t buffInsert[MAX_BUFFER_INSERT];

String openStyle(String style) {
  String s = "  .";
  s += style;
  s += " {\n";
  return s;
}

// <style>
//   .style {
//     background: url(//ip/fileName) no-repeat;
//   }
// </style>"

String backgroundIpAttr(String fileName) {
  String s = "    ";
  s += "background: url(//";
  s += stringIp(SELF_IP);
  s += "/";
  s += fileName;
  s += ") no-repeat;\n";
  return s;
}

void addBackgroundStyle(String style, String file, WiFiClient cl) {
  String s = OPEN_BLOCK_STYLE;
  s += openStyle(style);
  s += backgroundIpAttr(file);
  s += CLOSE_STYLE;
  s += CLOSE_BLOCK_STYLE;
  cl.println(s);
}

String themeSuffix(byte design) {
  switch (design) {
    case HOME_DESIGN:   return "_hm"; break;
    case MODERN_DESIGN: return "_md"; break;
    case HACKER_DESIGN: return "_hk"; break;
    case PART1_DESIGN:  return "_p1"; break;
    case PART2_DESIGN:  return "_p2"; break;
    case PART3_DESIGN:  return "_p3"; break;
    case PART4_DESIGN:  return "_p4"; break;
               default: return "";
  }
}

void insertBlock(uint8_t operation, WiFiClient cl) {
  String s = "";
  switch (operation) {
    case LINKS:
      s = themeSuffix(currentDesign);
      if (s == "_hm") {addBackgroundStyle(F("home"),   F("home.jpg"),   cl);}
      if (s == "_md") {addBackgroundStyle(F("modern"), F("modern.jpg"), cl);}
      insertFile = SD.open("/_one" + s + HTM_EXT);
      break;
    case HEADER:     s = themeSuffix(currentDesign); if (s == "") {s = "er";} insertFile = SD.open("/_head" + s + HTM_EXT); break;
    case FOOTER:     s = themeSuffix(currentDesign); if (s == "") {s = "er";} insertFile = SD.open("/_foot" + s + HTM_EXT); break;
    case BANNERS:    if (random(1, 15) == 5) {insertFile = SD.open("/_banner.htm");} break;
    case DASH:       insertFile = SD.open("/_dash" + themeSuffix(currentDesign) + HTM_EXT); break;
    case MENU:       insertFile = SD.open("/_menu" + themeSuffix(currentDesign) + HTM_EXT); break;
    case ADDRESS:    cl.print(stringIp(SELF_IP)); break;
    case SCRIPTS:    insertFile = SD.open("/scripts.js"); break;
    case FLOTR2:     insertFile = SD.open("/flotr2.js");  break;
    case PROCESSING: insertFile = SD.open("/process.js"); break;
    case THREE:      insertFile = SD.open("/three.js");   break;
    case JQUERY:     insertFile = SD.open("/jquery.js");  break;
    default: 
      {}
  } // switch

  if (insertFile) {
    while (insertFile.available()) {
      sizeInsert = insertFile.read(buffInsert, MAX_BUFFER_INSERT);
      cl.write(buffInsert, sizeInsert);
    }
    insertFile.close();
  }
} // insertBlock( )

