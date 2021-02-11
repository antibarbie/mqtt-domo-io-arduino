#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <string.h>

#define HACK_FIX_LAST_TWO_BITS // Hardware V2.1 has wrong inputs order

#include "ShiftOutput.h"
#include "ShiftInput.h"
#include "Blink.h"
#include "Cover.h"

#define RELEASE_VERSION "0.7 - 02/2021"

// ----------------------------------------------------------------
// Arduino modules
// - INPUT: Read informations on chips and publish them to MQTT
// - OUTPUT: Subscribe to informations on MQTT and write them physically on chips
// - CORE: Subscribe to informations on MQTT and publish computations on MQTT - print stuff on screen SSD1306
//
// - SENSOR: Read sensors and publishes them --- Other project ? ---
// ----------------------------------------------------------------

//#define MODE_INPUT
//#define MODE_OUTPUT
//#define MODE_CORE

#if !defined MODE_INPUT && !defined MODE_OUTPUT && !defined MODE_CORE 
#warning "Using a default MODE. You can should define MODE_CORE, MODE_INPUT or MODE_OUTPUT."
#define MODE_CORE
#endif

// ----------------------------------------------------------------------------
// SETTINGS TO MODIFY

#define MQTT_USER         "test"
#define MQTT_PASSWORD     "test"
#define MQTT_ROOT_TOPIC   "MDB"

// Update these with values suitable for your network.
byte current_mac[]    = {  0xDE, 0x2D, 0x8F, 0xBD, 0x8F, 0x01 }; // Last byte will be changed with ARDUINO#
byte current_ip[] = { 192, 168, 100, 0 };                   // Last byte will be changed with ARDUINO#
byte current_dns[] = { 192, 168, 100, 1 };
byte current_gw[] = { 192, 168, 100, 1 };
byte current_subnet[] = { 255, 255, 255, 0 };

// Target broker - key element for our mqtt network !
byte mqtt_broker_address[] = { 192, 168, 100, 46 };
const int mqtt_broker_port = 1883;

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// SETTINGS / NAMING


// MISC
// Common defs

// Reset the network is hardwired to the PIN 7
#define PIN_RESET_NETWORK 7

// The dip switches for node number are hardwired on A7 on the hardware module
#define PIN_DIPSWITCH A7
// The status led is on D2 on the hardware module
#define STATUS_LED 2

// Input defs

// The dip switches for chain length is on A6 on the hardware module
#define PIN_CHAINLENGTH A6
// The 74HC165E LD/PL (Parallel load) pin is on A0
#define PIN_INPUT_PL A0
// The 74HC165E CE (Clock enable) pin is on A1
#define PIN_INPUT_CE A1
// The 74HC165E CP (Clock) pin is on A2
#define PIN_INPUT_CP A2
// The 74HC165E Main data pin is on D3
#define PIN_INPUT_DATA0 3
// The 74HC165E "slave1" data pin is on D4
#define PIN_INPUT_DATA1 4
// The 74HC165E "slave2" data pin is on D5
#define PIN_INPUT_DATA2 5
// Option header OPT1 in INPUT BOARD
#define PIN_INPUT_OPTION1 6
// Option header OPT2 in INPUT BOARD
#define PIN_INPUT_OPTION2 9

// Output defs

// The output pin for the 74HC595 is hardwired on A0
#define PIN_OUTPUT_DATA A0
// The clock pin for the 74HC595 is hardwired on A1
#define PIN_OUTPUT_CLOCK A2
// The latch pin for the 74HC595 is hardwired on A2
#define PIN_OUTPUT_LATCH A1
// The output ENABLE pin for the 74HC595 is hardwired on A3
#define PIN_OUTPUT_OE A3

// MQTT

// ROOT/IN/1/28
// ROOT/OUT/2/31
// ROOT/STATUS/IN/1
// ROOT/STATUS/CORE/1
// ROOT/STATUS/OUT/0

// Watchdog for nodered logic - If nodered is running our CORE is inactive
#define MQTT_NODERED_WATCHDOG MQTT_ROOT_TOPIC "/NR/WATCHDOG"

#define MQTT_ALL_INPUT MQTT_ROOT_TOPIC "/IN/#"
#define MQTT_ALL_OUTPUT MQTT_ROOT_TOPIC "/OUT/#"
#define MQTT_ALL_STATUS MQTT_ROOT_TOPIC "/STATUS/#"
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
#include "DS18x.h"
#define MQTT_SHORT_NAME  "CORE NODE #%d - UID#%d"
#define MQTT_SHORT_TOPIC "/CORE/%d"
#endif


// Generic STATUS publish topic ROOT/STATUS/TYPE/node_id (FMT => %d)
#define MQTT_STATUS_PUBLISH_TOPIC MQTT_ROOT_TOPIC "/STATUS" MQTT_SHORT_TOPIC


// Last Will : alive => 0
#define MQTT_WILL_QOS 2
#define MQTT_WILL_MESSAGE "0"



// ----------------------------------------------------------------------------
// ARDUINO #

// Leaves room for some outputs, 2 core, 4 inputs
#define OUTPUT_BASE 248
#define CORE_BASE 244
#define INPUT_BASE 240

#ifdef MODE_OUTPUT
#define XBASE OUTPUT_BASE
#else
#ifdef MODE_INPUT
#define XBASE INPUT_BASE
#else
#define XBASE CORE_BASE
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
  // [@@@] 0U         [@ @] 318U         47k 22k 10k
  //                                      |   |   |
  // [@@ ] 1 1 0 : 3  [@  ] 1 0 0 : 1     1   2   3
  // [__@] 407U       [ @@] 178U
  //
  // [  @] 0 0 1 : 4  [ @@] 0 1 1 : 6
  // [@@ ] 510U       [@  ] 605U
  //
  // [@@@] 1 1 1 : 7  [@ @] 1 0 1 : 5
  // [___] 638U       [_@_] 559U
  //
  int dip = analogRead(pinDipSwitch);
  Serial.print("@@DEBUG:   DIP SWITCH value says: "); Serial.println(dip);

  const int id_table[] = { 0, 176, 317, 407, 510, 559, 605, 638 };
  int delta = 9999;
  int found_closer = 0;
  
  for (int i = 0; i < sizeof(id_table) / sizeof(int); i++)
    if ((abs(id_table[i] - dip) < delta))
    {
      delta = abs(id_table[i] - dip);
      found_closer = i;
    }      
  Serial.print("@@DEBUG:  Found DIP SWITCH: "); Serial.print(found_closer); Serial.print("  for value: "); Serial.println(dip);
  return found_closer;
}

// Return DIP SWITCH Arduino number
int getArduinoNumber()
{
  return arduinoNumber;
}

// Return UNIQUE ID Arduino number
#define UNIQUE_ID_ARDUINO_NUMBER ((byte)getArduinoNumber() + (byte)XBASE)




// --------------------------------------------------------------------------------------

// forward
void mqtt_input_callback(char* topic, byte* payload, unsigned int length);
void mqtt_output_callback(char* topic, byte* payload, unsigned int length);
void mqtt_core_callback(char* topic, byte* payload, unsigned int length);

// Common callback
void MqttMessageCallback(char* topic, byte* payload, unsigned int length) {

#if 0 // log everything
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
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

}

// ---------------------------------------------------------------------------

/// Ethernet object
EthernetClient ethClient;
/// MQTT Client object
PubSubClient mqttClient(mqtt_broker_address, mqtt_broker_port, MqttMessageCallback, ethClient);
/// Led blinker object
Blink blink(STATUS_LED);

// ---------------------------------------------------------------------------

#ifdef MODE_INPUT

// Not of much interest in topics for the inputs !
int mqtt_input_subscribe()
{
  return 1;
}
///
/// Input mode : no interest in subscriptions for now, but we publish stuff, at least
///
void mqtt_input_callback(char* topic, byte* payload, unsigned int length) {
  // NOP NOW
}

/// Input reader object
IShiftCommon * _current_input;

/// Callback  for inputs received
bool onInputButton(int inputIndex, bool inputStatus)
{
  char my_topic[sizeof(MQTT_IO_PUBLISH_TOPIC) + 2]; // "%d%d" turns into "999999" at worst so +2
  
#ifdef LINEAR_INPUT
  // Publish to base/IN/<id>/<flatindex>
  snprintf(my_topic, sizeof(my_topic), MQTT_IO_PUBLISH_TOPIC, getArduinoNumber(), inputIndex);
#else
  // Publish to base/IN/<id + index / 32 (slave modules)>/<index % 32  (per module)>
  auto arduinoModule = getArduinoNumber() + inputIndex / 32;
  snprintf(my_topic, sizeof(my_topic), MQTT_IO_PUBLISH_TOPIC, arduinoModule, inputIndex % 32);
#endif
  
  Serial.print("Publishing to '"); Serial.print(my_topic); Serial.print("' = "); Serial.println(inputStatus ? "1" : "0");

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

  // Read options !
  pinMode(PIN_INPUT_OPTION1, INPUT_PULLUP);
  pinMode(PIN_INPUT_OPTION2, INPUT_PULLUP);
  auto option1_no_slaves = digitalRead(PIN_INPUT_OPTION1) == LOW; // Option is pulled LOW when jumper is set
  auto option2_unknown = digitalRead(PIN_INPUT_OPTION2) == LOW;
  Serial.print("Read OPT1/OPT2 : "); Serial.print(option1_no_slaves?"On/":"Off/");Serial.println(option2_unknown?"On":"Off");
  
  // D3 : INPUT CHAIN 0
  // D4 : INPUT CHAIN 1
  // D5 : INPUT CHAIN 2
  // A0,A1,A2 : PL,CE,CP

  // A6: DIP SWITCH CHAIN LENGTH
  delay(2000); // enough time for the capacitors to load (otherwise we measure a wrong tension)
  byte _input_chain_length = setup_compute_dipswitch_number(PIN_CHAINLENGTH);
  Serial.print("Initialize CHAIN LENGTH : "); Serial.println(_input_chain_length);

  // Pour faire simple :
  // T > 0 : lire tout en local sur plein de bits
  switch (_input_chain_length)
  {
    default://int ploadPin,int clockEnablePin,int clockPin, int (&dataPins)[INS]
    case 0:  // T = 0, read local chips + slave1 + slave2
      if (option1_no_slaves)
        _current_input = new ShiftInput<32, 1, 32>(onInputButton, PIN_INPUT_PL, PIN_INPUT_CE, PIN_INPUT_CP, { PIN_INPUT_DATA0 });
      else
        _current_input = new ShiftInput<32, 3, 96>(onInputButton, PIN_INPUT_PL, PIN_INPUT_CE, PIN_INPUT_CP, {PIN_INPUT_DATA0, PIN_INPUT_DATA1, PIN_INPUT_DATA2});
      break;
    case 1:  // T = 1, read local + 1 chained chips
      _current_input = new ShiftInput<64, 1, 64>(onInputButton, PIN_INPUT_PL, PIN_INPUT_CE, PIN_INPUT_CP, {PIN_INPUT_DATA0});
      break;
    case 2:  // T = 2, read local + 2 chained chips
      _current_input = new ShiftInput<96, 1, 96>(onInputButton, PIN_INPUT_PL, PIN_INPUT_CE, PIN_INPUT_CP, {PIN_INPUT_DATA0});
      break;
    case 3:  // T = 3, read local + 3 chained chips
      _current_input = new ShiftInput<128, 1, 128>(onInputButton, PIN_INPUT_PL, PIN_INPUT_CE, PIN_INPUT_CP, {PIN_INPUT_DATA0});
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

// -------------------------------------------------------------------------------

#ifdef MODE_OUTPUT

ShiftOutput outputShiftRegister(PIN_OUTPUT_DATA, PIN_OUTPUT_CLOCK, PIN_OUTPUT_LATCH, PIN_OUTPUT_OE);


int mqtt_output_subscribe() // Very important for the outputs
{
  char my_topic[sizeof(MQTT_IO_SUBSCRIBE_TOPIC)];
  snprintf(my_topic, sizeof(my_topic), MQTT_IO_SUBSCRIBE_TOPIC, getArduinoNumber());
  Serial.print("Subscribing to '"); Serial.print(my_topic); Serial.println("'");
  return mqttClient.subscribe(my_topic);
}


/// apply the settings on the real world :
/// let's talk to all of these 74HC595 !
void on_output_value(int outputId, int current_value)
{
  outputShiftRegister.setBit(outputId, current_value != 0);
  outputShiftRegister.apply();
#if 0
  Serial.print("Outputs: "); Serial.println(outputShiftRegister.get(), BIN);
#endif
}

///
/// Output mode : we just read the MQTT topics, and apply the values to the output transistors
///
void mqtt_output_callback(char* topic, byte* payload, unsigned int length) {
  // Starts from our original topic : e.g. ROOT/OUT/3
  char my_topic[sizeof(MQTT_IO_SUBSCRIBE_TOPIC_COMMON)];
  snprintf(my_topic, sizeof(my_topic), MQTT_IO_SUBSCRIBE_TOPIC_COMMON, getArduinoNumber());
  int lt = strlen(my_topic);
  int ltt = strlen(topic);

  // If the topics starts correctly from the right value, and continues with a slash
  // also, keep out fool values !
  if (!strncmp(topic, my_topic, lt) && topic[lt] == '/' && (ltt - lt <= 3) && (length < 4)) {
    lt++;
    // Expected : numeric
    int outputId = atoi(& topic[lt]);
    // Expected : numeric payload
    int payload_length_retained = min(sizeof(my_topic) - 1, min(length, 10));
    memcpy(my_topic, payload, payload_length_retained); // numeric size.. not that large !
    my_topic[payload_length_retained] = 0;
    int my_topic_val = atoi(my_topic);
    on_output_value(outputId, my_topic_val); // apply the settings on the real world

#if 0
    Serial.print("OUTPUT: topic found for OUT "); Serial.print(outputId); Serial.print(" = "); Serial.println(my_topic_val);
#endif

  } else {
    Serial.print("OUTPUT: Unrecognized topic: "); Serial.println(topic);
  }
}

#endif

// -------------------------------------------------------------------------------

#ifdef MODE_CORE

#define WITH_COVER

//                    [NODE 0 (master)]  [Slave 1]    [Slave 2]
// flattened naming:   /IN/0/0-31        /IN/0/32-63  /IN/0/64-95
// cards/input naming: /IN/0/0-31        /IN/1/0-31   /IN/2/0-31
//

/// Status for delay input
struct StatusInputIO {

  /// 0 if inactive, != 0 millis() when input was last toggled on
  unsigned long start_millis;
  /// True if "on"
  bool current_status;
};

/// Definition of I/O table
struct CoreIODef
{
  /// Input MQTT topic
  const char *           input_topic;
  /// Output MQTT topic (default behaviour : toggle)
  const char *           output_topic;
  /// Output MQTT topic (reversed logic : security when two outputs cannot be active at the same time)
  const char *           output_topic_inv;
  /// Delay "on" in milliseconds (output automatically set to off after delay)
  unsigned short         max_impulse_on_ms;
  /// Behaviour : alternative : only on when input is on
  bool                   behave_only_on_when_input_on;
  /// Input Handled by NODE-RED or other supervisor when watdog is currently running
  bool                   handled_by_other_supervisor;
  /// Quick status
  StatusInputIO          input_status;
};


CoreIODef core_io_table[] = {
  
  // Wire 1 : 1..10 =>  IN/2/0 .. IN/2/9
 
  // CHP
  { "MDB/IN/2/0", "MDB/OUT/0/22" },//nb: Light 1
  { "MDB/IN/2/0", "MDB/OUT/0/25" },//nb: Light 2
  { "MDB/IN/2/1", "MDB/OUT/0/22" },
  { "MDB/IN/2/1", "MDB/OUT/0/25" },
  { "MDB/IN/2/4", "MDB/OUT/0/22" },
  { "MDB/IN/2/4", "MDB/OUT/0/25" },
  { "MDB/IN/2/5", "MDB/OUT/0/22" },
  { "MDB/IN/2/5", "MDB/OUT/0/25" },
  // CHP - SDB
  { "MDB/IN/2/6", "MDB/OUT/0/23" },
  { "MDB/IN/2/7", "MDB/OUT/0/24" },

#ifndef WITH_COVER
  // VR parental suite
  { "MDB/IN/2/2", "MDB/OUT/1/0", "MDB/OUT/1/1" },
  { "MDB/IN/2/3", "MDB/OUT/1/1", "MDB/OUT/1/0" },
  // VR bath parental suite
  { "MDB/IN/2/8", "MDB/OUT/1/2", "MDB/OUT/1/3" },
  { "MDB/IN/2/9", "MDB/OUT/1/3", "MDB/OUT/1/2" },
#endif

  // Wire 6 : 1..11 => IN/1/16..IN/1/26
  
  // CHC
  { "MDB/IN/1/16", "MDB/OUT/0/20" },
  // CHM
  { "MDB/IN/1/19", "MDB/OUT/0/21" },
#ifndef WITH_COVER
  // VR room 2
  { "MDB/IN/1/17", "MDB/OUT/1/6", "MDB/OUT/1/7" },
  { "MDB/IN/1/18", "MDB/OUT/1/7", "MDB/OUT/1/6" },
  // VR room 1
  { "MDB/IN/1/20", "MDB/OUT/1/4", "MDB/OUT/1/5" },
  { "MDB/IN/1/21", "MDB/OUT/1/5", "MDB/OUT/1/4" },
#endif

  // Wire 5 : 1..10 : IN/1/0 .. IN/1/8 (9?)
  
  // Salon + ext
  { "MDB/IN/1/4", "MDB/OUT/0/11" },
  { "MDB/IN/1/5", "MDB/OUT/0/13" },//ext ouest
  { "MDB/IN/1/8", "MDB/OUT/0/11" },
  { "MDB/IN/1/24", "MDB/OUT/0/11" },
  
  // Séjour + ext
  { "MDB/IN/1/0", "MDB/OUT/0/10" },
  { "MDB/IN/0/19", "MDB/OUT/0/10" },//
  { "MDB/IN/1/1", "MDB/OUT/0/14" },//ext sud
  { "MDB/IN/1/23", "MDB/OUT/0/10" },

#ifndef WITH_COVER
  // VR roll living saloon
  { "MDB/IN/1/2", "MDB/OUT/1/10", "MDB/OUT/1/11" },//Living
  { "MDB/IN/1/3", "MDB/OUT/1/11", "MDB/OUT/1/10" },
  { "MDB/IN/1/6", "MDB/OUT/1/12", "MDB/OUT/1/13" },//Saloon
  { "MDB/IN/1/7", "MDB/OUT/1/13", "MDB/OUT/1/12" },

  // Wire 4 : 1..11 => IN/0/16..IN/0/26

  // VR roll living kitchen saloon
  { "MDB/IN/0/20", "MDB/OUT/1/8", "MDB/OUT/1/9"  },//Kitchen
  { "MDB/IN/0/21", "MDB/OUT/1/9", "MDB/OUT/1/8"  },
#endif

  // Switch on/off kitchen -> Electric VMC trap
  { "MDB/IN/0/26", "MDB/OUT/1/21", NULL, 0, true }, // no toggle

  // Cuisine
  { "MDB/IN/0/17", "MDB/OUT/0/16" },
  { "MDB/IN/0/22", "MDB/OUT/0/16" },
  // Bar
  { "MDB/IN/0/18", "MDB/OUT/0/17" },
  { "MDB/IN/0/23", "MDB/OUT/0/17" },
  // Bureau
  { "MDB/IN/0/24", "MDB/OUT/0/12" },
  { "MDB/IN/0/25", "MDB/OUT/0/12" },
  // Cellier
  { "MDB/IN/0/16", "MDB/OUT/0/15" },


  // Wire 7: 1..6 IN/2/10..IN/2/15
  
  // Local tech
  { "MDB/IN/2/13", "MDB/OUT/1/16" },
  
  // Bua
  { "MDB/IN/2/10", "MDB/OUT/1/17" },//Bua Main
  { "MDB/IN/2/11", "MDB/OUT/1/18" },//Bua Other
  { "MDB/IN/2/12", "MDB/OUT/0/5" }, //grenier

  // WC
  { "MDB/IN/2/15", "MDB/OUT/1/19" },

  // Wire 8 : 1..8 => IN/2/16 .. IN/2/23
    
  // SDB
  { "MDB/IN/2/22", "MDB/OUT/0/18" },//SDB Main
  { "MDB/IN/2/23", "MDB/OUT/0/19" },//SDB Other

  // Wire 2 : 1..7 => IN/0/0 .. 
  // Wire 3 : 1 => IN/0/9 
  
  // Garage + ext
  { "MDB/IN/0/0", "MDB/OUT/0/6" },
  { "MDB/IN/0/1", "MDB/OUT/0/6" },
  { "MDB/IN/0/2", "MDB/OUT/0/6" },
  { "MDB/IN/0/3", "MDB/OUT/0/3" },//Ext
  // Grenier
  { "MDB/IN/0/9", "MDB/OUT/0/5" },
  // Cave 1/2/3
  { "MDB/IN/0/4", "MDB/OUT/0/0" },
  { "MDB/IN/0/5", "MDB/OUT/0/1" },
  { "MDB/IN/0/6", "MDB/OUT/0/2" },

  // Couloir
  { "MDB/IN/2/14", "MDB/OUT/0/8" }, //inversé avec wc
  { "MDB/IN/2/18", "MDB/OUT/0/8" },
  { "MDB/IN/2/21", "MDB/OUT/0/8" },
  { "MDB/IN/1/22", "MDB/OUT/0/8" },
  { "MDB/IN/1/25", "MDB/OUT/0/8" },

  // Entrée
  { "MDB/IN/2/16", "MDB/OUT/0/7" },
  { "MDB/IN/2/17", "MDB/OUT/0/7" },
  { "MDB/IN/2/20", "MDB/OUT/0/7" },
  { "MDB/IN/1/26", "MDB/OUT/0/7" },

  //


  // Push Switch entrance -> ring bell
  { "MDB/IN/2/19", "MDB/OUT/1/22", NULL, 0, true }, // no toggle

  // TEST ONLY
  //{ "MDB/IN/0/56", "MDB/OUT/1/2", "MDB/OUT/1/1" },
  //{ "MDB/IN/0/28", "MDB/OUT/1/3", NULL, 2000 },
  //{ "MDB/IN/0/29", "MDB/OUT/1/1", NULL, 1000 },

  // END
  { NULL, NULL, NULL },

};

#ifdef WITH_COVER

#define MQTT_COVER_ALL "MDB/VR/#"

Cover cover_table[] = {
  // MQTT cover topics   MDB/VR/name ; /status (opening,...) /pos (0..100) /pos/set (setpoint 0..100)
  //topic_cover, topic_up,      topic_dw,       topic_bt_up, topic_bt_dw, time_up, time_dw, time_margin, time_lag
  Cover("MDB/VR/CH1", "MDB/OUT/1/4", "MDB/OUT/1/5", "MDB/IN/1/20","MDB/IN/1/21",15000,  14600,    500,        400),
  Cover("MDB/VR/CH2", "MDB/OUT/1/6", "MDB/OUT/1/7", "MDB/IN/1/17","MDB/IN/1/18",15000,  14600,    500,        400),
  Cover("MDB/VR/CHP", "MDB/OUT/1/0", "MDB/OUT/1/1", "MDB/IN/2/2","MDB/IN/2/3",  15000,  14600,    500,        400),
  Cover("MDB/VR/SDB2","MDB/OUT/1/2", "MDB/OUT/1/3", "MDB/IN/2/8","MDB/IN/2/9",   9000,   9000,    500,        400),
  
  Cover("MDB/VR/SEJ","MDB/OUT/1/10", "MDB/OUT/1/11", "MDB/IN/1/2","MDB/IN/1/3",55000,  53000,    500,         500),
  Cover("MDB/VR/SAL","MDB/OUT/1/12", "MDB/OUT/1/13", "MDB/IN/1/6","MDB/IN/1/7",55000,  53000,    500,         500),
  Cover("MDB/VR/CUI","MDB/OUT/1/8", "MDB/OUT/1/9", "MDB/IN/0/20","MDB/IN/0/21",21000,  21000,    500,         500),
  Cover( )
};
#endif

int mqtt_core_subscribe() // Very important for the core logic
{
  return mqttClient.subscribe(MQTT_ALL_INPUT)
      && mqttClient.subscribe(MQTT_NODERED_WATCHDOG)
      && mqttClient.subscribe(MQTT_ALL_OUTPUT)
#ifdef WITH_COVER     
      && mqttClient.subscribe(MQTT_COVER_ALL)
#endif
      ;
}

bool publish_generic(const char * topic, const char * payload, bool retain)
{
  return mqttClient.publish(topic, payload, retain);
}


void publish_output(const char * topic, bool status_active)
{
  if (topic != NULL)
  {
    bool ok = publish_generic(topic, status_active ? "1" : "0", true);
    //blink.set(ok ? Blink::BlinkMode::blink_white : Blink::BlinkMode::blink_fast);
  }
}

// WATCHDOG
#define WITH_WATCHDOG
#define WATCHDOG_MILLIS 2500
#define WATCHDOG_NOT_RECEIVED 9999

int previous_watchdog = WATCHDOG_NOT_RECEIVED;
long previous_watchdog_ms = 0;

///
/// Our common logic - listen to ALL inputs and apply output logic
///
void mqtt_core_callback(char* topic, byte* payload, unsigned int length) {
  // If its a watchdog
  if (!strcmp(MQTT_NODERED_WATCHDOG,topic))
  {
     char my_buffer[8];
     int payload_length_retained = min(sizeof(my_buffer) - 1, min(length, 6));
     memcpy(my_buffer, payload, payload_length_retained); // numeric size.. not that large !
     my_buffer[payload_length_retained] = 0;
     int new_watchdog_value = atoi(my_buffer);
     if (new_watchdog_value != previous_watchdog) {
        previous_watchdog = new_watchdog_value;
        previous_watchdog_ms = millis();
     }    
     return;
  }

#ifdef WITH_WATCHDOG
  // Test watchdog : we are active if the watchdog is out of delay !
  if ( millis() - previous_watchdog_ms > (unsigned long)WATCHDOG_MILLIS )
  {
      previous_watchdog = WATCHDOG_NOT_RECEIVED;
  }
  // Ignore some messages when others are sending the watchdogs correctly !
  // Otherwise we apply our simple yet effective logic !
  bool supervisor_active = (WATCHDOG_NOT_RECEIVED != previous_watchdog);
#else
  bool supervisor_active = false;
#endif

// If it starts with a cover
#ifdef WITH_COVER
  for (int idx = 0; cover_table[idx].topic_cover != NULL; idx++)
  {
    cover_table[idx].Callback(topic, payload, length);
  }
#endif

  
  // If it starts with an input..

  // Loop in the INPUTs table
  for (int idx = 0; core_io_table[idx].input_topic != NULL; idx++)
  {
    // This is one of our input topic !
    if (!strcmp(topic, core_io_table[idx].input_topic)) 
    {
      Serial.print("Found topic :"); Serial.println(core_io_table[idx].input_topic);

      // Ignore some topics where another supervisor implement a more complicated logic
      if (supervisor_active && core_io_table[idx].handled_by_other_supervisor)
      {
        Serial.println("Topic handled by another supervisor.");
        continue;
      }


      // Value == 1 means input pressed
      bool on = length == 1 && (char)payload[0] == '1';

      // Classic switch / no toggle mode !
      if (core_io_table[idx].behave_only_on_when_input_on)
      {
        if (on)
          publish_output(core_io_table[idx].output_topic_inv, false);
        publish_output(core_io_table[idx].output_topic, on );
      }
      else if (core_io_table[idx].max_impulse_on_ms != 0 && on)
      {
        // Impulse with maximum length only
        auto timenow = millis();
        if (timenow == 0) timenow++;
        core_io_table[idx].input_status.start_millis = timenow;

        if (on)
          publish_output(core_io_table[idx].output_topic_inv, false);
        publish_output(core_io_table[idx].output_topic, on );
      }
      else if (on)// TOGGLE when clicked
      {
        bool out_on = ! core_io_table[idx].input_status.current_status;

        if (out_on)
          publish_output(core_io_table[idx].output_topic_inv, false);
        publish_output(core_io_table[idx].output_topic, out_on);
      }
      continue;
    }

    // ----------------------------------------------- 

    // Animate status !
    // This is one of our output topic !
    if (length == 1 && ((char)payload[0] == '1' || (char)payload[0] == '0') 
        && !strcmp(topic, core_io_table[idx].output_topic))
    {
        core_io_table[idx].input_status.current_status = ((char)payload[0] == '1');
    }    
  }
}

#define WITH_DS18


void setup_core()
{
#ifdef WITH_DS18
  temperature_sensors.setup();
#endif
  
  //  Cover roller handling
#ifdef WITH_COVER
  Cover::Setup(& publish_generic);
#endif
  
}


void core_loop()
{
#ifdef WITH_DS18
  temperature_sensors.loop();
#endif
  
  //  Cover roller handling
#ifdef WITH_COVER
  for (int idx = 0; cover_table[idx].topic_cover != NULL; idx++)
  {
    cover_table[idx].Loop();
  }
#endif
  

  // -----------------------------------------------------
  
  // handle the maximum time impulses
  for (int idx = 0; core_io_table[idx].input_topic != NULL; idx++)
    if (core_io_table[idx].input_status.start_millis != 0 &&
        ( millis() - core_io_table[idx].input_status.start_millis > (unsigned long)core_io_table[idx].max_impulse_on_ms )) // nb: the delta (now - start > delay) handles correctly the millis() rollover after 49 days !
    {
      core_io_table[idx].input_status.start_millis = 0;
      publish_output(core_io_table[idx].output_topic, false);
    }
}

#endif


// ---------------------------------------------------------------------------
bool test_good_ethernet(); // fwd
bool setup_network();//fwd


// MQTT Client connect/reconnect
void mqttClientConnect()
{
  // nb: our client name is our status topic
  char my_mqtt_status_topic[sizeof(MQTT_STATUS_PUBLISH_TOPIC) + 1];
  snprintf(my_mqtt_status_topic, sizeof(my_mqtt_status_topic), MQTT_STATUS_PUBLISH_TOPIC, getArduinoNumber());

  Serial.print("Attempting MQTT connect for "); Serial.println(my_mqtt_status_topic);

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
    // Setup the network again, effective in case of brown-out,
    // otherwise the program continues, but the ethernet layer loose its config.
    if (!test_good_ethernet())
      setup_network();
  }
}


// Setup common stuff
void setup_common()
{
  blink.setup();

  // Open serial communications
  Serial.begin(115200);
  Serial.print("@ Starting up... with ");  Serial.print(freeRam()); Serial.println(" free ram.");

  // Just blink a hello
  blink.set(Blink::BlinkMode::blink_whitetics);

  // Delay for the harware... is this Cargo Cult ?
  delay(100);

  // Network reset
  //pinMode( PIN_RESET_NETWORK, OUTPUT);
  //digitalWrite(PIN_RESET_NETWORK, LOW);
  //delay(100); // 500us minimum
  //digitalWrite(PIN_RESET_NETWORK, HIGH);

  // Setting up the network
  while (!setup_network())
  { 
    blink.loop();
  }
}

// Test if ethernet status is ok (could reset with more sensitivity than the arduino, in case of brown-out
// @return true if ok, false if problem with the network
bool test_good_ethernet()
{
  uint8_t _mac[6];
  Ethernet.MACAddress(_mac);
  return _mac[0] != 0;
}

// Setup Ethernet / IP network
// @return true if ok, false if problem with the network
bool setup_network()
{
  // Use a jumper to set arduino #number
  arduinoNumber = setup_compute_dipswitch_number(PIN_DIPSWITCH);

  char buffer[30];
  snprintf(buffer, sizeof(buffer), MQTT_SHORT_NAME, (int) getArduinoNumber(), (int) UNIQUE_ID_ARDUINO_NUMBER);
  Serial.print("@ DIP SWITCH says our node is : "); Serial.println(buffer);
  Serial.println("@ Release " RELEASE_VERSION );

  // Fix static informations about our arduino
  current_mac[5] = UNIQUE_ID_ARDUINO_NUMBER;
  current_ip[3]  = UNIQUE_ID_ARDUINO_NUMBER;

  // Setup ETHERNET / IP layer
  Ethernet.begin(current_mac, current_ip, current_dns, current_gw, current_subnet);
  
  // bad .. bad news !!
  if ((!test_good_ethernet()) || (Ethernet.hardwareStatus() == EthernetNoHardware)) 
  {
    blink.set(Blink::BlinkMode::blink_black);
    Serial.print("@@@@ Ethernet init failed.");
    return false;   
  }
  uint8_t _mac[6];
  Ethernet.MACAddress(_mac);
  
  Serial.print("@@@@ MAC : ");
  Serial.print(_mac[0], HEX); Serial.print(":"); Serial.print(_mac[1], HEX); Serial.print(":");
  Serial.print(_mac[2], HEX); Serial.print(":"); Serial.print(_mac[3], HEX); Serial.print(":");
  Serial.print(_mac[4], HEX); Serial.print(":"); Serial.print(_mac[5], HEX); Serial.println();
  Serial.print("@@@@ IP : "); Serial.print(Ethernet.localIP()); Serial.println("");

  return true;
}

// COMMON LOOP ------------------------------------------------------------
void common_loop()
{
#ifdef MODE_INPUT
  input_loop();
#endif
#ifdef MODE_CORE
  core_loop();
#endif

  // animate status
  blink.loop();
}

// ONE TIME INIT ----------------------------------------------------------
void setup()
{
  setup_common();

#ifdef MODE_INPUT
  setup_input();
#endif

#ifdef MODE_CORE
  setup_core();
#endif

#ifdef MODE_OUTPUT
  // Setup outpin pins for shift registers
  outputShiftRegister.setup();
#endif
}

// NORMAL LOOP ----------------------------------------------------------
void loop()
{
  // animate status
  blink.loop();

  // MQTT Initial connect/etc.. or recover connection
  if (!mqttClient.connected())
    mqttClientConnect();

  // common loop interest
  common_loop();

  // MQTT Loop
  mqttClient.loop();
}
