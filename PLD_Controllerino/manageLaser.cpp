#include "finiteStateMachine.h"
#include "manageLaser.h"
#include "config.h"

namespace manageLaser 
{
  // Laser Pulsar Variablen
  bool laserOn = false;
  unsigned long firedPulses = 0;
  unsigned long totalPulses = 0;
  unsigned long lastFired = 0;
  unsigned long pulsePeriod = 10000; // Standard: 0.1 Hz
  bool sequenceCompleted = false;
  
  void setup() 
  {
    // Pin Mode setzen
    pinMode(LASER_PIN, OUTPUT);
    digitalWrite(LASER_PIN, LOW);
    pinMode(LASER_RELAY, OUTPUT);
    digitalWrite(LASER_RELAY, LOW);
    pinMode(LASER_SPEAKER, OUTPUT);
    digitalWrite(LASER_SPEAKER, LOW);
    
    Serial.println("✅ Laser Pulsar System initialized");
  }
  
  void update() 
  {
    firePulsesMissing();
  }

  void firePulsesMissing()
  {
    // 🔥 PULS-LOGIK: Wenn Laser aktiv UND Zeit abgelaufen UND noch nicht fertig
    if(laserOn && (millis() - lastFired) > pulsePeriod && firedPulses < totalPulses)
    {
      firePulse();
      lastFired = millis();
      firedPulses += 1;
      
      // Fortschritt anzeigen (jeden 10. Puls oder wenn fertig)
      if (firedPulses % 10 == 0 || firedPulses == totalPulses) {
        Serial.print("🔫 Laser Pulse ");
        Serial.print(firedPulses);
        Serial.print("/");
        Serial.println(totalPulses);
      }
      
      finiteStateMachine::setState(SYS_LASER_ACTIVE); // Setze State auf Laser aktiv
    }
    
    // 🛑 AUTO-STOP: Wenn alle Pulse abgefeuert und noch nicht als beendet markiert
    if(firedPulses >= totalPulses && totalPulses > 0 && !sequenceCompleted)
    {
      laserOn = false;
      sequenceCompleted = true; // Markiere Sequenz als abgeschlossen
      Serial.println("OK:LASER_DONE");
      printLaserStatus();
      finiteStateMachine::setState(SYS_IDLE); // Neuen State setzen
    }

    if (firedPulses >= totalPulses && totalPulses != 0)
    {
      firedPulses = 0;
      totalPulses = 0;
    }
    
  }
  
  void processLaserCommand(const String& command) 
  {
    if (command.startsWith("CMD:LASER_p")) 
    {
      // Befehl: CMD:LASER_p50f5
      int pIndex = command.indexOf('p');
      int fIndex = command.indexOf('f');
      
      if (pIndex != -1 && fIndex != -1) 
      {
        unsigned long pulses = command.substring(pIndex + 1, fIndex).toInt();
        double frequency = command.substring(fIndex + 1).toFloat();
        
        startLaserSequence(pulses, frequency);
      } 
      else 
      {
        Serial.println("❌ Ungültiges Format: CMD:LASER_p<anzahl>f<frequenz>");
      }
    }
    else if (command == "CMD:LASER_stop") 
    {
      stopLaser();
    }
    else if (command == "CMD:LASER_killp") 
    {
      killPower();
    }
    else if (command == "CMD:LASER_restorep") 
    {
      restorePower();
    }
    else if (command == "CMD:LASER_status") 
    {
      printLaserStatus();
    }
    else if (command == "CMD:LASER_test") 
    {
      test();  // Einfacher Pin-Test
    }
    else 
    {
      Serial.println("❌ Unbekannter Laser-Befehl: " + command);
    }
  }


  void startLaserSequence(unsigned long pulses, double frequency) 
  {
    if (pulses <= 0 || frequency <= 0) {
      Serial.println("❌ Ungültige Parameter: pulses>0 und frequency>0 required");
      return;
    }
    if (laserOn) {
      Serial.println("❌ Laser Sequence läuft bereits");
      return;
    }
    if (digitalRead(LASER_RELAY) == HIGH) {
      Serial.println("❌ Laser-Stromversorgung ist getrennt. Bitte wiederherstellen.");
      return;
    }
    
    totalPulses = pulses;
    pulsePeriod = (unsigned long)(1000.0 / frequency); // ms zwischen Pulsen
    firedPulses = 0;
    sequenceCompleted =false;
    
    Serial.print("🚀 Starte Laser Sequence: ");
    Serial.print(pulses);
    Serial.print(" Pulse @ ");
    Serial.print(frequency, 1);
    Serial.println(" Hz");
    
    // Alarm abspielen
    alarm();
    
    // Laser starten
    laserOn = true;
    lastFired = millis(); // Timer starten
    
    finiteStateMachine::setState(SYS_LASER_ACTIVE);
  }
  
  void stopLaser() 
  {
    laserOn = false;
    totalPulses = 0;
    firedPulses = 0;
    sequenceCompleted = false;
    Serial.println("🛑 Laser gestoppt");
    finiteStateMachine::setState(SYS_IDLE);
  }
  
  void killPower() 
  {
    laserOn = false;
    totalPulses = 0;
    firedPulses = 0;
    sequenceCompleted = false;
    if ( digitalRead(LASER_RELAY) == HIGH ) 
    {
      if (finiteStateMachine::getState() == SYS_LASER_ACTIVE) 
        {
        finiteStateMachine::setState(SYS_IDLE);
        Serial.println("🛑 Laser gestoppt vor Stromtrennung");
        }
      return;
    }
    digitalWrite(LASER_RELAY, HIGH);
    Serial.println("🔌 Laser-Stromversorgung getrennt");
  }
  
  void restorePower() 
  {
    if( digitalRead(LASER_RELAY) == LOW ) {
      Serial.println("⚡ Laser-Stromversorgung ist bereits aktiv");
      return;
    }
    digitalWrite(LASER_RELAY, LOW);
    Serial.println("⚡ Laser-Stromversorgung wiederhergestellt");
  }
  
  void firePulse() 
  {
    digitalWrite(LASER_PIN, HIGH);
    delayMicroseconds(pulsePeriod *1000); // hier später anpassen (*1000 nur zum vorzeigen)
    digitalWrite(LASER_PIN, LOW);
    delay(100); // Kurze Pause nach dem Puls
  }
  
  void blockingTone(unsigned int frequency, unsigned long duration) 
  {
    tone(LASER_SPEAKER, frequency, duration);
    delay(duration);
  }
  
  void alarm() 
  {
    Serial.println("🔊 Alarm sound...");
    blockingTone(500, 500);
    delay(250);
    blockingTone(500, 500);
    delay(250);
    blockingTone(500, 500);
  }

  void test() 
  {
    if (!laserOn) 
    {
      Serial.println("🔴 Starte Laser-Test...");
      
      // LASER_PIN für 1 Sekunde
      Serial.println("💡 LASER_PIN (13) -> HIGH");
      digitalWrite(LASER_PIN, HIGH);
      delay(1000);
      digitalWrite(LASER_PIN, LOW);
      Serial.println("💡 LASER_PIN (13) -> LOW");
      delay(500); // Kurze Pause
      
      // LASER_RELAY für 1 Sekunde
      Serial.println("🔌 LASER_RELAY (12) -> HIGH");
      digitalWrite(LASER_RELAY, HIGH);
      delay(1000);
      digitalWrite(LASER_RELAY, LOW);
      Serial.println("🔌 LASER_RELAY (12) -> LOW");
      delay(500); // Kurze Pause
      
      // LASER_SPEAKER für 1 Sekunde
      Serial.println("🔊 LASER_SPEAKER (11) -> HIGH");
      digitalWrite(LASER_SPEAKER, HIGH);
      delay(1000);
      digitalWrite(LASER_SPEAKER, LOW);
      Serial.println("🔊 LASER_SPEAKER (11) -> LOW");
      
      Serial.println("✅ Laser-Test abgeschlossen");
    }
    else 
    {
      Serial.println("⚠️ Laser ist bereits aktiv - Test nicht möglich");
    }
  }
  
  bool isLaserActive() 
  {
    return laserOn;
  }

  bool isSequenceCompleted() 
  {
    return sequenceCompleted;
  }
  
  void printLaserStatus() 
  {
    Serial.print("Laser Status: ");
    Serial.print(laserOn ? "ACTIVE" : "INACTIVE");
    Serial.print(" | Progress: ");
    Serial.print(firedPulses);
    Serial.print("/");
    Serial.print(totalPulses);
    Serial.print(" | Relay: ");
    Serial.println(digitalRead(LASER_RELAY) ? "OFF" : "ON");
  }
} // Ende des Namespace