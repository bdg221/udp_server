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
const char* ssid = "XXXXX";
const char* password = "XXXXX";

// UDP port to be used
const int udp_port = 5005;

// Server variables 
String server_state = "CLOSED";
int last_received_seq_num = 0;
int last_sent_ack_num = 0;
const String EOM = "\r\n\r\n";


// DEBUG
bool DEBUG = true;

// UDP server
WiFiUDP Udp;


// location of LEDs
const int output26 = 26;
const int output27 = 27;

// 1024 bits used for incoming and outgoing messages
// packetBuffer is for incoming
// replyBuffer is for outgoing
char packetBuffer[1024];
char replyBuffer[1024];

// This is the string used for combining any broken up messages
String strBuff = "";

// Variable to determine if a delay is in use
boolean delayFlag = false;


/************************************************************************************************
 * The update_server_state method updates the state of the server.
 * The method takes a new state as a String then updates the global variable.
 * Also, this method changes the LEDs based off of the state.
 ************************************************************************************************/
void update_server_state(String new_state){
  new_state.trim();

  // If DEBUG is true then print information about the state change
  if (DEBUG){
    Serial.print("State change from ");
    Serial.print(server_state);
    Serial.print(" to ");
    Serial.println(new_state);
  }

  // Set the new state
  server_state = new_state;

  // If the new state is SYN_RECEIVED then set LED 26 to be on
  if(new_state == "SYN_RECEIVED"){
    ledOn(output26);    
  }

  // If the new state is ESTABLISHED set LED 27 to be on
  if(new_state == "ESTABLISHED"){
    ledOn(output27);
  }

  // If the new state is CLOSE_WAIT, then connection termination is starting
  // therefore set LED 27 to blink
  if(new_state == "CLOSE_WAIT"){
    ledBlink(output27);        
  }

  // If the new state is CLOSED, then set LED 26 and LED 27 to be off
  if(new_state == "CLOSED"){
    ledOff(output26);
    ledOff(output27);
  }
}


/************************************************************************************************
 * The helper method ledOff simply turns the LED on the supplied pin to off
 ************************************************************************************************/
void ledOff(int led_builtin){
  digitalWrite(led_builtin, LOW);
}

/************************************************************************************************
 * The helper method ledOff simply turns the LED on the supplied pin to off
 ************************************************************************************************/
void ledOn(int led_builtin){
  digitalWrite(led_builtin, HIGH);
}

/************************************************************************************************
 * The helper method ledOff simply turns the LED on the supplied pin to off
 ************************************************************************************************/
void ledBlink(int led_builtin){
  digitalWrite(led_builtin, HIGH);
  delay(333);
  digitalWrite(led_builtin, LOW);
  delay(333);
}

/************************************************************************************************
 * The print_received method takes a char array of the received bits. It parses out each piece of
 * the header/message and prints the information.
 ************************************************************************************************/
void print_received(char received[]){
  Serial.print("Received Packet From Client ");
  Serial.println(Udp.remoteIP());

  // The first 32 bits are used for the sequence number. This section takes the bits and translates
  // them to a decimal.
  int header_seq_num = 0;
  for (int bit =0; bit < 32; bit++){
    header_seq_num <<= 1;
    if(packetBuffer[bit] == '1') header_seq_num |= 1;
  }

  // The next 32 bits are used for the ack number. This section takes the bits and translates
  // them to a decimal.
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

/************************************************************************************************
 * The recOneChar method is used for receiving information from the Serial Monitor. The single 
 * character will either be a 0 or a 1 with 0 being used to set the delayFlag to false and a 1
 * used to set the delayFlag to true. The delayFlag is used in the main loop to add a delay so
 * it is easier to visualize the LED changes.
 ************************************************************************************************/
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

/************************************************************************************************
 * The setup method is the main method used by the Arduino to initialize things. It is run once
 * as the board boots up.
 ************************************************************************************************/
void setup() {

  // set the bod rate
  Serial.begin(115200);

  //Initialize the LED pin variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  
  // Set LED pins to LOW (off)
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

/************************************************************************************************
 * The loop method is the main method used by the Arduino. While running, the loop method is
 * called over and over again.
 ************************************************************************************************/
void loop() {
  
  
  if (server_state == "CLOSED"){
    /* 
     If the state of the server is CLOSED, set it to LISTEN 
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

    // set LED 26 to blink while listening
    ledBlink(output26); 

    // in a listening state, check if there is a character sent from the Serial Monitor
    // this is used to change the delayFlag
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

      // if DEBUG is on, print the info from the received packet
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

        // save a new seq_number as random int random.randint(0,(2 ** 5)-1) random_int(0,31)
        int seq_number = int(random(0,31));
        byte num32pins = 32;
        byte num29pins = 29;
      
        // create a new header of 96 bits to send back
        uint8_t replyHeader[96];
        
        // set sequence number
        byte num = seq_number;
        for (byte i=0; i<num32pins; i++) {
          // this is setting the bit values in reverse order starting at bit 32
          // this is necessary since the entire [0-31] is the sequence number
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

        // fill in the rest of the header with zeros
        for (byte i=0; i<num29pins; i++){
          replyHeader[95-i] = 0;
        }
        

        // if DEBUG is true then print the header packet that is being sent        
        if (DEBUG){

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

        // save the seq_number to the global variable and then increment the seq_number for next time
        last_sent_ack_num = seq_number;
        last_sent_ack_num++;


        // send the response packet back to the client
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(replyHeader, 96);
        Udp.endPacket();
        
        // update state to SYN_RECEIVED
        update_server_state("SYN_RECEIVED");

        // if delayFlag is true, add a 1 second delay
        if(delayFlag){
          delay(1000);
        }
      }
    }
  } else if (server_state == "SYN_RECEIVED"){
    /* in state SYN_RECEIVED
     * This state checks that the client received the ACK that was sent by the server.
     * If the client returns an ACK to the server's ACK then move on to ESTABLISHED. state.     
     */

    // receive message from the UDP server
    int packetSize = Udp.parsePacket();


    // if there is data in the packet
    if (packetSize) {

      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      // if DEBUG is on, print the info from the received packet
      if (DEBUG){
        print_received(packetBuffer);
      }

      // save header's seq_num to global variable last_received_seq_num
      int header_seq_num = 0;
      for (int bit =0; bit < 32; bit++){
        bitWrite(header_seq_num, 31-bit, packetBuffer);
      }
      last_received_seq_num = header_seq_num;
    
      // if header.ACK = 1
      if (packetBuffer[65] == '1'){

        // change to state ESTABLISHED
        update_server_state("ESTABLISHED");

        // if delayFlag is true, add a 1 second delay
        if(delayFlag){
          delay(1000);
        }

      }
    }      
  } else if (server_state == "ESTABLISHED"){
    /* in state ESTABLISHED
     * This state is used receiving data that is saved in the strBuff. Also, this state
     * checks to see if a FIN is received from the client in order to start terminating
     * the connection.
     */

    // receive message from the UDP server
    int packetSize = Udp.parsePacket();


    // if there is data in the packet
    if (packetSize) {

      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      // save header's seq_num to global variable last_received_seq_num
      int header_seq_num = 0;
      for (int bit =0; bit < 32; bit++){
        header_seq_num <<= 1;
        if(packetBuffer[bit] == '1') header_seq_num |= 1;
      }
      last_received_seq_num = header_seq_num;

      // if DEBUG is on, print the info from the received packet
      if (DEBUG){
        //Serial.println("Inside ESTABLISHED - ");
        print_received(packetBuffer);
      }

      // create a new header of 96 bits to send back
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
        
        // if DEBUG is true then print the header packet that is being sent    
        if (DEBUG){

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

        // if delayFlag is true, add a 1 second delay
        if(delayFlag){
          delay(1000);
        }
      } else{
        // In this section we are in ESTABLISHED and receiving data from the client. The
        // server needs to save the data and send back an ACK to the client with the
        // appropriate ACK number.

        // temporary String for this part of the message
        String msg;
        
        // read all data beyond the 96 bit header and translate it into ASCII. Then save
        // the data into the msg String.
        int x = 0;
        for (byte i=96; i<packetSize; i++){
          if (packetBuffer[i] <= 0x0F){
            msg += "0";
            
          }
          msg += String(packetBuffer[i]);
          x++;
        }

        // Print out the received message part
        Serial.print("Message: ");
        Serial.println(msg);

        // add the partial message to the full string message
        strBuff += msg;

        // set the sequence number for the response
        byte num = last_sent_ack_num;
        for (byte i=0; i<32; i++) {
          replyHeader[31-i] = bitRead(num, i);
        }

        // set ack_num (last sequence number + data size)
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
        
        // if DEBUG is true then print the header packet that is being sent  
        if (DEBUG){        
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
    /* in state CLOSE_WAIT
     * This state sends a ACK =1 and a FIN=1 to tell the client that the server is closing the 
     * connection.     
     */
     
    // create a new jheader to send back
    uint8_t replyHeader[96];

    // set LED 27 to blink to show that the connection is being terminated
    ledBlink(output27);

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
    
    // if DEBUG is true then print the header packet that is being sent  
    if (DEBUG){
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
    /* in state LAST_ACK
     * This state looks for the final ACK from the client to acknowledge that the server
     * is closing the connection. Once that is received, set the server state to CLOSED.     
     */

    // check for a new packet     
    int packetSize = Udp.parsePacket();

    // since the connection termination is still in process, keep LED 27 blinking
    ledBlink(output27);

    // if there is data in the packet
    if (packetSize) {

      // read all of the data and put it into packetBuffer
      int len = Udp.read(packetBuffer, 1024);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      // if DEBUG is on, print the info from the received packet
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



      // if the client sent the final ACK then close the connection
      if (packetBuffer[65] == '1'){

        // print the full message from the client
        Serial.print("Full message from client: ");
        Serial.println(strBuff);

        // set server state to CLOSED
        update_server_state("CLOSED");
      }
    }

  } else {
    // shouldn't ever reach this state
  }   
}