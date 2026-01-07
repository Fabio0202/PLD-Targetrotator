#include "stepperControl.h"
#include <AccelStepper.h>
#include "finiteStateMachine.h"
#include "config.h"
#include "manageLaser.h"

namespace stepperControl 
{
  AccelStepper stepper(AccelStepper::DRIVER, PUL_PIN, DIR_PIN);
  MoveSource lastMove = MOVE_NONE;

  // Variablen definieren - diese koennen jetzt von aussen geaendert werden
  float userMaxSpeed = DEFAULT_MAX_SPEED;
  float userAcceleration = DEFAULT_ACCELERATION;
  static bool driverEnabled = true;
  int savedPositions[6] = {0, 267, 533, 800, 1067, 1333};
  int targetPos = 0;
  
  void setup() 
  {
    pinMode(DRIVER_ENABLE_PIN, OUTPUT);
    enableDriver(true);
    stepper.setCurrentPosition(0);
    stepper.setMaxSpeed(userMaxSpeed);
    stepper.setAcceleration(userAcceleration);
    pinMode(LICHTSCHRANKE, INPUT_PULLUP);
  }
  
  void update() 
  {
    stepper.run();
  }
  
  void enableDriver(bool enable) 
  {
    driverEnabled = enable;
    digitalWrite(DRIVER_ENABLE_PIN, !enable); 
    if (enable) 
    {
      Serial.println("Stepper-Treiber aktiviert");
    } 
    else 
    {
      Serial.println("Stepper-Treiber deaktiviert (Manueller Modus)");
    }
  }

  bool isDriverEnabled() 
  {
    return driverEnabled;
  }

  void setMaxSpeed(float speed) 
  {
    userMaxSpeed = speed;
    stepper.setMaxSpeed(userMaxSpeed);
    Serial.print("MaxSpeed gesetzt auf: ");
    Serial.println(userMaxSpeed);
  }
  
  void setAcceleration(float accel) 
  {
    userAcceleration = accel;
    stepper.setAcceleration(userAcceleration);
    Serial.print("Acceleration gesetzt auf: ");
    Serial.println(userAcceleration);
  }

  float getMaxSpeed() 
  {
    return userMaxSpeed;
  }

  float getAcceleration() 
  {
    return userAcceleration;
  }
  
  void stop() 
  {
    stepper.stop();
  }
  
  void setCurrentPosition(long position) 
  {
    stepper.setCurrentPosition(position);
  }
  
  bool isMoveComplete() 
  {
    long dist = stepper.distanceToGo();
    float spd = stepper.speed();
        // Move gilt nur als komplett, wenn keine Distanz mehr übrig ist UND Motor steht (kleine Toleranz)
    return (dist == 0 && fabs(spd) < 0.5);
  }

  void checkMoveComplete() 
  {
    if (isMoveComplete() && (lastMove != MOVE_NONE)) 
    {
      if (lastMove == MOVE_GOTO) 
      {
        Serial.print("OK:GOTO");
      } 
      else if (lastMove == MOVE_LOAD) 
      {
        Serial.print("OK:LOAD");
      }
      else 
      {
        Serial.print("OK:MOVE");
      }
      lastMove = MOVE_NONE;
      finiteStateMachine::setState(SYS_TEACH_DONE);
    }
  }
  
  int getNormalizedPosition() 
  {
    long pos = stepper.currentPosition();
    int norm = pos % MAX_STEP;
    if (norm < 0) norm += MAX_STEP;
    return norm;
  }
  
  void savePosition(const String& cmd) 
  {
    if (!finiteStateMachine::isTeachDone()) 
    {
      Serial.println("❌ Teach nicht abgeschlossen.");
      return;
    }
    
    int posIndex = cmd.substring(9).toInt();
    if (posIndex < 1 || posIndex > 6) 
    {
      Serial.println("❌ Speicherplatz ungültig (1-6)");
      return;
    }
    
    int normedPos = getNormalizedPosition();
    savedPositions[posIndex-1] = normedPos;
    Serial.print("💾 Position ");
    Serial.print(savedPositions[posIndex-1]);
    Serial.print(" gespeichert in Slot ");
    Serial.println(posIndex);
  }
  
  void loadPosition(const String& cmd) 
  {
    if (!finiteStateMachine::isTeachDone())
    {
      Serial.println("❌ Teach nicht abgeschlossen.");
      return;
    }
    
    int posIndex = cmd.substring(9).toInt();
    if (posIndex < 1 || posIndex > 6) 
    {
      Serial.println("❌ Speicherplatz ungültig (1-6)");
      return;
    }
    
    targetPos = savedPositions[posIndex-1];
    long forwardPos = getForwardSteps(targetPos);
    Serial.print("➡️ Fahre gespeicherte Pos ");
    Serial.println(targetPos);
    moveToRelative(forwardPos);
    lastMove = MOVE_LOAD;
    finiteStateMachine::setState(SYS_MOVE_TO_POS);
  }
  
  void gotoPosition(const String& cmd) 
  {
    if (!finiteStateMachine::isTeachDone()) 
    {
      Serial.println("❌ Teach nicht abgeschlossen.");
      return;
    }
    
    int pos = cmd.substring(9).toInt(); // extrahiert Zahl nach "CMD:GOTO:"

    if (pos < 0 || pos >= MAX_STEP) {
        Serial.print("❌ Ungültige Position. Gültiger Bereich: 0 bis ");
        Serial.println(MAX_STEP-1);
        return;
    }

    targetPos = pos;
    long forwardPos = getForwardSteps(targetPos);
    Serial.print("➡️ Fahre Zielpos ");
    Serial.println(targetPos);
    moveToRelative(forwardPos);
    lastMove = MOVE_GOTO;
    finiteStateMachine::setState(SYS_MOVE_TO_POS);
  }

  long getForwardSteps(int target) 
  {
    long current = stepper.currentPosition() % MAX_STEP;
    if (current < 0) current += MAX_STEP;
    target = target % MAX_STEP;

    long steps;
    if (target >= current) {
        steps = target - current;
    } 
    else 
    {
        steps = MAX_STEP - current + target;
    }

    return stepper.currentPosition() + steps;
  }

  void moveToRelative(long distance) 
  {
    if (!driverEnabled) 
    {
      Serial.println("❌ Treiber ist deaktiviert. Manueller Modus aktiv.");
      return;
    }
    if (manageLaser::isLaserActive()) 
    {
      Serial.println("❌ Laser ist aktiv. Bewegung nicht möglich.");
      return;
    }
    stepper.moveTo(distance);
    Serial.print("Bewegung gestartet");
    if (distance == 0) {
      return;
    }
  }

  void printStatus() 
  {
    Serial.print("TeachDone: "); 
    Serial.println(finiteStateMachine::isTeachDone());
    Serial.print("Aktuelle Position: "); 
    Serial.println(getNormalizedPosition());
    Serial.println("Gespeicherte Positionen:");
    for (int i = 0; i < 6; i++) 
    {
      Serial.print(i+1); 
      Serial.print(": ");
      Serial.println(savedPositions[i]);
    }
    Serial.print("MaxSpeed: "); 
    Serial.println(userMaxSpeed);
    Serial.print("Acceleration: "); 
    Serial.println(userAcceleration);
    
    if (lastMove == MOVE_LOAD) 
    {
      Serial.println("Letzter Move: LOAD");
    } 
    else if (lastMove == MOVE_GOTO) 
    {
      Serial.println("Letzter Move: GOTO");
    }
    else 
    {
      Serial.println("Letzter Move: NONE");
    }
  }
}