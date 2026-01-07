//------------------------------------------------------------------------------
// Mega - Positionierung in jede Richtung möglich
// Version: 19.09.2025
//------------------------------------------------------------------------------

#include "config.h"
#include "finiteStateMachine.h"
#include "manageLaser.h"
#include "readSerialCommand.h"
#include "stepperControl.h"

//------------------------------------------------------------------------------
// Setup
void setup() 
{
  Serial.begin(9600);
  
  // Initialize all modules
  stepperControl::setup();
  readSerialCommand::setup();
  manageLaser::setup();
  finiteStateMachine::setup();
  
  Serial.println(F(">>> Steuerung bereit"));
  printHelp();
}

//------------------------------------------------------------------------------
// Main Loop
void loop() 
{
  // Read and process serial commands
  readSerialCommand::update();
    // Execute finite state machine
  finiteStateMachine::update();
  
    // Run stepper motor
  stepperControl::update();
  
  // Manage laser LED timing
  manageLaser::update();
}

//------------------------------------------------------------------------------
// Helper Functions
void printHelp() 
{
  Serial.println(F("Gültige Befehle:"));
  Serial.println(F("  CMD:TEACH              -> Nullpunkt fahren"));
  Serial.println(F("  CMD:RESET              -> Teach zurücksetzen"));
  Serial.println(F("  CMD:POS                -> aktuelle Position anzeigen"));
  Serial.println(F("  CMD:GOTO:<pos>         -> Position anfahren (0-1600)"));
  Serial.println(F("  CMD:SAVE:<1-6>         -> aktuelle Pos speichern"));
  Serial.println(F("  CMD:LOAD:<1-6>         -> gespeicherte Pos anfahren"));
  Serial.println(F("  CMD:SETMAXSPEED:<v>    -> MaxSpeed ändern"));
  Serial.println(F("  CMD:SETACCEL:<v>       -> Acceleration ändern"));
  Serial.println(F("  CMD:STATUS             -> Status anzeigen"));
  Serial.println(F(""));
  Serial.println(F("Laser-Steuerung:"));
  Serial.println(F("  CMD:LASER_p<num>f<freq>-> Laser-Puls Sequenz (z.B. CMD:LASER_p50f5)"));
  Serial.println(F("  CMD:LASER_stop         -> Laser komplett stoppen"));
  Serial.println(F("  CMD:LASER_killp        -> Laser-Strom abschalten"));
  Serial.println(F("  CMD:LASER_restorep     -> Laser-Strom einschalten"));
  Serial.println(F("  CMD:LASER_status       -> Laser-Status anzeigen"));
  Serial.println(F("  CMD:LASER_test         -> Erweiterter Laser-Test"));
  Serial.println(F(""));
  Serial.println(F("Beispiele:"));
  Serial.println(F("  CMD:LASER_p10f2        -> 10 Pulse mit 2Hz (erst Alarm, dann Pulse)"));
  Serial.println(F("  CMD:LASER_p100f20      -> 100 Pulse mit 20Hz"));
  Serial.println(F("--------------------------------------------"));
}