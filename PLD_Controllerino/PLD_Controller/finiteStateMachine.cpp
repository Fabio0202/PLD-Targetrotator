#include "finiteStateMachine.h"
#include "stepperControl.h"
#include "manageLaser.h"
#include "config.h"

namespace finiteStateMachine 
{
  SystemState sysState;
  bool teachDone;
  static bool freiStart;
  static unsigned long highStart;

  void setup() 
  {
    sysState = SYS_IDLE; // Initialzustand
    teachDone = false; // Teach-Status initialisieren
    freiStart = false; // ob der Frei-Fahrmodus bereits gestartet wurde
    highStart = 0; // zeit in der die Lichtschranke HIGH ist
  }
  
  void update() 
  {
    switch (sysState)
    {
      case SYS_IDLE:
        break;
      
      case SYS_TEACH_START:
        if (digitalRead(LICHTSCHRANKE) == HIGH)
          {
            if (!freiStart)
              {
                Serial.println("🚀 Frei fahren...");
                stepperControl::moveToRelative(200); // Move slightly to the right to clear the sensor
                freiStart = true;
                sysState = SYS_TEACH_FREI;
              }
          }
        else
          {
            Serial.println("🚀 Teach normal...");
            stepperControl::moveToRelative(5000); // Move far to the right
            sysState = SYS_TEACH_RECHTS;
          }
        break;
        
      case SYS_TEACH_FREI:
        if (stepperControl::isMoveComplete()) 
          {
            Serial.println("✅ Frei gefahren");
            sysState = SYS_TEACH_START; // beginne Teach nochmal
            freiStart = false; // Reset für nächsten Gebrauch
          }
        break;
      
      case SYS_TEACH_RECHTS:
        static unsigned long highLicht = 0;
                  
        if (digitalRead(LICHTSCHRANKE) == HIGH)
        {
          if (highLicht == 0)
           {
              highLicht = millis(); // remember start time
           }
          else if (millis() - highLicht > 50)
            { //50ms stable HIGH
            stepperControl::stop();
            stepperControl::setCurrentPosition(0);
            Serial.println("✅ Nullpunkt gesetzt");
            sysState = SYS_TEACH_DONE;
            }
        }
        else
        {
          highLicht = 0; // reset if LOW
        }
        break;
        
      case SYS_TEACH_DONE:
      if(!teachDone) // nur wenn TEACH noch nicht als done markiert ist
        {
          teachDone = true;
          Serial.println("TEACH ist fertig!"); 
        }
        sysState = SYS_IDLE;
      break;
        
      case SYS_MOVE_TO_POS:
        stepperControl::checkMoveComplete();
        if (stepperControl::isMoveComplete()) 
          {
            sysState = SYS_TEACH_DONE;
          }
          break;

      case SYS_LASER_ACTIVE:
        // Warten auf Laser-Completion
        if (manageLaser::isSequenceCompleted()) 
        {
          sysState = SYS_IDLE;
        }
        break;
      
      case SYS_MANUAL_MODE:
        setTeachDone(false); // Teach muss neu gemacht werden
        setState(SYS_IDLE); // zurück zu idle wechseln
        break;
    }
  }

  void printCurrentState() 
  {
    Serial.print("SYSTEM_STATE: ");
    switch (sysState) {
        case SYS_IDLE: Serial.println("SYS_IDLE"); break;
        case SYS_TEACH_START: Serial.println("SYS_TEACH"); break;
        case SYS_TEACH_DONE: Serial.println("SYS_TEACH_DONE"); break;
        case SYS_MOVE_TO_POS: Serial.println("SYS_MOVE_TO_POS"); break;
        case SYS_LASER_ACTIVE: Serial.println("SYS_LASER_ACTIVE"); break;
        default: Serial.println("UNKNOWN"); break;
    }
  }
  
  SystemState getState() { return sysState; }
  
  void setState(SystemState newState) { sysState = newState; }
  
  bool isTeachDone() { return teachDone; }

  void setTeachDone(bool done) { teachDone = done; }
}
