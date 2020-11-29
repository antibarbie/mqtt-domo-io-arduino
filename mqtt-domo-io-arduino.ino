#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <string.h>
#include "ShiftOutput.h"
#include "ShiftInput.h"
#include "Blink.h"

#define RELEASE_VERSION "0.2 - 11/2020"

// ----------------------------------------------------------------
// Arduino modules
// - INPUT: Read informations and publish them
// - OUTPUT: Subscribe to informations and write them physically
// - CORE: Subscribe to informations and publish computations
// - TRACE: Subscribe to topics and print them on screen
// - SENSOR: Read sensors and publishes them --- Other project ? ---
// ----------------------------------------------------------------

#define MODE_INPUT
//#define MODE_OUTPUT
//#define MODE_CORE
//#define MODE_TRACE

#if !defined MODE_INPUT && !defined MODE_OUTPUT && !defined MODE_CORE && !defined MODE_TRACE
#warning "defaulting to MODE_CORE. You can also define MODE_INPUT or MODE_OUTPUT or MODE_TRACE."
#define MODE_CORE
#endif

// ----------------------------------------------------------------------------
// SETTINGS TO MODIFY

#define MQTT_USER         "test"
#define MQTT_PASSWORD     "test"
#define MQTT_ROOT_TOPIC   "MDB"

// Update these with values suitable for your network.
byte current_mac[]    = {  0xDE,0x2D,0x8F,0xBD,0x8F,0x01 }; // Last byte will be changed with ARDUINO#
byte current_ip[] = { 192, 168, 100, 0 };                   // Last byte will be changed with ARDUINO#
byte current_dns[] = { 192,168,100,1 };
byte current_gw[] = { 192,168,100,1 };
byte current_subnet[] = { 255,255,255,0 };

// Target broker - key element for our mqtt network !
byte mqtt_broker_address[] = { 192, 168, 100, 44 };
const int mqtt_broker_port = 1883;

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// SETTINGS / NAMING


// MISC
// Common defs
#define PIN_DIPSWITCH A7
#define STATUS_LED PD2
// Input defs
#define PIN_CHAINLENGTH A6 
#define PIN_INPUT_PL A0
#define PIN_INPUT_CE A1
#define PIN_INPUT_CP A2

#define PIN_INPUT_DATA0 PD3
#define PIN_INPUT_DATA1 PD4
#define PIN_INPUT_DATA2 PD5

// Output defs
#define PIN_OUTPUT_DATA A0
#define PIN_OUTPUT_CLOCK A1
#define PIN_OUTPUT_LATCH A2

// MQTT

// ROOT/IN/1/28
// ROOT/OUT/2/31
// ROOT/STATUS/IN/1
// ROOT/STATUS/CORE/1
// ROOT/STATUS/OUT/0
// ROOT/TRACE/..   (idem STATUS)

#define MQTT_ALL_INPUT MQTT_ROOT_TOPIC "/IN/#"
#define MQTT_ALL_OUTPUT MQTT_ROOT_TOPIC "/OUT/#"
#define MQTT_ALL_STATUS MQTT_ROOT_TOPIC "/STATUS/#"
#define MQTT_ALL_TRACE MQTT_ROOT_TOPIC "/TRACE/#"
#define MQTT_ALL_NODES_SUFFIX "/#"

// My own prefix
#ifdef MODE_INPUT  
#define MQTT_SHORT_NAME  "INPUT NODE #%d - UID#%d"
#define MQTT_SHORT_TOPIC "/IN/%d" 
// Generic I/O publish topic ROOT/TYPE/node_id/io_number  (FMT => %d %d )
#define MQTT_IO_PUBLISH_TOPIC MQTT_ROOT_TOPIC MQTT_SHORT_TOPIC "/%d"
#endif
#ifdef MODE_OUTPUT
#define MQTT_SHORT_NAME  "OUTPUT NODE #%d - UID#%d"
#define MQTT_SHORT_TOPIC "/OUT/%d" 
// Generic I/O publish topic ROOT/TYPE/node_id/io_number  (FMT => %d %d )
#define MQTT_IO_SUBSCRIBE_TOPIC_COMMON MQTT_ROOT_TOPIC MQTT_SHORT_TOPIC
#define MQTT_IO_SUBSCRIBE_TOPIC MQTT_IO_SUBSCRIBE_TOPIC_COMMON MQTT_ALL_NODES_SUFFIX
#endif
#ifdef MODE_CORE
#define MQTT_SHORT_NAME  "CORE NODE #%d - UID#%d"
#define MQTT_SHORT_TOPIC "/CORE/%d"
#endif
#ifdef MODE_TRACE
#define MQTT_SHORT_NAME  "TRACE NODE #%d - UID#%d"
#define MQTT_SHORT_TOPIC "/TRACE/%d"
#endif


// Generic TRACE publish topic ROOT/TRACE/TYPE/node_id (FMT => %d)
#define MQTT_TRACE_PUBLISH_TOPIC MQTT_ROOT_TOPIC "/TRACE" MQTT_SHORT_TOPIC

// Generic STATUS publish topic ROOT/STATUS/TYPE/node_id (FMT => %d)
#define MQTT_STATUS_PUBLISH_TOPIC MQTT_ROOT_TOPIC "/STATUS" MQTT_SHORT_TOPIC


// Last Will : alive => 0
#define MQTT_WILL_QOS 2
#define MQTT_WILL_MESSAGE "0"



// ----------------------------------------------------------------------------
// ARDUINO #

// Leaves room for some outputs, 2 core, 2 trace, 4 inputs
#define OUTPUT_BASE 248
#define CORE_BASE 246
#define TRACE_BASE 244
#define INPUT_BASE 240

#ifdef MODE_OUTPUT
#define XBASE OUTPUT_BASE
#else
#ifdef MODE_INPUT
#define XBASE INPUT_BASE
#else
#ifdef MODE_TRACE
#define XBASE TRACE_BASE
#else
#define XBASE CORE_BASE
#endif
#endif
#endif

int freeRam() 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

// Unique arduino number
byte arduinoNumber = 0xFF;


// Computes arduino absolutely unique number
int setup_compute_dipswitch_number(int pinDipSwitch)
{
  // Resistors :       GND -- 10k -- A0 --+---+---+
  // [   ] 0 0 0 : 0  [ @ ] 0 1 0 : 2     |   |   |
  // [@@@] 0R         [@ @] 317R         47k 22k 10k
  //                                      |   |   |
  // [@@ ] 1 1 0 : 3  [@  ] 1 0 0 : 1     1   2   3
  // [__@] 407R       [ @@] 176R
  //
  // [  @] 0 0 1 : 4  [ @@] 0 1 1 : 6
  // [@@ ] 510R       [@  ] 605R         
  //                                 
  // [@@@] 1 1 1 : 7  [@ @] 1 0 1 : 5
  // [___] 638R       [_@_] 559R 
  //  
  int dip = analogRead(pinDipSwitch);
  //Serial.print("@@DEBUG:   DIP SWITCH value says: "); Serial.println(dip);

  const int id_table[] = { 0, 176, 317, 407, 510, 559, 605, 638 };
  const int epsilon = 20;
  for (int i = 0; i < sizeof(id_table)/sizeof(int); i++) {
    if ((id_table[i] - epsilon <= dip)&&(id_table[i] + epsilon >= dip))
    {
      //Serial.print("@@DEBUG:  Found DIP SWITCH: "); Serial.print(i); Serial.print("  for value: "); Serial.println(dip);
      return i;
    }
  }
  
  return 0;
}

// Return DIP SWITCH Arduino number
int getArduinoNumber()
{
  return arduinoNumber;
}

// Return UNIQUE ID Arduino number
#define UNIQUE_ID_ARDUINO_NUMBER ((byte)getArduinoNumber() + (byte)XBASE)


// Debug output for our mac address
void printMacAddress() {
    uint8_t mac[6];
    Ethernet.MACAddress(mac);

    Serial.print(mac[0], HEX); Serial.print(":"); Serial.print(mac[1], HEX); Serial.print(":");
    Serial.print(mac[2], HEX); Serial.print(":"); Serial.print(mac[3], HEX); Serial.print(":");
    Serial.print(mac[4], HEX); Serial.print(":"); Serial.print(mac[5], HEX); Serial.println();
}

#ifdef MODE_OUTPUT

ShiftOutput outputShiftRegister(PIN_OUTPUT_DATA, PIN_OUTPUT_CLOCK, PIN_OUTPUT_LATCH);

/// apply the settings on the real world :
/// let's talk to all of these 74HC595 !
void on_output_value(int outputId, int current_value)
{
  outputShiftRegister.setBit(outputId, current_value != 0);
  outputShiftRegister.apply();    

  Serial.print("Outputs: "); Serial.println(outputShiftRegister.get(),HEX);
}
#endif


// ----------------------------------------------------------------------------
// MQTT CB

#ifdef MODE_INPUT
///
/// Input mode : no interest in subscriptions for now, but we publish stuff, at least
///
void mqtt_input_callback(char* topic, byte* payload, unsigned int length) {
  // NOP NOW  
}
#endif

#ifdef MODE_OUTPUT
///
/// Output mode : we just read the MQTT topics, and apply the values to the output transistors
///
void mqtt_output_callback(char* topic, byte* payload, unsigned int length) {
  // Starts from our original topic : e.g. ROOT/OUT/3
  char my_topic[sizeof(MQTT_IO_SUBSCRIBE_TOPIC_COMMON)];
  snprintf(my_topic, sizeof(my_topic),MQTT_IO_SUBSCRIBE_TOPIC_COMMON,getArduinoNumber());
  int lt = strlen(my_topic);
  int ltt = strlen(topic);

  // If the topics starts correctly from the right value, and continues with a slash
  // also, keep out fool values !
  if (!strncmp(topic, my_topic, lt) && topic[lt] == '/' && (ltt - lt <= 3) && (length < 4)) {
    lt++;
    // Expected : numeric
    int outputId = atoi(& topic[lt]);
    Serial.print("OUTPUT: topic found for OUT "); Serial.println(outputId);
    // Expected : numeric payload
    int payload_length_retained = min(sizeof(my_topic)-1,min(length, 10));
    memcpy(my_topic, payload, payload_length_retained); // numeric size.. not that large !    
    my_topic[payload_length_retained] = 0;
    on_output_value(outputId, atoi(my_topic)); // apply the settings on the real world
  } else {
    Serial.print("OUTPUT: Unrecognized topic: "); Serial.println(topic);
  }
}
#endif

#ifdef MODE_CORE
///
/// Our common logic - listen to ALL inputs and apply output logic
///
void mqtt_core_callback(char* topic, byte* payload, unsigned int length) {
  // If it starts with an input..
  
}
#endif

#ifdef MODE_TRACE
///
/// Log the traces on the screen
///
void mqtt_trace_callback(char* topic, byte* payload, unsigned int length) {
  
}
#endif

// --------------------------------------------------------------------------------------

// Common callback
void MqttMessageCallback(char* topic, byte* payload, unsigned int length) {

#if 1 // log everything
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif// LOG

  // jump to specific callback codes
#ifdef MODE_INPUT
  mqtt_input_callback(topic, payload, length);
#endif
#ifdef MODE_OUTPUT
  mqtt_output_callback(topic, payload, length);
#endif
#ifdef MODE_CORE
  mqtt_core_callback(topic, payload, length);
#endif
#ifdef MODE_TRACE
  mqtt_trace_callback(topic, payload, length);
#endif
  
}

// ---------------------------------------------------------------------------


EthernetClient ethClient;
PubSubClient mqttClient(mqtt_broker_address, mqtt_broker_port, MqttMessageCallback, ethClient);
Blink blink(STATUS_LED);

// ---------------------------------------------------------------------------

#ifdef MODE_INPUT

// Not of much interest in topics for the inputs !
int mqtt_input_subscribe() 
{
  return 1;
}

/// Input reader object
IShiftCommon * _current_input;

/// Callback  for inputs received
bool onInputButton(int inputIndex, bool inputStatus)
{  
  char my_topic[sizeof(MQTT_IO_PUBLISH_TOPIC)+2]; // "%d%d" turns into "999999" at worst so +2
  snprintf(my_topic, sizeof(my_topic),MQTT_IO_PUBLISH_TOPIC,getArduinoNumber(), inputIndex);
  Serial.print("Publishing to '"); Serial.print(my_topic);Serial.print("' = ");Serial.println(inputStatus ? "1" : "0");
  
   bool ok = mqttClient.publish(my_topic, inputStatus ? "1" : "0");  
   blink.set(ok ? Blink::BlinkMode::blink_white : Blink::BlinkMode::blink_fast);
   return ok;
}

// Read DIP SWITCH (Chain length)
// SET Specific PINS
// Initialize status
void setup_input()
{
  // D9-D6 : OPT INPUT
  //--commun-- D2 : STATUS LED
  // D3 : INPUT CHAIN 0
  // D4 : INPUT CHAIN 1
  // D5 : INPUT CHAIN 2
  // A0,A1,A2 : PL,CE,CP  
  
  // A6: DIP SWITCH CHAIN LENGTH
  //--commun-- A7: DIP SWITCH NODE
  byte _input_chain_length = setup_compute_dipswitch_number(PIN_CHAINLENGTH);
  Serial.print("Initialize CHAIN LENGTH : ");Serial.println(_input_chain_length);

  // Pour faire simple : 
  // T > 0 : lire tout en local sur plein de bits
  switch(_input_chain_length)
  {
    default://int ploadPin,int clockEnablePin,int clockPin, int (&dataPins)[INS]
    case 0:  // T = 0, read local chips + slave1 + slave2
      _current_input = new ShiftInput<32,3,96>(onInputButton, PIN_INPUT_PL,PIN_INPUT_CE,PIN_INPUT_CP,{PIN_INPUT_DATA0,PIN_INPUT_DATA1,PIN_INPUT_DATA2});
      break;
    case 1:  // T = 1, read local + 1 chained chips
      _current_input = new ShiftInput<64,1,64>(onInputButton, PIN_INPUT_PL,PIN_INPUT_CE,PIN_INPUT_CP,{PIN_INPUT_DATA0});
      break;
    case 2:  // T = 2, read local + 2 chained chips
      _current_input = new ShiftInput<96,1,96>(onInputButton, PIN_INPUT_PL,PIN_INPUT_CE,PIN_INPUT_CP,{PIN_INPUT_DATA0});
      break;
    case 3:  // T = 3, read local + 3 chained chips
      _current_input = new ShiftInput<128,1,128>(onInputButton, PIN_INPUT_PL,PIN_INPUT_CE,PIN_INPUT_CP,{PIN_INPUT_DATA0});
      break;    
  }

  // specific SETUP
  if (_current_input)
    _current_input->setup();

}


// INPUT loop : read the inputs ! Publish the results
void input_loop() 
{
  // Read inputs in function of the configurated topology (slaves/chains, #items)
  if (_current_input)    
    _current_input->loop();
}

#endif
#ifdef MODE_OUTPUT
int mqtt_output_subscribe() // Very important for the outputs
{
  char my_topic[sizeof(MQTT_IO_SUBSCRIBE_TOPIC)];
  snprintf(my_topic, sizeof(my_topic),MQTT_IO_SUBSCRIBE_TOPIC,getArduinoNumber());
  Serial.print("Subscribing to '"); Serial.print(my_topic);Serial.println("'");
  return mqttClient.subscribe(my_topic);  
}
#endif
#ifdef MODE_CORE
int mqtt_core_subscribe() // Very important for the core logic
{
  return mqttClient.subscribe(MQTT_ALL_INPUT);  
}
#endif
#ifdef MODE_TRACE
int mqtt_trace_subscribe() // Source for the traces / status
{
  mqttClient.subscribe(MQTT_ALL_STATUS);
  return mqttClient.subscribe(MQTT_ALL_TRACE);
}
#endif

// ---------------------------------------------------------------------------

// MQTT Client connect/reconnect
void mqttClientConnect()
{
  Serial.println(freeRam());
  Serial.println("mqttClientConnect -------------------- ");
  // nb: our client name is our status topic
  char my_mqtt_status_topic[sizeof(MQTT_STATUS_PUBLISH_TOPIC)+1]; 
  snprintf(my_mqtt_status_topic, sizeof(my_mqtt_status_topic),MQTT_STATUS_PUBLISH_TOPIC,getArduinoNumber());

  Serial.print("Attempting MQTT connect for ");Serial.println(my_mqtt_status_topic);
  Serial.println(freeRam());

  // client id, client username, client password, last will topic, last will qos, last will retain, last will message
  if (
    mqttClient.connect(my_mqtt_status_topic, MQTT_USER, MQTT_PASSWORD, my_mqtt_status_topic /* last will topic == root/status/me */, MQTT_WILL_QOS, true, MQTT_WILL_MESSAGE)
    ) {
    // connection succeeded
    Serial.println("Connected ok. Publishing status.");
  
    boolean r;
    // Publish status
    r = mqttClient.publish(my_mqtt_status_topic, "1");

    // Blink status
    blink.set(Blink::BlinkMode::blink_white);

    // Subscribing
    #ifdef MODE_INPUT
    r = mqtt_input_subscribe(); // Not of much interest for the inputs !
    #endif
    #ifdef MODE_OUTPUT
    r = mqtt_output_subscribe(); // Very important for the outputs
    #endif
    #ifdef MODE_CORE
    r = mqtt_core_subscribe(); // Very important for the core logic
    #endif
    #ifdef MODE_TRACE
    r = mqtt_trace_subscribe(); // Source for the traces
    #endif
    
    Serial.print("subscribed: "); Serial.println(r);    
    // Blink status
    blink.set(r ? Blink::BlinkMode::blink_white : Blink::BlinkMode::blink_fast);
  } 
  else {
    // Blink status
    blink.set(Blink::BlinkMode::blink_slow);
    
    // connection failed
    // mqttClient.state() will provide more information
    // on why it failed.
    Serial.print("Connection failed: ");
    Serial.println(mqttClient.state());        
    Serial.print("Eth link: ");
    Serial.print(Ethernet.linkStatus());
    Serial.print(" Eth hardware: ");
    Serial.println(Ethernet.hardwareStatus());
  }

}



// Setup common stuff
void setup_common()
{
  // Open serial communications
  Serial.begin(115200);
  Serial.println("@ Starting up...");
  Serial.println(freeRam());

  // Just blink a hello
  blink.set(Blink::BlinkMode::blink_black);
  
  // Use a jumper to set arduino #number
  arduinoNumber = setup_compute_dipswitch_number(PIN_DIPSWITCH);

  char buffer[30];
  snprintf(buffer,sizeof(buffer),MQTT_SHORT_NAME, (int) getArduinoNumber(), (int) UNIQUE_ID_ARDUINO_NUMBER);
  Serial.print("@ DIP SWITCH says our node is : "); Serial.println(buffer);
  Serial.println("@ Release " RELEASE_VERSION );

  // Fix static informations about our arduino
  current_mac[5] = UNIQUE_ID_ARDUINO_NUMBER;
  current_ip[3]  = UNIQUE_ID_ARDUINO_NUMBER; 
  
  // Setup ETHERNET / IP layer
  Ethernet.begin(current_mac,current_ip, current_dns, current_gw, current_subnet);
  Serial.print("@@@@ MAC : "); printMacAddress(); 
  Serial.print("@@@@ IP : ");Serial.print(Ethernet.localIP());Serial.println("");

  // Dead Ethernet hardware!
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    blink.set(Blink::BlinkMode::blink_fast);    
    while (1) {
      blink.loop();
      delay(10);
    }
  }
  // Delay for the harware... is this Cargo Cult ?
  delay(100);    
}

// COMMON LOOP ------------------------------------------------------------
void common_loop()
{
  #ifdef MODE_INPUT
  input_loop();
  #endif
}

// ONE TIME INIT ----------------------------------------------------------
void setup()
{
  blink.setup();
  setup_common();

  Serial.println(freeRam());
  #ifdef MODE_INPUT
  setup_input();
  #endif
  Serial.println(freeRam());
  
  #ifdef MODE_OUTPUT
  // Setup outpin pins for shift registers
  outputShiftRegister.setup();
  #endif
  Serial.println("setup ends....");
}

// NORMAL LOOP ----------------------------------------------------------
void loop()
{
  //Serial.println("loop begins....");
  
  // animate status
  blink.loop();
  
  // MQTT Initial connect/etc.. or recover connection
  if (!mqttClient.connected())
    mqttClientConnect();

  #if 1
  // common loop interest
  common_loop();
  
  // animate status
  blink.loop();

  // MQTT Loop
  mqttClient.loop();    
  #endif
}
