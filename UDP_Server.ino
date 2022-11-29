/****************
 Brian Goldsmith, Damon Raynor
 This sketch will create a UDP over TCP server. The first LED will blink when 
 the server is listening. When a client attempts to make a connection, the first
 LED will turn solid and the second LED will blink while establishing the initial
 handshake. Once the handshake is completed, the second LED will turn solid until
 the connection ends.
****************/

// load Wi-Fi library
#include <WiFi.h>
#include <WiFiUdp.h>

// Replace with your network credentials
const char* ssid = "keylimepie";
const char* password = "2JamamasAnd1Elephant";

// UDP port to be used
const int udp_port = 5005;

// Server variables
String server_state = "CLOSED";
int last_received_seq_num = 0;
int last_sent_ack_num = 0;
const String EOM = "\r\n\r\n";


// DEBUG
bool DEBUG = true;

//UDP server
WiFiUDP Udp;



const int output26 = 26;
const int output27 = 27;

// 1024 bits broken
char packetBuffer[1024];
char replyBuffer[1024];

String strBuff = "";

// Variable to determine if a delay is in use
boolean delayFlag = false;

// update state
void update_server_state(String new_state){
  new_state.trim();
  if (DEBUG){
    Serial.print("State change from ");
    Serial.print(server_state);
    Serial.print(" to ");
    Serial.println(new_state);
  }
  server_state = new_state;

  if(new_state == "SYN_RECEIVED"){
    ledOn(output26);    
  }
  if(new_state == "ESTABLISHED"){
    ledOn(output27);
  }
  if(new_state == "CLOSE_WAIT"){
    ledBlink(output27);    
  }
  if(new_state == "CLOSED"){
    ledOff(output26);
    ledOff(output27);
  }
}

void ledOff(int led_builtin){
  digitalWrite(led_builtin, LOW);
}

void ledOn(int led_builtin){
  digitalWrite(led_builtin, HIGH);
}

void ledBlink(int led_builtin){
  digitalWrite(led_builtin, HIGH);
  delay(333);
  digitalWrite(led_builtin, LOW);
  delay(333);
}

void print_received(char received[]){
  Serial.print("Received Packet From Client ");
  Serial.println(Udp.remoteIP());

  int header_seq_num = 0;
  for (int bit =0; bit < 32; bit++){
    header_seq_num <<= 1;
    if(packetBuffer[bit] == '1') header_seq_num |= 1;
  }

  int header_ack_num = 0;
  for (int bit =32; bit < 64; bit++){
    header_ack_num <<= 1;
    if(packetBuffer[bit] == '1') header_ack_num |= 1;
  }
  // print first 32 bits in binary then print seq number in decimal
  for (byte i=0; i<32; i++){
    Serial.print(received[i]);
  }
  Serial.print(" : seq = ");
  Serial.println(header_seq_num);

  // print next 32 bits in binary then ack_number
  for (byte i=0; i<32; i++){
    Serial.print(received[i+32]);
  }
  Serial.print(" : ack = ");
  Serial.println(header_ack_num);

  // print last 32 bits with flags
  for (byte i=0; i<32; i++){
    Serial.print(received[i+64]);
  }
  Serial.print(" : syn = ");
  Serial.print(received[64]);
  Serial.print(", ack = ");
  Serial.print(received[65]);
  Serial.print(", fin = ");
  Serial.println(received[66]);
}

void recvOneChar() {
    if (Serial.available() > 0) {
        char receivedChar = Serial.read();
        Serial.print("Received the character:");
        Serial.println(receivedChar);
        if (receivedChar == '0'){
          delayFlag = false;                    
        } else if (receivedChar == '1'){
          delayFlag = true;
        }
        
    }
}


void setup() {
  Serial.begin(115200);
  //Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);
    // Connect to Wi-Fi network with SSID and password
  if (DEBUG){
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (DEBUG){
      Serial.print(".");
    }
  }
  // Print local IP address and start web server
  if (DEBUG){
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  // Start UDP server
  Udp.begin(udp_port);

}

void loop() {
  
  // Check if the state of the server is CLOSED if so set it to LISTEN
  if (server_state == "CLOSED"){
    /* in state CLOSED
     * This state switches from state CLOSED to state LISTEN
     */

    update_server_state("LISTEN");
  } else if (server_state == "LISTEN"){
    /* in state LISTEN
     * This state is used for the initial connection. After receiving the initial
     * SYN=1, the server responds with SYN=1 and ACK=1. After sending the SYN ACK,
     * the server is changed to state SYN_RECEIVED.
     */

    // clear string buffer
    strBuff = "";
    ledBlink(output26); 
    recvOneChar();
    // receive message from the UDP server
    int packetSize = Udp.parsePacket();

    // if there is data in the packet
    if (packetSize) {
      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      if (DEBUG){
        print_received(packetBuffer);
      }

      // save header's seq_num to last_received_seq_num
      int header_seq_num = 0;
      for (int bit =0; bit < 32; bit++){
        header_seq_num <<= 1;
        if(packetBuffer[bit] == '1') header_seq_num |= 1;
      }
      last_received_seq_num = header_seq_num;
      

      // if header.SYN = 1
      if (packetBuffer[64] =='1'){

        // save new seq_number as random int random.randint(0,(2 ** 5)-1) random_int(0,31)
        int seq_number = int(random(0,31));
        byte num32pins = 32;
        byte num29pins = 29;
      
        // create a new ACK message to send back
        uint8_t replyHeader[96];
        
        // set sequence number
        byte num = seq_number;
        for (byte i=0; i<num32pins; i++) {
          replyHeader[31-i] = bitRead(num, i);
        }

        // set ack_num
        num = last_received_seq_num +1;
        for (byte i=0; i< num32pins; i++) {
          replyHeader[63-i] = bitRead(num,i);
        }

        // set SYN flag = 1
        replyHeader[64] = 1;
        
        // set ACK flag = 1
        replyHeader[65] = 1;

        // set FIN flag = 0
        replyHeader[66] = 0;

        // fill in the rest with zeros
        for (byte i=0; i<num29pins; i++){
          replyHeader[95-i] = 0;
        }
        
        
        if (DEBUG){
          //print(pretty_bits_print(bits))

          Serial.println("Sending Packet to Client");
          // print first 32 bits in binary then print seq number in decimal
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i]);
          }
          Serial.print(" : seq = ");
          Serial.println(seq_number);

          // print next 32 bits in binary then ack_number
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i+32]);
          }
          Serial.print(" : ack = ");
          Serial.println(last_received_seq_num+1);

          // print last 32 bits with flags
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i+64]);
          }
          Serial.print(" : syn = ");
          Serial.print(replyHeader[64]);
          Serial.print(", ack = ");
          Serial.print(replyHeader[65]);
          Serial.print(", fin = ");
          Serial.println(replyHeader[66]);
        }

        // increment seq_number for next time
        last_sent_ack_num = seq_number;
        last_sent_ack_num++;


        // send the response back
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(replyHeader, 96);
        Udp.endPacket();
        
        // update state to SYN_RECEIVED
        update_server_state("SYN_RECEIVED");
        if(delayFlag){
          delay(1000);
        }
      }
    }
  } else if (server_state == "SYN_RECEIVED"){
    // receive message from the UDP server
    int packetSize = Udp.parsePacket();


    // if there is data in the packet
    if (packetSize) {

      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }
      if (DEBUG){
        //Serial.println("Inside SYN_RECEIVED - ");
        print_received(packetBuffer);
      }

      // save header's seq_num to last_received_seq_num
      int header_seq_num = 0;
      for (int bit =0; bit < 32; bit++){
        bitWrite(header_seq_num, 31-bit, packetBuffer);
      }
      last_received_seq_num = header_seq_num;
    
      // if header.ACK = 1
      if (packetBuffer[65] == '1'){
        update_server_state("ESTABLISHED");
        if(delayFlag){
          delay(1000);
        }
      }
    }      
  } else if (server_state == "ESTABLISHED"){
    // receive message from the UDP server
    int packetSize = Udp.parsePacket();


    // if there is data in the packet
    if (packetSize) {

      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }


      // save header's seq_num to last_received_seq_num
      int header_seq_num = 0;
      for (int bit =0; bit < 32; bit++){
        header_seq_num <<= 1;
        if(packetBuffer[bit] == '1') header_seq_num |= 1;
      }
      last_received_seq_num = header_seq_num;
      if (DEBUG){
        //Serial.println("Inside ESTABLISHED - ");
        print_received(packetBuffer);
      }

              // create a new ACK message to send back
        uint8_t replyHeader[96];

      // if FIN =1 then send back ack and move to close wait
      if (packetBuffer[66] == '1'){

        // set sequence number
        byte num = last_sent_ack_num;
        for (byte i=0; i<32; i++) {
          replyHeader[31-i] = bitRead(num, i);
        }

        // set ack_num
        num = last_received_seq_num +1;
        for (byte i=0; i< 32; i++) {
          replyHeader[63-i] = bitRead(num,i);
        }

        // set SYN flag = 0
        replyHeader[64] = 0;
        
        // set ACK flag = 1
        replyHeader[65] = 1;

        // set FIN flag = 0
        replyHeader[66] = 0;

        // fill in the rest with zeros
        for (byte i=0; i<29; i++){
          replyHeader[95-i] = 0;
        }
        
        
        if (DEBUG){
          //print(pretty_bits_print(bits))

          Serial.println("Sending Packet to Client");
          // print first 32 bits in binary then print seq number in decimal
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i]);
          }
          Serial.print(" : seq = ");
          Serial.println(last_sent_ack_num);

          // print next 32 bits in binary then ack_number
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i+32]);
          }
          Serial.print(" : ack = ");
          Serial.println(last_received_seq_num+1);

          // print last 32 bits with flags
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i+64]);
          }
          Serial.print(" : syn = ");
          Serial.print(replyHeader[64]);
          Serial.print(", ack = ");
          Serial.print(replyHeader[65]);
          Serial.print(", fin = ");
          Serial.println(replyHeader[66]);
        }

        // increment seq_number for next time
        last_sent_ack_num++;


        // send the response back
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(replyHeader, 96);
        Udp.endPacket();

        // update state to CLOSE_WAIT
        update_server_state("CLOSE_WAIT");
        if(delayFlag){
          delay(1000);
        }
      } else{

        // get BODY of packet and print message
        String msg;
        int x = 0;
        for (byte i=96; i<packetSize; i++){
          if (packetBuffer[i] <= 0x0F){
            msg += "0";
            
          }
          msg += String(packetBuffer[i]);
          x++;
        }
        Serial.print("Message: ");
        Serial.println(msg);
        strBuff += msg;

        // send back an ACK
        
        byte num = last_sent_ack_num;
        for (byte i=0; i<32; i++) {
          replyHeader[31-i] = bitRead(num, i);
        }

        // set ack_num
        byte acknum = last_received_seq_num + x;
        for (byte i=0; i< 32; i++) {
          replyHeader[63-i] = bitRead(acknum,i);
        }

        // set SYN flag = 0
        replyHeader[64] = 0;
        
        // set ACK flag = 1
        replyHeader[65] = 1;

        // set FIN flag = 0
        replyHeader[66] = 0;

        // fill in the rest with zeros
        for (byte i=0; i<29; i++){
          replyHeader[95-i] = 0;
        }      
        
        if (DEBUG){
          //print(pretty_bits_print(bits))

          Serial.println("Sending Packet to Client");
          // print first 32 bits in binary then print seq number in decimal
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i]);
          }
          Serial.print(" : seq = ");
          Serial.println(last_sent_ack_num);

          // print next 32 bits in binary then ack_number
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i+32]);
          }
          Serial.print(" : ack = ");
          Serial.println(acknum);

          // print last 32 bits with flags
          for (byte i=0; i<32; i++){
            Serial.print(replyHeader[i+64]);
          }
          Serial.print(" : syn = ");
          Serial.print(replyHeader[64]);
          Serial.print(", ack = ");
          Serial.print(replyHeader[65]);
          Serial.print(", fin = ");
          Serial.println(replyHeader[66]);
        }

        // increment seq_number for next time
        last_sent_ack_num++;


        // send the response back
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(replyHeader, 96);
        Udp.endPacket();
      }
    }
  } else if (server_state == "CLOSE_WAIT"){
    // create a new ACK message to send back
    uint8_t replyHeader[96];
    ledBlink(output27);
    if (DEBUG){
      //Serial.println("Inside CLOSE_WAIT - ");
    }

    // set sequence number
    byte num = last_sent_ack_num;
    for (byte i=0; i<32; i++) {
      replyHeader[31-i] = bitRead(num, i);
    }

    // set ack_num
    num = last_received_seq_num +1;
    for (byte i=0; i< 32; i++) {
      replyHeader[63-i] = bitRead(num,i);
    }

    // set SYN flag = 0
    replyHeader[64] = 0;
    
    // set ACK flag = 1
    replyHeader[65] = 1;

    // set FIN flag = 1
    replyHeader[66] = 1;

    // fill in the rest with zeros
    for (byte i=0; i<29; i++){
      replyHeader[95-i] = 0;
    }
    
    
    if (DEBUG){
      //print(pretty_bits_print(bits))

      Serial.println("Sending Packet to Client");
      // print first 32 bits in binary then print seq number in decimal
      for (byte i=0; i<32; i++){
        Serial.print(replyHeader[i]);
      }
      Serial.print(" : seq = ");
      Serial.println(last_sent_ack_num);

      // print next 32 bits in binary then ack_number
      for (byte i=0; i<32; i++){
        Serial.print(replyHeader[i+32]);
      }
      Serial.print(" : ack = ");
      Serial.println(last_received_seq_num+1);

      // print last 32 bits with flags
      for (byte i=0; i<32; i++){
        Serial.print(replyHeader[i+64]);
      }
      Serial.print(" : syn = ");
      Serial.print(replyHeader[64]);
      Serial.print(", ack = ");
      Serial.print(replyHeader[65]);
      Serial.print(", fin = ");
      Serial.println(replyHeader[66]);
    }

    // increment seq_number for next time
    last_sent_ack_num++;


    // send the response back
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(replyHeader, 96);
    Udp.endPacket();
    
    // update state to SYN_RECEIVED
    update_server_state("LAST_ACK");
  } else if (server_state == "LAST_ACK"){
    int packetSize = Udp.parsePacket();

    ledBlink(output27);
    if (DEBUG){
      //Serial.println("Inside LAST_ACK - ");
    }

    // if there is data in the packet
    if (packetSize) {

      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }


      // save header's seq_num to last_received_seq_num
      int header_seq_num = 0;
      for (int bit =0; bit < 32; bit++){
        header_seq_num <<= 1;
        if(packetBuffer[bit] == '1') header_seq_num |= 1;
      }
      last_received_seq_num = header_seq_num;



      // if FIN =1 then send back ack and move to close wait
      if (packetBuffer[65] == '1'){
        Serial.print("Full message from client: ");
        Serial.println(strBuff);
        update_server_state("CLOSED");
      }
    }

  } else {
    if(DEBUG){
      //Serial.print("Should be in state: ");
      //Serial.println(server_state);
    }
  }   
}
