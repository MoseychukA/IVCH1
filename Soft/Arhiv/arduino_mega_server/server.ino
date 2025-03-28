/*
  Module Server
  part of Arduino Mega Server project
*/

#define SERVER_PORT 80
EthernetServer server(SERVER_PORT);

// HTTP request
#define REQ_BUF_SIZE 128
char HTTP_req[REQ_BUF_SIZE] = {0}; // null terminated string
int reqIndex = 0;

// String request
#define MAX_LEN_STRING  128
#define MAX_LEN_REQUEST 256 // 512
String request = "";

// webFile
File webFile;
#define MAX_BUFFER_SIZE 256
uint16_t rsize;
uint8_t buff[MAX_BUFFER_SIZE];

bool allowMarkers = false;
#define MARKER     0x25 // %
#define LINKS      0x26 // &
#define SCRIPTS    0x23 // #
#define HEADER     0x31 // 1
#define DASH       0x32 // 2
#define MENU       0x33 // 3
#define FOOTER     0x34 // 4
#define BANNERS    0x35 // 5
#define ALT_MENU   0x36 // 6
#define ALT_CSS    0x37 // 7
#define PRO        0x38 // 8
#define ADDRESS    0x21 // !
#define FLOTR2     0x3F // ?
#define PROCESSING 0x2B // +
#define THREE      0x40 // @
#define JQUERY     0x3D // =

// Server init

void serverInit() {
  server.begin();
  delay(200);
  started("Server");
}

// Markers works

int checkMarker(int startPos, int endPos) {
  for (int i = startPos; i < endPos; i++) {
    if (buff[i] == MARKER && (buff[i + 1] == HEADER ||
                              buff[i + 1] == LINKS ||
                              buff[i + 1] == DASH ||
                              buff[i + 1] == MENU ||
                              buff[i + 1] == ALT_MENU ||
                              buff[i + 1] == BANNERS ||
                              buff[i + 1] == FOOTER ||
                              buff[i + 1] == ALT_CSS ||
                              buff[i + 1] == PRO ||
                              buff[i + 1] == ADDRESS ||
                              buff[i + 1] == FLOTR2 ||
                              buff[i + 1] == PROCESSING ||
                              buff[i + 1] == THREE ||
                              buff[i + 1] == JQUERY ||
                              buff[i + 1] == SCRIPTS)) {
      return i;
    }
  }
  return -1;
}

void sendDelta(int startPos, int endPos, EthernetClient cl) {
  String s = "";
  for (int i = startPos; i < endPos; i++) {s += char(buff[i]);}
  cl.print(s);
}

void markersWorks(int pos, EthernetClient cl) {
  if (pos > rsize - 2) {return;}
  int markerPos = checkMarker(pos, rsize - 1);
  if (markerPos > -1) {
    sendDelta(pos, markerPos, cl);
    insertBlock(buff[markerPos + 1], cl);
    markersWorks(markerPos + 2, cl);
  } else {
      if (pos == 0) {
        String s = "";
        for (int i = 0; i < rsize; i++) {s += char(buff[i]);}
        cl.print(s);
      } else {
          sendDelta(pos, rsize, cl);
        }
    }
}

// Server works

void serverWorks2(EthernetClient sclient) {
  //byte row = 1;
  String strRequest = "";
  if (sclient) {
    bool currentLineIsBlank = true;
    while (sclient.connected()) {      
      if (sclient.available()) {   // client data available to read
        char c = sclient.read();   // read 1 byte (character) from client

        /* limit the size of the stored received HTTP request
           buffer first part of HTTP request in HTTP_req array (string)
           leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1) */

        // make request (array)
        if (reqIndex < (REQ_BUF_SIZE - 1)) {
          HTTP_req[reqIndex] = c; // save HTTP request character
          reqIndex++;
        }
        
        // make request (String)
        /*
        if (request.length() < MAX_LEN_REQUEST) {
          if (c == '\n') {row++;}
          if (row != 2 && row != 3 && row != 4 && row != 5 && row != 6) {
            request += c;
          }
        }
        */

        if (strRequest.length() < MAX_LEN_STRING) {
          strRequest += c;
        }
        if (c == '\n' || strRequest.length() >= MAX_LEN_STRING) {
          if (strRequest.indexOf(F("GET")) == 0) {
            request += strRequest;
            strRequest = "";
          }
          else if (strRequest.indexOf(F("Authorization")) == 0) {
            request += strRequest;
            strRequest = "";
          }
          else {
            strRequest = "";
            }
        }

        /* last line of client request is blank and ends with \n
           respond to client only after last line received */

        if (c == '\n' && currentLineIsBlank) {
          if (authMode == OFF || (authMode == ON && request.lastIndexOf(AUTH_HASH) > -1)) {

            //Serial.println(request.length());
            //Serial.println(request);
            
            parseCommands(sclient);
            parseRequest(sclient); 

            if (webFile) {
              while(webFile.available()) {
                rsize = webFile.read(buff, MAX_BUFFER_SIZE);
              
                if (allowMarkers) {
                  markersWorks(0, sclient);
                } else {
                    sclient.write(buff, rsize);
                  }
              }
                      
              webFile.close();
            } // if (webFile)
               
            // Reset buffer index and all buffer elements to 0
            reqIndex = 0;
            StrClear(HTTP_req, REQ_BUF_SIZE);
            request = "";
     
          } else { // if (authMode == AUTH_OFF || (authMode == AUTH_ON && request.lastIndexOf(AUTH_HASH) > -1))
              request = "";
              sclient.println(F("HTTP/1.0 401 Unauthorized"));
              sclient.println(F("WWW-Authenticate: Basic realm=\"Arduino Mega Server\""));
            }
          
          break;
        }
        
        // every line of text received from the client ends with \r\n
        if (c == '\n') {
          
          /* last character on line of received text starting new line with next character read */
          currentLineIsBlank = true;
        } else if (c != '\r') {
            // a text character was received from client
            currentLineIsBlank = false;
          }
      } // if (client.available())
    } // while (client.connected())  
    delay(5); // give the web browser time to receive the data
    sclient.stop(); // close the connection
  } // if (client)  
} // serverWorks2( )

void serverWorks() {
  for (int sock = 0; sock < MAX_SOCK_NUM - FREE_SOCKETS; sock++) {
    EthernetClient sclient = server.available_(sock);
    serverWorks2(sclient);
  }
  /*
  EthernetClient sclient = server.available();
  serverWorks2(sclient);
  */
}

