#ifndef CONFIG_H
#define CONFIG_H

// ---------------- Pin Definitions ----------------
#define PUL_PIN 6
#define DIR_PIN 7
#define LICHTSCHRANKE 3
#define DE_RE_PIN 2
#define DRIVER_ENABLE_PIN 8

// Laser System Pins 
#define LASER_PIN 13      // Laser-Puls Pin
#define LASER_RELAY 12    // Relais für Laser-Stromversorgung
#define LASER_SPEAKER 11  // Speaker für Alarm

// ---------------- Constants / Default Parameters ----------------
const int MAX_STEP = 1600;
const float DEFAULT_MAX_SPEED = 500.0;
const float DEFAULT_ACCELERATION = 500.0;
extern float userMaxSpeed;
extern float userAcceleration;

#endif