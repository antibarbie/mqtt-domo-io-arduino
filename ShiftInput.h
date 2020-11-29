#define CHAR_BIT 8


namespace std{
  template <size_t N> class bitset;


  template <size_t N> bitset<N> operator&(const bitset<N>&, const bitset<N>&);
  template <size_t N> bitset<N> operator|(const bitset<N>&, const bitset<N>&);
  template <size_t N> bitset<N> operator^(const bitset<N>&, const bitset<N>&);
  
  //Actual Code


  template<size_t N> class  bitset {
  private:
    //Number of characters allocated to hold the bits
    static const size_t WORD_SIZE = CHAR_BIT;   //Use int maybe?
    static const size_t num_bytes = (N + WORD_SIZE - 1) / WORD_SIZE;

    //From the bit number, figure out which byte we are working with
    size_t byte_num(size_t bit_num) const{ 
      if(WORD_SIZE == 8){
        return (bit_num >> 3);
      }
      if(WORD_SIZE == 16){
        return (bit_num >> 4);
      }
      if(WORD_SIZE == 32){
        return (bit_num >> 5);
      }
      if(WORD_SIZE == 64){
        return (bit_num >> 6);
      }
      return bit_num / WORD_SIZE;
    }
    //From the bit number, figure out which bit inside the byte we need
    size_t bit_num(const size_t bit_num) const{
      return bit_num % WORD_SIZE;
    }


    //Point to the actual data
    char data[num_bytes];
    
  public:
#if 0
    class  reference {
      friend class bitset;
      reference() : bit_num(0), parent(0) {  }
      size_t bit_num;
      bitset * parent;
    public:
      ~reference() {  }
      reference& operator=(bool x){     // for b[i] = x;
        parent->set(bit_num, x);
        return *this;
      }
      reference& operator=(const reference& x){ // for b[i] = b[j];
        parent->set(bit_num, x);
        return *this;
      }
      bool operator~() const{       // flips the bit
        return !parent->test(bit_num);
      }
      operator bool() const{        // for x = b[i];
        return parent->test(bit_num);
      }
      reference& flip(){        // for b[i].flip();
        parent->flip(bit_num);
        return *this;
      }
    };
#endif

    bitset(){
      reset();
    }
    bitset(unsigned long val){
      reset();
      size_t count = sizeof(val) * CHAR_BIT;
      if(count > N){
        count = N;
      }
      for(size_t i = 0; i < count; ++i){
        set(i, ((val >> i) & 1));
      }
    }

    bitset(const bitset & val){
      for(size_t i = 0; i < num_bytes; ++i){
        data[i] = val.data[i];
      }
    }

    bitset<N>& operator&=(const bitset<N>& rhs){
      for(size_t i =0; i < num_bytes; ++i){
        data[i] &= rhs.data[i];
      }
      return *this;
    }

    bitset<N>& operator|=(const bitset<N>& rhs){
      for(size_t i =0; i < num_bytes; ++i){
        data[i] |= rhs.data[i];
      }
      return *this;
    }
    bitset<N>& operator^=(const bitset<N>& rhs){
      for(size_t i=0; i < num_bytes; ++i){
        data[i] ^= rhs.data[i];
      }
      return *this;
    }

    bitset<N>& operator<<=(size_t pos){
      for(size_t i = N-1; i >=pos; --i){
        set(i, test(i - pos));
      }
      for(size_t i = 0; i < pos; ++i){
        reset(i);
      }
      return *this;
    }

    bitset<N>& operator>>=(size_t pos){
      for(size_t i = 0; i < (N - pos); ++i){
        set(i, test(i + pos));
      }
      for(size_t i = pos; i > 0; --i){
        reset(N - i);
      }
      return *this;
    }

    bitset<N>& set(){
      size_t i;
      for(i = 0; i < N ; ++i){
        data[byte_num(i)] |= (1<<bit_num(i));
      }
      return *this;
    }
    bitset<N>& set(size_t pos, int val = true){
      if(val == true){
        data[byte_num(pos)] |= (1<<bit_num(pos));
      }else{
        data[byte_num(pos)] &= ~(1<<bit_num(pos));
      }
      return *this;
    }
    bitset<N>& reset(){
      for(size_t i = 0; i <= num_bytes; ++i){
        data[i] = 0;
      }
      return *this;
    }
    bitset<N>& reset(size_t pos){
      data[byte_num(pos)] &= ~(1<<bit_num(pos));
      return *this;
    }
    bitset<N>  operator~() const{
      bitset<N> retval(*this);
      retval.flip();
      return retval;
    }

    bitset<N>& flip(){
      for(size_t i = 0; i <= num_bytes; ++i){
        data[i] =  ~data[i];
      }
      return *this;
    }
    bitset<N>& flip(size_t pos){
      char temp = data[byte_num(pos)] & (1 << bit_num(pos));
      if(temp == 0){  //Bit was 0
        data[byte_num(pos)] |= (1 << bit_num(pos));
      }else{    //Bit was 1
        data[byte_num(pos)] &= ~(1<<bit_num(pos));
      }
      return *this;
    }
#if 0
    reference operator[](size_t pos){   // for b[i];
      reference retval;
      retval.parent = this;
      retval.bit_num = pos;
      return retval;
    }
#endif

    unsigned long to_ulong() const{
      if(N > sizeof(unsigned long) * CHAR_BIT){
        // __throw_overflow_error();
      }
      unsigned long retval = 0;
      size_t count = N;
      for(size_t i = count; i > 0; --i){
        if(test(i)){
          retval +=1;
        }
        retval<<=1;
      }
      if(test(0)){
        retval +=1;
      }
      return retval;
    }

    void print(HardwareSerial & serial)
    {
      for(size_t i = N ; i > 0; --i){
        if(test(i-1) == true){
          serial.print("1");
        }else{
          serial.print("0");
        }
      }    
    }    

    size_t count() const{
      size_t retval = 0;
      for(size_t i =0; i < N; ++i){
        if(test(i)){
          ++retval;
        }
      }
      return retval;
    }
    size_t size()  const{
      return N;
    }

    bitset<N>& operator=(const bitset<N> & rhs){
      if(&rhs == this){
        return *this;
      }
      //for(size_t i = 0; i <= byte_num(N); ++i)
      for(size_t i =0; i< num_bytes; ++i){      
        data[i] = rhs.data[i];
      }
      return *this;
    }


    bool operator==(const bitset<N>& rhs) const{
      for(size_t i =0; i< N; ++i){
        if(test(i) != rhs.test(i)){
          return false;
        }
      }
      return true;
    }

    bool operator!=(const bitset<N>& rhs) const{
      for(size_t i =0; i< N; ++i){
        if(test(i) != rhs.test(i)){
          return true;
        }
      }
      return false;
    }

    bool test(size_t pos) const{
      return (data[byte_num(pos)] & (1<<bit_num(pos)) ) != 0;
    }

    bool any() const{
      for(size_t i = 0; i< N; ++i){
        if(test(i)==true){
          return true;
        }
      }
      return false;
    }

    bool none() const{
      if(any() == true){
        return false;
      }
      return true;
    }

    bitset<N> operator<<(size_t pos) const{
      bitset retval(*this);
      retval<<=pos;
      return retval;
    }
    bitset<N> operator>>(size_t pos) const{
      bitset retval(*this);
      retval>>=pos;
      return retval;
    }
  };

  //Non-member functions


  template <size_t N>  bitset<N> operator&(const bitset<N>& lhs, const bitset<N>& rhs){
    bitset<N> retval(lhs);
    retval &= rhs;
    return retval;
  }

  template <size_t N>  bitset<N> operator|(const bitset<N>& lhs, const bitset<N>& rhs){
    bitset<N> retval(lhs);
    retval |= rhs;
    return retval;
  }

  template <size_t N>  bitset<N> operator^(const bitset<N>& lhs, const bitset<N>& rhs){
    bitset<N> retval(lhs);
    retval ^= rhs;
    return retval;
  }


}//std

class IShiftCommon
{
  public:

  virtual void setup() = 0;
  virtual void loop() = 0;
};

/// Lecture brute des  entrées sur X bits
template <size_t BITS, size_t INS, size_t OUTS> class ShiftInput : public IShiftCommon
{
  const int PULSE_WIDTH_USEC = 5;

  /// Callback type
  typedef bool (& triggerEventCallback)(int index, bool statusInput);

  /// Parallel load
  byte m_ploadPin;
  /// Clock Enable
  byte m_clockEnablePin;
  /// Data
  byte m_dataPin[INS];
  /// Clock
  byte m_clockPin;
  /// Callback
  triggerEventCallback m_callbackTrigger;
  
  /// Total bits
 
  public:
  
  /** Création - initialisation
   *  @param ploadPin Numéro de pin PARALLEL LOAD 
   *  @param clockEnablePin Numéro de pin CLOCK ENABLE
   *  @param dataPin Numéro de pin DATA
   *  @param clockPin Numéro de pin CLOCK 
   */
  ShiftInput(triggerEventCallback & callbackTrigger, int ploadPin,int clockEnablePin,int clockPin, const int (&dataPins)[INS])
  : m_ploadPin(ploadPin),
    m_clockEnablePin(clockEnablePin),
    m_clockPin(clockPin),
    m_callbackTrigger(callbackTrigger)
  { 
    for (int j = 0; j < INS; j++)  //initialize from array initializer
        m_dataPin[j] = dataPins[j];
  }
  
  virtual void setup()
  {
    Serial.print("Setting up stuff : ");Serial.print(INS);Serial.print(" branch(s) ");Serial.print(BITS);Serial.print(" bits = total ");Serial.println(OUTS);
    pinMode(m_ploadPin, OUTPUT);
    pinMode(m_clockEnablePin, OUTPUT);
    pinMode(m_clockPin, OUTPUT);
    for (int j = 0; j < INS; j++)
      pinMode(m_dataPin[j], INPUT);
    
    digitalWrite(m_clockPin, LOW);
    digitalWrite(m_ploadPin, HIGH);
    delayMicroseconds(PULSE_WIDTH_USEC);

    Serial.println("First read... ");

    m_lastDebounceTime =0L;
    m_buttonState.reset();
    //m_buttonState = readInputs();
    m_lastButtonRead = m_buttonState;          
    Serial.println("First read done... ");
  }

  /// Read everything with anti-parasite protection
  std::bitset<OUTS> readInputs()
  {
    #if 1
    // Lire 3 par sécurité anti-parasite
    std::bitset<OUTS> i1, i2, i3;
    do {
      i1 = readInputsInner();  
      i2 = readInputsInner();  
      i3 = readInputsInner();
    } while(i1 != i2 || i2 != i3);
#else
    std::bitset<OUTS> i1 = readInputsInner();  

#endif
    //Serial.println("Unparaziting ok --------------------------------------");
    return i1;
  } 

  /// Read all chains of inputs and flatten all the resulting bits
  std::bitset<OUTS> readInputsInner()
  {
    std::bitset<OUTS> bytesVal;

    /* Trigger a parallel Load to latch the state of the data lines,
    */
    digitalWrite(m_clockEnablePin, HIGH);
    digitalWrite(m_ploadPin, LOW);
    delayMicroseconds(PULSE_WIDTH_USEC);
    digitalWrite(m_ploadPin, HIGH);
    digitalWrite(m_clockEnablePin, LOW);

    /* Loop to read each bit value from the serial out line
     * of the SN74HC165N.
     * 
     * [ BITS ] --- sequential data to read
     * 
     * [ BITS ] [ BITS ] ... [ BITS ]
     *       \     |         /
     *          'INS' BRANCHES
     *          
     *  OUTS resulting bits
     *  [ BITS ] + [ BITS ] ... + [ BITS]
    */
    for(int i = 0; i < BITS; i++)
    {
        int offset_bits = 0;
        for (int j = 0; j < INS; j++)
        {
          long bitVal = digitalRead(m_dataPin[j]);
          //Serial.print("Setting bit #");Serial.print((BITS - 1) - i + offset_bits);Serial.print(" to ");Serial.println(bitVal);
#if 1
          auto index = (BITS - 1) - i + offset_bits;
          if (index >= OUTS || index < 0) {
            Serial.print("Setting off bit #");Serial.print(index);Serial.print(" to ");Serial.println(bitVal);
          } else{
            bytesVal.set(index, bitVal != 0);
          }
#endif
          // nb: Offset addition to avoid a multiply
          offset_bits += BITS;
          // nb: for reference : original code
          //bytesVal |= (bitVal << ((m_dataWidth-1) - i));
        }
        
        //bytesVal |= (bitVal << i);

        /* Pulse the Clock (rising edge shifts the next bit).
        */
        digitalWrite(m_clockPin, HIGH);
        delayMicroseconds(PULSE_WIDTH_USEC);
        digitalWrite(m_clockPin, LOW);
    }
    // Delay Between polls
    delayMicroseconds(1000);

    return(bytesVal);
  }


    /// the debounce time; increase if the output flickers
    const long debounceDelay = 30;    
    
    /// Bitfield: état boutons
    std::bitset<OUTS>  m_buttonState;
    /// Bitfield: dernière lecture
    std::bitset<OUTS>  m_lastButtonRead;   
    /// event times
    long m_lastDebounceTime;


    /// Boucle pour tester les entrées
    virtual void loop()
    {
      std::bitset<OUTS> reading = readInputs();

      // If the switch changed, due to noise or pressing:
      if (reading != m_lastButtonRead) {
        // reset the debouncing timer
        m_lastDebounceTime = millis();

        //Serial.print("?[");reading.print(Serial);Serial.println("]");
      } 
      else if ((millis() - m_lastDebounceTime) > debounceDelay) { // debounce
        // whatever the reading is at, it's been there for longer
        // than the debounce delay, so take it as the actual current state:
          
        // if the button state has changed:
        if (reading != m_buttonState) {         
          std::bitset<OUTS> previous = m_buttonState;
          
          // Trigger event(s)
          if (triggerEvent(previous, reading))
          {
            // Memorize only if publish successful !
            m_buttonState = reading;
          }
        }
      }
      m_lastButtonRead = reading;

    }
    
    /// Trigger event for each variation
    bool triggerEvent(std::bitset<OUTS> & previousFlags, std::bitset<OUTS> & currentFlags)
    {
      bool ok = true;
      Serial.print("<[");previousFlags.print(Serial);Serial.println("]");
      Serial.print(">[");currentFlags.print(Serial);Serial.println("]");
      
      for (int n = 0; n < OUTS && ok; n++)
      {
        // Compare Nth bit
        if (previousFlags.test(n) != currentFlags.test(n))
        {
          // message on variation
          ok = m_callbackTrigger(n, currentFlags.test(n));
        } 
      }
      return ok;
    }
    
  
};
