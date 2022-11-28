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
#include <SPI.h>

// Replace with your network credentials
const char* ssd = "keylimepie";
const char* password = "2JamamasAnd1Elephant";

// UDP port to be used
const int udp_port = 5005;

// Server variables
String server_state = "CLOSED"
int last_received_seq_num = 0
int last_sent_ack_num = 0
const String EOM = "\r\n\r\n"

// DEBUG
bool DEBUG = true;

//UDP server


// Variables to save state of LEDs
String output26State = "off";
String output27State = "off";

const int output26 = 26;
const int output27 = 27;

// Variable to determine if a delay is in use
String delay = "off";

// update state
void update_server_state(String new_state){
  if DEBUG{
    Serial.print("State change from ");
    Serial.print(server_state);
    Serial.print(" to ");
    Serial.print(new_state);
  }
  server_state = new_state;
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
  if DEBUG{
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if DEBUG{
      Serial.print(".");
    }
  }
  // Print local IP address and start web server
  if DEBUG{
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  // Start UDP server
  Udp.begin(udp_port);

}

void loop() {
  // put your main code here, to run repeatedly:

}
