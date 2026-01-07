#ifndef MANAGE_LASER_H
#define MANAGE_LASER_H

#include <Arduino.h>

namespace manageLaser 
{
  // Laser Pulsar Variablen
  extern bool laserOn;
  extern unsigned long firedPulses;
  extern unsigned long totalPulses;
  extern unsigned long lastFired;
  extern unsigned long pulsePeriod;
  
  void setup();
  void update();
  
  // Hauptfunktionen
  void startLaserSequence(unsigned long pulses, double frequency);
  void pauseLaser();
  void stopLaser();
  void continueLaser();
  void killPower();
  void restorePower();
  
  // Hilfsfunktionen
  void firePulse();
  void alarm();
  void blockingTone(unsigned int frequency, unsigned long duration);
  
  // Befehlverarbeitung
  void processLaserCommand(const String& command);
  
  // Status
  bool isLaserActive();
  bool isSequenceCompleted();
  void printLaserStatus();

  // Teste Laser aktivitaet
  void activate();
  void test();
  void firePulsesMissing();
  unsigned long calculateLaserTime(unsigned long pulses, double frequency);

}

#endif