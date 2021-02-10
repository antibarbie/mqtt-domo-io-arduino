#define WITH_DUMP_TEMP
//#define WITH_DUMP_LIST

#ifdef WITH_DUMP_LIST
// fwd
void dumpList(int deviceCount, DallasTemperature & sensors);
#endif

struct namedOneWireDevice {
  int mysensorsid;
  const char * tag;
  DeviceAddress dev;
  float temp;
  bool changed;
  int pinHint;
};

#define IO_ID_TEMP1 250
#define IO_ID_TEMP2 249
#define IO_ID_TEMP3 248
#define IO_ID_TEMP4 247
#define IO_ID_TEMP5 246

namedOneWireDevice ds1820[] = {

  
{ IO_ID_TEMP1, "temp_martin", {  0x10, 0x88, 0xE3, 0x35, 0x01, 0x08, 0x00, 0x30 }, DEVICE_DISCONNECTED_C, false, 0 },
{ IO_ID_TEMP2,"temp_entree", {  0x10, 0xA4, 0x86, 0x4A, 0x01, 0x08, 0x00, 0x0A }, DEVICE_DISCONNECTED_C, false, 0 },
{ IO_ID_TEMP3, "temp_sejour", {  0x28, 0x01, 0xFA, 0xAF, 0x02, 0x00, 0x00, 0x4E }, DEVICE_DISCONNECTED_C, false, 0 },
{ IO_ID_TEMP4,"temp_ch2", {  0x28, 0x9A, 0xC1, 0xFA, 0x02, 0x00, 0x00, 0xAA }, DEVICE_DISCONNECTED_C, false, 0 },
{ IO_ID_TEMP5,"temp_chp", {  0x28, 0xC7, 0x05, 0xB0, 0x02, 0x00, 0x00, 0x08 }, DEVICE_DISCONNECTED_C, false, 0 },
{ 245, "deviceB", {  0x10, 0x9F, 0x05, 0x36, 0x01, 0x08, 0x00, 0x20 }, DEVICE_DISCONNECTED_C, false, 0 },
{ 244, "deviceD", {  0x10, 0x1E, 0x90, 0x41, 0x01, 0x08, 0x00, 0xB6 }, DEVICE_DISCONNECTED_C, false, 0 },
{ 243, "deviceE", {  0x10, 0xC0, 0xA7, 0x4B, 0x01, 0x08, 0x00, 0xAF }, DEVICE_DISCONNECTED_C, false, 0 },
};



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

      for(namedOneWireDevice & z : ds1820)
      {
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
        Serial.print(z.tag);
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

class ManyDS18X {
  DS18X ** _ds18;
  int _count;
  public:
  template<size_t N>
  ManyDS18X(int (&pins)[N]);
  void loop();
  void setup();
  void update_ds1820_variations();
  void presentation();
  
};

template<size_t N> ManyDS18X::ManyDS18X(int (&pins)[N])
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
void ManyDS18X::setup()
{
  for(int q = 0; q < _count; q++)
  {
     _ds18[q]->setup();
  }
}

MyMessage msgtemp(0,V_TEMP);

void ManyDS18X::update_ds1820_variations() {
  for (namedOneWireDevice & device : ds1820) {
    if (device.changed) {
      device.changed = false;
      // renvoyer un message
      msgtemp.setSensor(device.mysensorsid);
      msgtemp.set(device.temp, 2);
      send(msgtemp, false);
      #if 1
      Serial.print("Temperature updated : ");
      Serial.println(device.temp);      
      #endif
    }
  }
}


void ManyDS18X::presentation() {
  for (namedOneWireDevice & device : ds1820) {
    if (device.changed) {
      device.changed = false;
      // Affichage initial
      present(device.mysensorsid, S_TEMP, device.tag, false);
    }
  }  
}

/////////////////////////////////////////////////////////////////////////////


int onewirepins[] = {7};//,8,9};
ManyDS18X temperature_sensors(onewirepins);
