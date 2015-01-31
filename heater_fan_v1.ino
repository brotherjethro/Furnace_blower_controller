// Controller for home furnace blower
//
// Inputs from thermostat:
//       Heat Call
//       Cool
//       Fan
//
// Output to furnace:
//       Fan
//
//

#define HEAT_IN_PIN    4
#define COOL_IN_PIN    8
#define FAN_IN_PIN    12
#define FAN_OUT_PIN    7
#define BLINK_PIN     13


#define DEBUG  1

#if DEBUG
  #define DBG(x)  x
#else
  #define DBG(x)
#endif


#define US_SINCE(us)           (micros() - (us))
#define MS_SINCE(ms)           (millis() - (ms))

#define ALMOST_OVERFLOW_U32    (0xffffffffUL - 10000UL)  // 10 seconds away from overflow (Almost 50 days)

//#define ONE_MINUTE_MS          (1000UL)  // for super fast time (one second = one minute)
#define ONE_MINUTE_MS          (60UL * 1000UL)
#define ONE_HOUR_MS            (60UL * ONE_MINUTE_MS)
#define ONE_DAY_MS             (24UL * ONE_HOUR_MS)
#define ONE_WEEK_MS            (7UL  * ONE_DAY_MS)


typedef unsigned long u32bit;



class blinky_led
{
  public:
    void begin(int pin)
    {
      m_pin = pin;
      pinMode(m_pin, OUTPUT);
      digitalWrite(m_pin, 0);
      m_last_changed_ms = millis();
    }
    
    void tick(void)
    {
      if (MS_SINCE(m_last_changed_ms) >= 200)
      {
        digitalWrite(m_pin, !digitalRead(m_pin));
        m_last_changed_ms = millis();
      }
    }
    
  private:
    int    m_pin;
    u32bit m_last_changed_ms;
};




class bool_time_tracker
{
  public:
    bool_time_tracker(boolean state = false)
    {
      m_state = state;
      m_last_state_change_ms_valid = false;
      m_last_state_change_ms = 0;
    }
    
    void state_now(boolean state)
    {
      if (state != m_state)
      {
        m_last_state_change_ms = millis();
        m_last_state_change_ms_valid = true;
      }
      else if (m_last_state_change_ms_valid)
      {
        // If the time since the last state change is going to
        // overflow soon, then say it's no longer valid.
        if (MS_SINCE(m_last_state_change_ms) >= ALMOST_OVERFLOW_U32)
        {
          m_last_state_change_ms = 0;
          m_last_state_change_ms_valid = false;
        }
      }
    }
    
    boolean was_on_in_last_ms(u32bit ms)
    {
      state_now(m_state);
      
      if (m_state)
        return true;
      else if (m_last_state_change_ms_valid && (MS_SINCE(m_last_state_change_ms) <= ms))
        return true;
      else
        return false;
    }
    
  private:
    boolean m_state;
    boolean m_last_state_change_ms_valid;
    u32bit  m_last_state_change_ms;
};





class input_24vac
{
  public:
    
    void begin(int pin)
    {
      m_pin = pin;
      pinMode(m_pin, INPUT);
      m_avg_pin_val = 0.0;
      m_time_last_read_us = 0;
    }

    void tick(void)
    {
      if (US_SINCE(m_time_last_read_us) >= 1000)
      {
        float now_val = digitalRead(m_pin) ? 1.0 : 0.0;
        m_avg_pin_val = (0.99 * m_avg_pin_val) + (0.01 * now_val);
        m_time_tracker.state_now(is_on());
        m_time_last_read_us = micros();
      }
    }

    boolean is_on(void)
    {
      return m_avg_pin_val >= 0.5;
    }

    void dbg(void)
    {
      DBG((Serial.print("/ in")));
      DBG((Serial.print(m_pin)));
      DBG((Serial.print(is_on() ? " ON  " : " off ")));
    }

    class bool_time_tracker m_time_tracker;
    
  private:
    u32bit          m_time_last_read_us;
    int             m_pin;
    float           m_avg_pin_val;        
};



class input_24vac & get_heat_input(void);
class input_24vac & get_cool_input(void);
class input_24vac & get_fan_input(void);



class fan_manager
{
  #define NUM_CLIENTS 4
  
  public:
    void begin(int pin)
    {
      m_pin = pin;
      pinMode(pin, OUTPUT);
      digitalWrite(pin, 0);
      m_ever_ran = false;
    }

    boolean is_running(void)
    {
      return digitalRead(m_pin);
    }

    void tick(void)
    {
      set_client_needs(0, get_heat_input().is_on(), 10);   // Heat: 10 minutes extra
      set_client_needs(1, get_cool_input().is_on(), 20);   // Cool: 20 minutes extra
      set_client_needs(2, get_fan_input().is_on(),   1);   // Fan:   1 minute  extra

      // Should we run the fan without any external request?
      if (   get_cool_input().m_time_tracker.was_on_in_last_ms(ONE_WEEK_MS)  // cooled within last week
          && !m_time_tracker.was_on_in_last_ms(20 * ONE_MINUTE_MS)           // fan hasn't run in 20 min
          && m_ever_ran
         )
      {
        // When weather requires cooling, run (20 off / 20 on)
        set_client_needs(3, true,  20);
        set_client_needs(3, false, 20);
      }
      else if (   !m_time_tracker.was_on_in_last_ms(30 * ONE_MINUTE_MS)      // fan hasn't run in 30 min
               && m_ever_ran
              )
      {
        // When weather doesn't require cooling, run (30 off / 10 on)
        set_client_needs(3, true,  10);
        set_client_needs(3, false, 10);
      }
      
      m_ever_ran |= is_running();
      
      m_time_tracker.state_now(is_running());
    }

    void dbg(void)
    {
      DBG((Serial.print("/ fan_mgr ")));
      DBG((Serial.print(is_running() ? "ON  " : "off ")));
      if (m_extra_ms)
      {
        u32bit ms_remaining = m_extra_ms - MS_SINCE(m_extra_start_ms);
        u32bit mins_remaining = ms_remaining / 60000;
        u32bit secs_remaining = (ms_remaining % 60000) / 1000;
        DBG((Serial.print(mins_remaining)));
        DBG((Serial.print(":")));
        DBG((Serial.print(secs_remaining)));
        DBG((Serial.print(" extra ")));
      }
    }
    
    class bool_time_tracker m_time_tracker;
    
  private:
    int                     m_pin;
    u32bit                  m_extra_ms;
    u32bit                  m_extra_start_ms;
    boolean                 m_client_needs[NUM_CLIENTS];
    boolean                 m_ever_ran;

    void set_client_needs(int client, boolean state, int extra_minutes)
    {
      boolean need_on = false;
      u32bit  extra_ms = extra_minutes * ONE_MINUTE_MS;

      //-------------------------------------------------
      // Check for end of extra time, if any:
      //-------------------------------------------------
      if (m_extra_ms)
      {
        if (MS_SINCE(m_extra_start_ms) > m_extra_ms)
        {
          // Extra time has ended:
          m_extra_ms = 0;
          m_extra_start_ms = 0;
        }
      }

      //-------------------------------------------------
      // If this client is changing from TRUE to FALSE,
      // then check to see if their extra time would end
      // after the current extra time, and update it
      // if needed.
      //-------------------------------------------------
      if (!state && m_client_needs[client] && extra_ms)
      {
        u32bit extra_elapsed_ms = MS_SINCE(m_extra_start_ms);
        
        if (   !m_extra_ms
            || (extra_ms > (m_extra_ms - extra_elapsed_ms))
           )
        {
          // This new extra time would end later:
          m_extra_start_ms = millis();
          m_extra_ms = extra_ms;
        }
      }
      
      m_client_needs[client] = state;

      for (int i=0; i<NUM_CLIENTS; i++)
        need_on |= m_client_needs[i];

      if (need_on && !is_running())
      {
        //-------------------------------------------------
        // Some client wants the fan to start:
        //-------------------------------------------------
        digitalWrite(m_pin, 1);
      }
      else if (!need_on && is_running())
      {
        //-------------------------------------------------
        // No inputs need fan on, but wait for extra time
        // before turning it off:
        //-------------------------------------------------
        if (!m_extra_ms)
        {
          digitalWrite(m_pin, 0);
        }
      }
    }
};





class blinky_led     status_blink;

class input_24vac    heat_in;
class input_24vac    cool_in;
class input_24vac    fan_in;

class fan_manager    fan_out;




void setup(void)
{
  DBG((Serial.begin(9600)));
  DBG((Serial.println("---BOOTED---")));

  status_blink.begin(BLINK_PIN);
  
  heat_in.begin(HEAT_IN_PIN);
  cool_in.begin(COOL_IN_PIN);
  fan_in.begin(FAN_IN_PIN);

  fan_out.begin(FAN_OUT_PIN);
}



void loop(void)
{
  static u32bit ms_last_dbg = 0;
  
  heat_in.tick();
  cool_in.tick();
  fan_in.tick();
 
  fan_out.tick();

  status_blink.tick();

  if (MS_SINCE(ms_last_dbg) >= 1000)
  {
    ms_last_dbg = millis();

    u32bit mins = ms_last_dbg / 60000;
    u32bit secs = (ms_last_dbg % 60000) / 1000;
    DBG((Serial.print(mins)));
    DBG((Serial.print(":")));
    DBG((Serial.print(secs)));
    DBG((Serial.print(" ")));

    heat_in.dbg();
    cool_in.dbg();
    fan_in.dbg();
    fan_out.dbg();
    DBG((Serial.println("")));
  }
}




class input_24vac & get_heat_input(void)
{
  return heat_in;
}

class input_24vac & get_cool_input(void)
{
  return cool_in;
}

class input_24vac & get_fan_input(void)
{
  return fan_in;
}

class fan_manager & get_fan_out(void)
{
  return fan_out;
}

