#include <OneWire.h>
#include <DallasTemperature.h>

//#define WITH_DUMP_TEMP
//#define WITH_DUMP_LIST

// Formats a device address to string
void printAddress(char * addressBuffer, size_t addressBufferSize, DeviceAddress a)
{
  if (addressBufferSize > 0) {
    addressBuffer[addressBufferSize - 1] = 0;
    snprintf(addressBuffer, addressBufferSize, "%02x%02x%02x%02x%02x%02x%02x%02x", a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
  }
}

#ifdef WITH_DUMP_LIST
// fwd
void dumpList(int deviceCount, DallasTemperature & sensors)
{
  DeviceAddress address;
  for (int idx = (int)sensors.getDeviceCount() - 1; idx >= 0 ; idx--)
    if (sensors.getAddress(address, idx))
    {
      Serial.print("Sensor found @");Serial.print(idx);Serial.print(" addr: ");
      char buff[18];
      printAddress(buff, sizeof(buff), address);
      Serial.println(buff);
    }
}
#endif


struct MemoOneWireDevice {
  DeviceAddress dev;
  float temp;
  bool changed;
  int pinHint;
};

#define KNOWN_DS1820 40
MemoOneWireDevice ds1820[KNOWN_DS1820];


/// Lecture d'objets température sur bus 1-wire
class DS18X {
   enum Phase {
    PHASE_BEGIN,
    PHASE_REQUEST,
    PHASE_WAIT,
    PHASE_READ,
    PHASE_SLEEP
   };
   Phase _phase;
   OneWire  _bus;
   DallasTemperature _sensors;
   int _pin;
   int _count;
   long _lastReadMillis;
   // millis for DS18x20 family is  ~750 ms depending on resolution
   const long delayRead = 1000;
   // millis between two reads
   const long delaySleep = 60000;
   
   // margin between changes in temperature
   const float changeMargin = 0.05f;
public:   
   /// Création du bus sur un pin donné
   DS18X(int busPin);
   /// Initialisation
   void setup();
   /// Boucle commune
   void loop();
   /// Scan des appareils
   
};
// Création
DS18X::DS18X(int busPin) : _bus(busPin), _sensors(&_bus), _pin(busPin),_count(0),_phase(PHASE_BEGIN) {  
}
void DS18X::setup() {
  _sensors.setResolution(12);
  _sensors.setWaitForConversion(false);
  _sensors.setCheckForConversion(false);  
}
void DS18X::loop() {
  #if 0
  Serial.print("-loop ");
  Serial.print(_pin);
  Serial.print(" phase ");
  Serial.println(_phase);
  #endif
  
  switch (_phase)
  {
    case PHASE_BEGIN:
      {
         _sensors.begin(); 
         int cnt = _sensors.getDeviceCount();
         if (_count != cnt)
         {
             for (int devId = 0; devId < cnt; devId++)
             {
                DeviceAddress adr;
                if (_sensors.getAddress(adr, devId))
                {
                  bool found = false;
                  // fill our memory list
                  for(MemoOneWireDevice & z : ds1820)
                  {
                    if (!memcmp(z.dev, adr, sizeof(DeviceAddress))) {
                      found = true;
                      break;
                    }
                  }
                  if (!found) {
                    for(MemoOneWireDevice & z : ds1820)
                    {
                      // search for empty address
                      if (z.dev[0] == 0 && z.dev[1] == 0)
                      {
                        memcpy(z.dev, adr, sizeof(DeviceAddress));
                        z.pinHint = _pin;
                        z.temp = DEVICE_DISCONNECTED_C;
                        z.changed = false;
                        break;
                      }
                    }
                           
                  }
                }
             }
#ifdef WITH_DUMP_LIST
             // dump ?
             dumpList(cnt, _sensors);
#endif
             _count = cnt;
         }
         _phase = PHASE_REQUEST;
      }
      break;
    case PHASE_REQUEST:
      _sensors.requestTemperatures(); // Send the command to get temperature readings 
      _phase = PHASE_WAIT;
      _lastReadMillis = millis();
      break;
    case PHASE_WAIT:
      if( millis() - _lastReadMillis > delayRead)
      {
        _phase = PHASE_READ;
      }
      break;
    case PHASE_READ:

      for(MemoOneWireDevice & z : ds1820)
      {
        // pas initialisé
        if (z.dev[0] == 0 && z.dev[1] == 0)
          continue;
        
        // pas branché sur notre pin à nous
        if (z.pinHint != 0 && z.pinHint != _pin)
          continue;
        float t = _sensors.getTempC(z.dev);
        if (t == DEVICE_DISCONNECTED_C)
          continue;
        // écrire en cas de changement de température
        if (z.temp == DEVICE_DISCONNECTED_C || fabs(z.temp - t) > changeMargin )
        {
          z.pinHint = _pin;
          z.temp = t;
          z.changed = true;
          
#ifdef WITH_DUMP_TEMP
        Serial.print(" - pin ");
        Serial.print(_pin);
        Serial.print(" / ");
        char buff[18];
        printAddress(buff, sizeof(buff), z.dev);
        
        Serial.print(buff);
        
        Serial.print(" : ");
        Serial.println(t);
#endif
          
        }
      }
    
      _phase = PHASE_SLEEP;
      _lastReadMillis = millis();
      break;
   case PHASE_SLEEP:
      if( millis() - _lastReadMillis > delaySleep)
      {
        _phase = PHASE_BEGIN;
      }
      break;
  }
}

/// Handle many DS18x sensors
class ManyDS18X {
  DS18X ** _ds18;
  int _count;
  const char * _common_topic;
public:
  template<size_t N>
  ManyDS18X(const int (&pins)[N]);
  
  void loop();
  /// Pass MQTT common prefix TOPIC and MQTT callback function during setup()
  void setup(const char * common_topic, bool (* fun)(const char*, const char*, bool));
  
private:
  void update_ds1820_variations();
private:
  //  static publish function pointer
  static bool (* publish_generic)(const char * topic, const char * payload, bool retain);  
  
};

template<size_t N> ManyDS18X::ManyDS18X(const int (&pins)[N])
{
  _count = N;
  _ds18 = new DS18X*[N];

  for(int q = 0; q < N; q++)
  {
     _ds18[q] = new DS18X(pins[q]);
  }
}
void ManyDS18X::loop()
{
  for(int q = 0; q < _count; q++)
  {
     _ds18[q]->loop();
  }
  update_ds1820_variations();
}

/// @param common_topic MQTT topic prefix e.g. "HOME/SENSORS/TEMP/" sensor unique ID will be postfixed
void ManyDS18X::setup(const char * common_topic, bool (* publish_generic_function)(const char*, const char*, bool))
{
  // MQTT Prefix (HERE BE DRAGONS: NOT OWNED BY US !)
  _common_topic = common_topic;
  // MQTT Callback
  publish_generic = publish_generic_function;
  
  // Initialize memory !
  for(MemoOneWireDevice & z : ds1820)
  {
    z.dev[0] = 0;
    z.dev[1] = 0;
    z.pinHint = 0;
    z.temp = DEVICE_DISCONNECTED_C;
    z.changed = false;
  }
  
  for(int q = 0; q < _count; q++)
  {
     _ds18[q]->setup();
  }
}

//  static publish function pointer
bool (* ManyDS18X::publish_generic)(const char * topic, const char * payload, bool retain) = 0;



void ManyDS18X::update_ds1820_variations() {
  for (MemoOneWireDevice & device : ds1820) {
    if (device.changed) {
      device.changed = false;

      // Send MQTT message  topic: HOME/PREFIX/SENSORS/  +  sensor_unique_uid  payload: "12.34"
      char xxbuff[18];
      printAddress(xxbuff, sizeof(xxbuff), device.dev);
      char topicbuff[128];
      snprintf(topicbuff,sizeof(topicbuff),"%s%s", _common_topic, xxbuff);
      char tempbuff[12];
      dtostrf(device.temp, 4, 2, tempbuff);
      publish_generic(topicbuff, tempbuff, false);
      
      #if 1
      Serial.print("Sensor : ");
      Serial.print(xxbuff);
      Serial.print(" Temperature updated : ");
      Serial.println(device.temp);      
      #endif
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
