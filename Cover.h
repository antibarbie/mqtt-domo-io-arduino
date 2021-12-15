#define NO_VALUE 0xFF

enum CoverState {
  Not_known,
  Closing,
  Closed,
  Opening,
  Opened,
  Stopped
};


/// Handling roller cover
struct Cover
{
 
public:
    /// MQTT cover topic   MDB/VR/name ; /status (opening,...) /pos (0..100) /pos/set (setpoint 0..100)
    const char * topic_cover;
    /// MQTT topic for UP output
    const char * topic_up;
    /// MQTT topic for DW output
    const char * topic_dw;
    /// MQTT topic for button UP input
    const char * topic_bt_up;
    /// MQTT topic for button DW input
    const char * topic_bt_dw;

    /// Time in millis to completely go up (excluding lag)
    word time_up;
    /// Time in millis to completely go down (excluding lag)
    word time_dw;
    /// Lag between power on and actual movement
    word time_lag;
    /// Time margin for full_up and full_down (extra time to compensate)
    word time_margin;
    /// Rotation time for SUN BLADES, after "lag" and before actual up/down movement
    //short time_tilt;


    /// Actual position
    byte actual_pos;
    /// Actual state
    CoverState actual_state;
public:
    /// movement start time
    long millis_time_start;
    /// expected movement duration
    long millis_delta_time_expected;

    /// Setpoint position
    byte setpoint_pos;
    /// Memorized position
    byte memo_pos;
    /// Memorized setpoint position
    byte memo_setpoint_pos;

    /// Currently set to up
    bool status_up;
    /// Currently set to dw
    bool status_dw;

public:
  Cover();
  Cover(const char * cv, const char * up, const char * dw, const char * btup, const char * btdw, int tup, int tdw, int tlag, int tmargin);
  /// Common loop
  void Loop();
  /// MQTT Callback 
  void Callback(char* topic, byte* payload, unsigned int length);
  /// Setup
  static void Setup(bool (* fun)(const char*, const char*, bool));
  
private:
  // TRIGGER MOVEMENT 
  void loop_testTrigger();
  // TEST END MOVEMENT
  void loop_testEndOfMovement();
  // STATE
  void loop_updateState();
  // TEST MOVING
  void loop_testMoving();
  // UPDATE POSITION
  void loop_updatePosition();

private:
  // Helper function : stop the cover
  void StopMovement();

private:
  //  static publish function pointer
  static bool (* publish_generic)(const char * topic, const char * payload, bool retain);  
};

//  static publish function pointer
bool (* Cover::publish_generic)(const char * topic, const char * payload, bool retain) = 0;

Cover::Cover()
: actual_pos(NO_VALUE),
  millis_time_start(0),
  millis_delta_time_expected(0),
  setpoint_pos(NO_VALUE),
  memo_pos(NO_VALUE),
  memo_setpoint_pos(NO_VALUE)
{}
Cover::Cover(const char * cv, const char * up, const char * dw, const char * btup, const char * btdw, int tup, int tdw, int tlag, int tmargin)
: topic_cover(cv),
  topic_up(up),
  topic_dw(dw),
  topic_bt_up(btup),
  topic_bt_dw(btdw),
  time_up(tup),
  time_dw(tdw),
  time_margin(tmargin),
  time_lag(tlag),
  actual_pos(NO_VALUE),
  millis_time_start(0),
  millis_delta_time_expected(0),
  setpoint_pos(NO_VALUE),
  memo_pos(NO_VALUE),
  memo_setpoint_pos(NO_VALUE)
{
}

void Cover::StopMovement()
{
  // STOP
  publish_generic( topic_dw, "0", true);
  publish_generic( topic_up, "0", true);
  status_up = false;
  status_dw = false;      
  // --------------END :
  setpoint_pos = NO_VALUE;
  memo_setpoint_pos = NO_VALUE;
  millis_delta_time_expected = 0;
  millis_time_start = 0;
  // ------------- ^^ END ^^  
}

void Cover::Callback(char* topic, byte* payload, unsigned int length)
{
  bool on = length == 1 && (char)payload[0] == '1';
  bool off = length == 1 && (char)payload[0] == '0';
  
  // *** Test IN TOPIC => setpoint 0/100
  if (!strcmp(topic, topic_bt_up) && on) {
    // A good way to stop ?
    if (status_up)
      setpoint_pos = actual_pos;
    else     
      setpoint_pos = 100;
    Serial.print(topic_cover);Serial.println(" BT UP received");    
  } else if (!strcmp(topic, topic_bt_dw) && on) {
    // A good way to stop ?
    if (status_dw)
      setpoint_pos = actual_pos;
    else
      setpoint_pos = 0;
    Serial.print(topic_cover);Serial.println(" BT DW received");        
  // *** Test OUT TOPIC => status dw/up
  } else if (!strcmp(topic, topic_up)) {
    if (on) {Serial.print(topic_cover);Serial.println(" OUT UP received"); }
    status_up = on;
  } else if (!strcmp(topic, topic_dw)){
    if (on) {Serial.print(topic_cover);Serial.println(" OUT DW received"); }
    status_dw = on;
  }
  else 
  {
    // Specific cover
    auto covername_len = strlen(topic_cover);
    if (!strncmp(topic_cover, topic, covername_len))
    {
       Serial.print(topic_cover);Serial.println(" PREFIX command detected");
       Serial.println(topic);        
      
      // *** Test /set TOPIC => commands OPEN CLOSE STOP
      if (!strcmp(topic + covername_len, "/set"))
      {
        Serial.print(topic_cover);Serial.println(" SET command received");        
        Serial.println(topic);        
        
        if (length == 4 && !memcmp((char*)payload,"OPEN",4)) {
          Serial.println("Got OPEN"); 
          setpoint_pos = 100;
        }else if (length == 5 && !memcmp((char*)payload,"CLOSE",5)) {
          Serial.println("Got CLOSE"); 
          setpoint_pos = 0;
        }else if (length == 4 && !memcmp((char*)payload,"STOP",4)) {
          Serial.println("Got STOP"); 
          StopMovement();
        }else {
          Serial.print("Payload not ok. Len ="); Serial.println(length);
          for (int i = 0; i < min(length,9); i++)
          Serial.print((char)(payload[i])); 
          Serial.println(" ...");
        }
        return;
      }
 
      int value_payload = length <= 5 ? atoi((char*)payload): -1;
      if (value_payload < 0 || value_payload > 100)
        return;
      
      // *** Test /pos/set TOPIC => setpoint value
      if (!strcmp(topic + covername_len, "/pos/set"))
      {
        Serial.print(topic_cover);Serial.println(" SETPOINT value received");
        setpoint_pos = value_payload;
      }
      // *** Test /pos TOPIC => set initial pos
      else if (!strcmp(topic + covername_len, "/pos"))
      {
        if (actual_pos == NO_VALUE) {
          Serial.print(topic_cover);Serial.print(" POS value updated to ");Serial.println(value_payload);
          actual_pos = value_payload;
        }
      }
    }
    
  }
}

/// Setup
void Cover::Setup(bool (* fun)(const char*, const char*, bool))
{
  publish_generic = fun;
}

void Cover::Loop()
{
  // 1) TRIGGER MOVEMENT 
  loop_testTrigger();
  // 2) TEST END MOVEMENT
  loop_testEndOfMovement();
  // 3) STATE
  loop_updateState();
  // 4) TEST MOVING
  loop_testMoving();
  // 5) UPDATE POSITION
  loop_updatePosition();  
}

// 1) TRIGGER MOVEMENT 
void Cover::loop_testTrigger()
{
  if (setpoint_pos != NO_VALUE)
  {
    Serial.print(topic_cover);Serial.print(" found SETPOINT =");Serial.print((int)setpoint_pos);
    Serial.print(" and ACTUAL_POS =");Serial.println((int)actual_pos);
    
    memo_setpoint_pos = setpoint_pos;
    setpoint_pos = NO_VALUE;

    // Fail if current position is not known and set neither 0 nor 100
    if (actual_pos == NO_VALUE && memo_setpoint_pos != 0 && memo_setpoint_pos != 100)
      return;

    //START
    millis_time_start = millis();
    memo_pos = actual_pos;
    
    if (memo_setpoint_pos == 0 && actual_pos == NO_VALUE)  // degenerated case
    {
      publish_generic( topic_up, "0", true);
      publish_generic( topic_dw, "1", true);
      millis_delta_time_expected = time_lag + time_dw +time_margin;
    }
    else if (memo_setpoint_pos == 100 && actual_pos == NO_VALUE) // degenerated case
    {
      publish_generic( topic_dw, "0", true);
      publish_generic( topic_up, "1", true);
      millis_delta_time_expected = time_lag + time_up +time_margin;      
    }
    else if (memo_setpoint_pos >= 0 && memo_setpoint_pos <= 100 && memo_setpoint_pos != actual_pos)
    {
      if (memo_setpoint_pos > actual_pos)
      {
        // UP
        publish_generic( topic_dw, "0", true);
        publish_generic( topic_up, "1", true);
        millis_delta_time_expected = (long)((float)time_lag + (float)time_up * ((float)memo_setpoint_pos - (float)actual_pos) / 100.0f + (memo_setpoint_pos == 100 ? (float)time_margin : 0.0f)) ;        
      }
      else
      {
        // DW
        publish_generic( topic_up, "0", true);
        publish_generic( topic_dw, "1", true);
        millis_delta_time_expected = (long)( (float)time_lag + (float)time_dw * ((float)actual_pos - (float)memo_setpoint_pos) / 100.0f+ (memo_setpoint_pos == 0 ? (float)time_margin : 0.0f));                
      }
    }
    else if (memo_setpoint_pos == actual_pos)
    {
      StopMovement();
    }
    // Debug LOG
    if (millis_delta_time_expected > 0)
    {
      Serial.print(topic_cover);Serial.print(" START MOVE FOR (ms) : ");Serial.println((int)millis_delta_time_expected);
    }
  }
}
// 2) TEST END MOVEMENT
void Cover::loop_testEndOfMovement()
{
  if (memo_setpoint_pos != NO_VALUE)
  {
    if (millis() - millis_time_start > millis_delta_time_expected)
    {
      Serial.print(topic_cover);Serial.println(" END OF MOVE DETECTED");
      
      // Force theorical value
      actual_pos = memo_setpoint_pos;

      // Force stop and cleanup values
      StopMovement();

      // PUBLISH POS
      char txt[40], valuetext[4];
      // Create topic : e.g.   HOME/COVER/johnny's room/pos
      snprintf(txt, sizeof(txt), "%s/pos", topic_cover); 
      txt[sizeof(txt) - 1] = 0; // force terminal zero
      snprintf(valuetext, sizeof(valuetext), "%d", actual_pos); 
      publish_generic( txt, valuetext, true);
       
    }
  }
}


// 3) STATE
void Cover::loop_updateState()
{
  auto state_s = Not_known;
  if (status_dw)
    state_s = Closing;
  else if (status_up)
    state_s = Opening;
  else if (actual_pos == 0)
    state_s = Closed;
  else if (actual_pos == 100)
    state_s = Opened;
  else
    state_s = Stopped; 

  // should we end a status update
  if (state_s != actual_state)
  {
    auto state_txt = "unknown";
    switch(state_s) {
      case Closing:
        state_txt = "closing";
        break;
      case Closed:
        state_txt = "closed";
        break;
      case Opening:
        state_txt = "opening";
        break;
      case Opened:
        state_txt = "opened";
        break;
      case Stopped:
        state_txt = "stopped";
        break;
    }
    Serial.print(topic_cover);Serial.print(" from state =");Serial.print(actual_state);Serial.print(" new state =");Serial.println(state_txt);

    actual_state = state_s;
    
    char txt[40];
    // Create topic : e.g.   HOME/COVER/johnny's room/state
    snprintf(txt, sizeof(txt), "%s/state", topic_cover); 
    txt[sizeof(txt) - 1] = 0; // force terminal zero
    publish_generic( txt, state_txt, false);
  }
  
}

// 4) TEST MOVING
void Cover::loop_testMoving()
{
  // If something is moving, try to setup our values anyway
  if (memo_setpoint_pos == NO_VALUE && setpoint_pos == NO_VALUE && millis_time_start == 0 && (status_up || status_dw))
  {
    Serial.print(topic_cover);Serial.print(" start moving detected pos=");Serial.println((int)actual_pos);
    
    //START
    millis_time_start = millis();
    memo_pos = actual_pos;
  }
}

// 5) UPDATE POSITION
void Cover::loop_updatePosition()
{
  if (memo_pos != NO_VALUE && millis_time_start != 0 && (status_up || status_dw))
  {
    long deltaTime = millis() - millis_time_start - time_lag;
    // Do not recompute position if we are during the "lag"
    if (deltaTime < 0)
      return;
    
    auto estimated_pos = ((int)memo_pos) + (int)((status_up ? 1.0f : -1.0f) * (float)(deltaTime) / (float)(status_up ? time_up : time_dw) * 100.0f);
    
    //Serial.print("est. pos:"); Serial.println(estimated_pos);
    
    estimated_pos = max(0, min(estimated_pos, 100));

    if ((int)actual_pos != estimated_pos)
    {
      //Serial.print(topic_cover);Serial.print(" pos updated from=");Serial.print((int)actual_pos);Serial.print(" to=");Serial.println((int)estimated_pos);
      
      actual_pos = estimated_pos;
  
      char txt[40], valuetext[4];
      // Create topic : e.g.   HOME/COVER/johnny's room/pos
      snprintf(txt, sizeof(txt), "%s/pos", topic_cover); 
      txt[sizeof(txt) - 1] = 0; // force terminal zero
      snprintf(valuetext, sizeof(valuetext), "%d", actual_pos); 
      publish_generic( txt, valuetext, true);
    }
  }
}
