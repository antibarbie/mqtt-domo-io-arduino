/// Gestion de 32 sorties brutes
class ShiftOutput
{
  int m_dataPin;
  int m_clockPin;
  int m_latchPin;

  long m_data;
  
  public:
    /// Création avec les trois pins d'entrée/sortie
    ShiftOutput(int dataPin, int clockPin, int latchPin)
    {
      m_data = 0;
      m_dataPin = dataPin;
      m_clockPin = clockPin;
      m_latchPin = latchPin;      
    }

    /// Statut actuel d'une I/O de la carte
    bool getOutputStatus(int bit)
    {
      return ((m_data >> bit) & 1L) == 1L;
    }

    /// Modifier les I/O de la carte sans les appliquer
    void setBit(int bit, bool value)
    {
        m_data &= ~(1L << bit);
        m_data |= value ? (1L << bit): 0L;
    }
    
    /// Set all I/O at the same time
    void set(long data)
    {
        m_data = data;
    }
    /// Get all I/O at the same time
    long get()
    {
        return m_data;
    }
    /// Initialisation
    void setup()
    {
      pinMode(m_latchPin, OUTPUT);
      pinMode(m_dataPin, OUTPUT);  
      pinMode(m_clockPin, OUTPUT);
      apply();
    }

    void shift(uint8_t bitOrder, byte val)
    {
    #if 1
         shiftOut(m_dataPin, m_clockPin, bitOrder, val);
    #else
         int i;
         const int delaymus = 50;
    
         for (i = 0; i < 8; i++)  {
               digitalWrite(m_clockPin, LOW);            
               delayMicroseconds(delaymus); 
               if (bitOrder == LSBFIRST)
                     digitalWrite(m_dataPin, !!(val & (1 << i)));
               else      
                     digitalWrite(m_dataPin, !!(val & (1 << (7 - i))));
               delayMicroseconds(delaymus); 
               digitalWrite(m_clockPin, HIGH);
               delayMicroseconds(delaymus); 
         }
    #endif
      }

    /// Appliquer les I/O de la carte sur le hardware
    void apply()
    {
      // turn off the output so the pins don't light up
      // while you're shifting bits:
      digitalWrite(m_latchPin, LOW);
      // shift the bits out:     
      shift( LSBFIRST, (m_data >> 24) & 0xFF);
      shift( LSBFIRST, (m_data >> 16) & 0xFF);
      shift( LSBFIRST, (m_data >> 8) & 0xFF);
      shift( LSBFIRST, (m_data) & 0xFF);

      // turn on the output so the LEDs can light up:
      digitalWrite(m_latchPin, HIGH);
    }
};

