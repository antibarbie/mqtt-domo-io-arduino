/// Led blinker
class Blink {
  public:
  #define BLK(light,dark) ( ((unsigned long)((dark)&0xFFFF)<<16) | (unsigned long)((light) & 0xFFFF) )
  #define BLK_light(v) ( ((unsigned long)((v)&0xFFFF)) )
  #define BLK_dark(v) ( ((unsigned long)(((v)>>16)&0xFFFF)) )
  
  enum BlinkMode {
    blink_black = BLK(100,2000),
    blink_slow= BLK(1000,2000),
    blink_fast = BLK(40,200),
    blink_white  = BLK(3000,40),   
    blink_whitetics  = BLK(200,40),
  };
  private:
  
  /// Default blinking
  BlinkMode _normal_blink = blink_black;
  /// Current blinking
  BlinkMode _current_blink = blink_black;
  /// Output pin
  byte _ledPin;
  /// Blink cycles
  byte _cyclesCount = 0;
  /// ledState used to set the LED
  bool _ledState; 
  /// will store last time LED was updated
  unsigned long _previousMillis;

public:
  /// Normal construction
  Blink(int ledPin) {
    _ledPin = ledPin;
  }
  /// Activates a blinking cycle
  void set(BlinkMode mode = blink_fast, int cycles = 0) {
     _current_blink = mode;  
     _cyclesCount = cycles;

     // apply immediately
     if (_cyclesCount) _cyclesCount++;
     apply();
  }  
public:
  /// apply blinking
  void apply() {
    loop();
  }
  
  void setup() {
    // initialize digital pin as an output.
    pinMode(_ledPin, OUTPUT);  
  }
  
  void loop() {
    // check to see if it's time to change the LED
    unsigned long currentMillis = millis();
    unsigned long intervalx =  _ledState ? BLK_light(_current_blink) : BLK_dark(_current_blink);
      
    if (currentMillis - _previousMillis >= intervalx) {
      // save the last time you blinked the LED
      _previousMillis = currentMillis;
  
      // if the LED is off turn it on and vice-versa:
      _ledState = ! _ledState;

      // special blink for some cycles
      if (_cyclesCount) {
        _cyclesCount--;
        if (_cyclesCount == 0)
          _current_blink = _normal_blink;
      }
  
      // set the LED with the ledState of the variable:
      digitalWrite(_ledPin, _ledState);
    }
  }
};//blink
