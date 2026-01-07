#ifndef STEPPER_CONTROL_H
#define STEPPER_CONTROL_H

#include <Arduino.h>

enum MoveSource 
{
  MOVE_NONE,
  MOVE_LOAD, 
  MOVE_GOTO 
};

namespace stepperControl 
{
  extern float userMaxSpeed; //externe Variablen deklarieren (definiert in stepperControl.cpp)
  extern float userAcceleration;

  void setup();
  void update();
  void setMaxSpeed(float speed);
  void setAcceleration(float accel);
  void moveToAbsolute(long position);
  void moveToRelative(long distance);
  void stop();
  void setCurrentPosition(long position);
  bool isMoveComplete();
  int getNormalizedPosition();
  void savePosition(const String& cmd);
  void loadPosition(const String& cmd);
  void gotoPosition(const String& cmd);
  void printStatus();
  void resetMoveSource();
  void enableDriver(bool enable);
  bool isDriverEnabled();
  long getForwardSteps(int target);
  unsigned long calculateMoveTime(int targetPosition);
  void checkMoveComplete();

}

#endif