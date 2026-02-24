#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>
#include "vinyl.h"


/*


 *
 * Structure du code :
 * 1. Déclaration des objets audio et connexions
 * 2. Classe MyDSP pour encapsuler la logique
 * 3. Setup et Loop principaux
 */


// ===============================================
// AUDIO OBJECTS DECLARATION
// ===============================================


AudioInputUSB usb1;


// Filtres EQ pour le son vinyl
AudioFilterStateVariable vinylEqR;
AudioFilterStateVariable vinylEqL;


// Delay pour simulation de variation de vitesse (pitch)
AudioEffectDelay delayL;
AudioEffectDelay delayR;


// Générateurs de craquements
AudioSynthWaveform popOscLight;         // Craquements légers (poussière)
AudioSynthWaveform popOscDeep;          // Craquements profonds (défaut)
AudioSynthNoiseWhite scratchNoise;      // Bruit pour rayures
AudioEffectEnvelope popEnvelopeLight;
AudioEffectEnvelope popEnvelopeDeep;
AudioEffectEnvelope scratchEnvelope;


// Bruit de fond vinyl
AudioSynthNoisePink pinkNoise;


// Needle drop au démarrage (plus subtil que DJ scratch)
AudioSynthWaveform needleDropOsc;
AudioEffectEnvelope needleDropEnvelope;
AudioSynthNoiseWhite needleDropNoise;
AudioMixer4 needleDropMixer;


// Mixers
AudioMixer4 cracklesMixer;              // Mix tous les craquements
AudioMixer4 effectsCombiner;            // Combine crackles + DJ scratch
AudioMixer4 mixerL;                     // Mixer final gauche
AudioMixer4 mixerR;                     // Mixer final droite


AudioOutputI2S i2s1;
AudioControlSGTL5000 sgtl5000_1;


// ===============================================
// AUDIO CONNECTIONS
// ===============================================


// Signal principal : USB -> EQ -> Delay (pour pitch instable)
AudioConnection patchCord1(usb1, 0, vinylEqR, 0);
AudioConnection patchCord2(usb1, 1, vinylEqL, 0);
AudioConnection patchCord3(vinylEqL, 0, delayL, 0);
AudioConnection patchCord4(vinylEqR, 0, delayR, 0);


// Delay -> Mixer (son filtré avec pitch instable)
AudioConnection patchCord5(delayL, 0, mixerL, 0);
AudioConnection patchCord6(delayR, 0, mixerR, 0);


// USB direct -> Mixer (son clair)
AudioConnection patchCord7(usb1, 0, mixerL, 1);
AudioConnection patchCord8(usb1, 1, mixerR, 1);


// Bruit de fond
AudioConnection patchCord9(pinkNoise, 0, mixerL, 2);
AudioConnection patchCord10(pinkNoise, 0, mixerR, 2);


// Craquements -> Crackles Mixer
AudioConnection patchCordC1(popOscLight, 0, popEnvelopeLight, 0);
AudioConnection patchCordC2(popOscDeep, 0, popEnvelopeDeep, 0);
AudioConnection patchCordC3(scratchNoise, 0, scratchEnvelope, 0);
AudioConnection patchCordC4(popEnvelopeLight, 0, cracklesMixer, 0);
AudioConnection patchCordC5(popEnvelopeDeep, 0, cracklesMixer, 1);
AudioConnection patchCordC6(scratchEnvelope, 0, cracklesMixer, 2);


// Needle Drop -> Needle Drop Mixer
AudioConnection patchCordN1(needleDropOsc, 0, needleDropEnvelope, 0);
AudioConnection patchCordN2(needleDropEnvelope, 0, needleDropMixer, 0);
AudioConnection patchCordN3(needleDropNoise, 0, needleDropMixer, 1);


// Crackles -> Effects Combiner
AudioConnection patchCord11(cracklesMixer, 0, effectsCombiner, 0);


// Needle Drop -> Effects Combiner
AudioConnection patchCord12(needleDropMixer, 0, effectsCombiner, 1);


// Effects Combiner -> Mixer final
AudioConnection patchCord13(effectsCombiner, 0, mixerL, 3);
AudioConnection patchCord14(effectsCombiner, 0, mixerR, 3);


// Sortie
AudioConnection patchCord15(mixerL, 0, i2s1, 0);
AudioConnection patchCord16(mixerR, 0, i2s1, 1);



// ===============================================
// VARIABLES GLOBALES
// ===============================================


const int BUTTON_PIN = 2;
const int POT_INTENSITY_PIN = A8;  // Potentiomètre pour l'intensité de l'effet
const int POT_VOLUME_PIN = A5;     // Potentiomètre pour le volume global
const int modeButtonPin = 3;


MyDSP dsp;
bool lastButtonState = HIGH;


// ===============================================
// SETUP
// ===============================================


void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(modeButtonPin, INPUT_PULLUP);
  dsp.begin();
}


// ===============================================
// LOOP
// ===============================================


void loop() {
  // Lecture du bouton
  bool buttonState = digitalRead(BUTTON_PIN);
 
  if (lastButtonState == HIGH && buttonState == LOW) {
    dsp.toggleVinyl();
    delay(200);  // Anti-rebond
  }
 
  lastButtonState = buttonState;
  static bool lastModeButtonState = HIGH;
  bool modeButtonState = digitalRead(modeButtonPin);

  if (modeButtonState == LOW && lastModeButtonState == HIGH) {
    dsp.toggleVinylType();
    delay(200); // anti-rebond simple
  }

  lastModeButtonState = modeButtonState;
 
  // Lecture du potentiomètre d'intensité (A8)
  int intensityKnobValue = analogRead(POT_INTENSITY_PIN);
  float intensity = (float)intensityKnobValue / 1023.0;
 
  // Lecture du potentiomètre de volume (A0)
  int volumeKnobValue = analogRead(POT_VOLUME_PIN);
  float volume = (float)volumeKnobValue / 1023.0;
 
  // Mise à jour du DSP
  dsp.update(intensity);
  dsp.updateVolume(volume);

  static unsigned long lastSend = 0;

  if (millis() - lastSend > 50) { //bloc pour pas surcharger le buffer 
      Serial.print("[INTENSITY]");
      Serial.println(intensity);

      Serial.print("[VOLUME]");
      Serial.println(volume);

      lastSend = millis();
  }
 
  delay(10);
}



/*
 * ===============================================
 * NOTES TECHNIQUES
 * ===============================================
 *
 * Structure du code :
 * - Tous les objets audio sont déclarés globalement (requis par Teensy Audio Library)
 * - La logique est encapsulée dans la classe MyDSP
 * - Le setup() et loop() sont minimalistes et délèguent à MyDSP
 *
 *
 * 1. SON PLUS ÉTOUFFÉ (nouveau)
 *    - Fréquence du filtre abaissée : 600/650 Hz
 *    - Résonance augmentée : 2.5/2.6
 *    - Mix ajusté : 85% filtré / 15% direct
 *    - Résultat : son beaucoup plus "muffled" et vintage
 *
 * 2. DEUX POTENTIOMÈTRES (nouveau)
 *    - A8 : Intensité de l'effet vinyl (bruit + fréquence des craquements)
 *    - A0 : Volume global (0-80% du volume max)
 *
 * 3. NEEDLE DROP subtil
 *    - Fréquence 80->50 Hz avec sweep doux
 *    - Bruit d'impact léger
 *    - Durée courte (~400ms)
 *
 * 4. FLUCTUATIONS PRONONCÉES (wow & flutter)
 *    - Wow : ±150 Hz à 0.4 Hz (variation lente de vitesse)
 *    - Flutter : ±60 Hz à 4 Hz (variation rapide)
 *    - Micro-variations : ±20 Hz à 8 Hz (texture)
 *
 * 5. PITCH INSTABLE via delay line
 *    - Delay variable 2-8ms (±3ms)
 *    - Simule les variations de vitesse du moteur
 *    - Différence stéréo pour plus de réalisme
 *
 * 6. POPS TRÈS PRÉSENTS
 *    - Amplitudes augmentées : 1.2-1.5
 *    - Volumes aléatoires : 0.7-1.6
 *    - Fréquence augmentée : 80% de chance pour les légers
 *    - Gain du mixer final : 0.5
 *
 * Contrôles :
 * - Bouton (pin 2) : Active/désactive le mode vinyl
 * - Potentiomètre A8 : Intensité de l'effet
 * - Potentiomètre A0: Volume global de sortie
 */


