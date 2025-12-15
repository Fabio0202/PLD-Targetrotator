#include "readSerialCommand.h"
#include "finiteStateMachine.h"
#include "stepperControl.h"
#include "manageLaser.h"
#include "config.h"

namespace readSerialCommand 
{
  static bool lastStateBusy = false;

  void setup() 
  {
    pinMode(DE_RE_PIN, OUTPUT); // Set DE/RE pin as output for RS485 (not currently used)
    digitalWrite(DE_RE_PIN, LOW);
  }
  
  void update() 
  {
    SystemState currentState = finiteStateMachine::getState();
    static String inputLine = "";

    while (Serial.available()) 
    {
      char c = Serial.read();
      if (c == '\n' || c == '\r') 
      {
        inputLine.trim();
        if (inputLine.length() > 0) 
        {
          // 🟢 AUSNAHMEN: Diese Befehle werden IMMER verarbeitet
          bool isAllowedCommand = (inputLine == "CMD:POS" || 
                                  inputLine == "CMD:STATUS" || 
                                  inputLine == "CMD:LASER_stop" ||
                                  inputLine.startsWith("CMD:ESTIMATE_MOVE:") ||
                                  inputLine.startsWith("CMD:ESTIMATE_LASER:"));
          
          if (isAllowedCommand || currentState == SYS_IDLE) 
          {
            processCommand(inputLine);
          }
          // 🔴 System busy - Befehl ignorieren
          else 
          {
            if (!lastStateBusy)
            {
              Serial.println("⚠️ System busy. Befehl wird ignoriert.");
              lastStateBusy = true;
            }
          }
        }
        inputLine = "";
      } 
      else 
      {
        inputLine += c;
      }
    }
    
    // Reset lastStateBusy wenn System wieder idle ist
    if (currentState == SYS_IDLE) 
    {
      lastStateBusy = false;
    }
  }
  
  void processCommand(const String& cmd) 
  {
    if (cmd == "CMD:TEACH") 
    {
      finiteStateMachine::setState(SYS_TEACH_START);
    }
    else if (cmd.startsWith("CMD:SAVE")) 
    {
      stepperControl::savePosition(cmd);
    }
    else if (cmd.startsWith("CMD:LOAD")) 
    {
      stepperControl::loadPosition(cmd);
    }
    else if (cmd.startsWith("CMD:GOTO"))
    {
      stepperControl::gotoPosition(cmd);
    }
    else if (cmd == "CMD:POS") 
    {
      Serial.print("📍 Aktuelle Position: ");
      Serial.println(stepperControl::getNormalizedPosition());
    }
    else if (cmd == "CMD:STATUS") 
    {
      stepperControl::printStatus();
      manageLaser::printLaserStatus();
      finiteStateMachine::printCurrentState();
    }
    else if (cmd.startsWith("CMD:SETMAXSPEED")) 
    {
      float newSpeed = cmd.substring(16).toFloat();
      if (newSpeed > 0) 
      {
        stepperControl::setMaxSpeed(newSpeed);
        Serial.print("✅ Neue MaxSpeed gesetzt: ");
        Serial.println(newSpeed);
      } 
      else 
      {
        Serial.println("❌ Ungültiger Wert für MaxSpeed");
      }
    }
    else if (cmd.startsWith("CMD:SETACCEL")) 
    {
      float newAccel = cmd.substring(13).toFloat();
      if (newAccel > 0) 
      {
        stepperControl::setAcceleration(newAccel);
        Serial.print("✅ Neue Acceleration gesetzt: ");
        Serial.println(newAccel);
      } 
      else 
      {
        Serial.println("❌ Ungültiger Wert für Acceleration");
      }
    }
    else if (cmd == "CMD:RESET") 
    {
      finiteStateMachine::setTeachDone(false);
      finiteStateMachine::setState(SYS_IDLE);
      Serial.println("♻️ Teach zurückgesetzt.");
    }
    else if (cmd.startsWith("CMD:LASER_")) 
    {
      manageLaser::processLaserCommand(cmd);
    }
    else if (cmd == "CMD:MANUALLY")
    { 
      stepperControl::stop();
      manageLaser::stopLaser();
      stepperControl::enableDriver(false);
      Serial.println("⚠️ Manueller Modus aktiviert. Treiber deaktiviert & Laser gestopt.");
      finiteStateMachine::setState(SYS_MANUAL_MODE);
    }
    else if (cmd == "CMD:AUTO")
    {
      stepperControl::setCurrentPosition(0);
      stepperControl::enableDriver(true);
      Serial.println("✅ Automatischer Modus aktiviert. Treiber aktiviert.");
    }
    else 
    {
      Serial.println("❌ Unbekannter Befehl: " + cmd);
    }
  }
}