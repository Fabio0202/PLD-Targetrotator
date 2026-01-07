#ifndef FINITE_STATE_MACHINE_H
#define FINITE_STATE_MACHINE_H

#include "config.h"

enum SystemState 
{
  SYS_IDLE,
  SYS_TEACH_START,
  SYS_TEACH_RECHTS,
  SYS_TEACH_FREI,
  SYS_TEACH_DONE,
  SYS_MOVE_TO_POS,
  SYS_LASER_ACTIVE,
  SYS_MANUAL_MODE
};

namespace finiteStateMachine 
{
  void setup();
  void update();
  SystemState getState();
  void setState(SystemState newState);
  bool isTeachDone();
  void setTeachDone(bool done);
  void printCurrentState();
}

#endif
