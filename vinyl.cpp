#include "vinyl.h"
#include <Audio.h>

// IMPORTANT : on déclare les objets audio comme extern
extern AudioFilterStateVariable vinylEqR;
extern AudioFilterStateVariable vinylEqL;

extern AudioEffectDelay delayL;
extern AudioEffectDelay delayR;

extern AudioSynthWaveform popOscLight;
extern AudioSynthWaveform popOscDeep;
extern AudioSynthNoiseWhite scratchNoise;
extern AudioEffectEnvelope popEnvelopeLight;
extern AudioEffectEnvelope popEnvelopeDeep;
extern AudioEffectEnvelope scratchEnvelope;

extern AudioSynthNoisePink pinkNoise;

extern AudioSynthWaveform needleDropOsc;
extern AudioEffectEnvelope needleDropEnvelope;
extern AudioSynthNoiseWhite needleDropNoise;
extern AudioMixer4 needleDropMixer;

extern AudioMixer4 cracklesMixer;
extern AudioMixer4 effectsCombiner;
extern AudioMixer4 mixerL;
extern AudioMixer4 mixerR;

extern AudioControlSGTL5000 sgtl5000_1;


// ===============================================
// CLASSE MyDSP - ENCAPSULATION DE LA LOGIQUE
// ===============================================


MyDSP::MyDSP() {
    vinylEnabled = false;
    vinylIntensity = 0.5;
   
    lastLightPopTime = 0;
    lastDeepPopTime = 0;
    lastScratchTime = 0;
    nextLightPopInterval = 300;
    nextDeepPopInterval = 2000;
    nextScratchInterval = 5000;
   
    needleDropActive = false;
    needleDropStartTime = 0;
   
    currentDelayTime = 5.0;  // Initialiser à 5ms
   
    baseNoiseGain = 0.025;
    baseEqFreqL = 600;  
    baseEqFreqR = 650;  
    lastVol = -1;

    currentType = VINYL_78; // défaut ancien
  }


  // ===== INITIALISATION =====
void MyDSP::begin() {
    Serial.begin(9600);
    AudioMemory(60);


    sgtl5000_1.enable();
    sgtl5000_1.volume(0.5);


    setupAudioObjects();
    setNormalMode();


    Serial.println("===========================================");
    Serial.println("VINYL FILTER EFFECT - Ready!");
    Serial.println("===========================================");
}


  // ===== CONFIGURATION DES OBJETS AUDIO =====
void MyDSP::setupAudioObjects() {
    // EQ Vinyl - PLUS ÉTOUFFÉ pour un son vraiment lo-fi
    vinylEqL.frequency(600);    // Baissé de 800 à 600 Hz
    vinylEqL.resonance(2.5);    // Augmenté pour accentuer l'effet
    vinylEqR.frequency(650);    // Baissé de 850 à 650 Hz
    vinylEqR.resonance(2.6);    // Augmenté pour accentuer l'effet


    // Delay pour pitch instable (variation de vitesse de platine)
    delayL.delay(0, 5);  // Delay initial très court (5ms)
    delayR.delay(0, 5);


    // Bruit de fond
    pinkNoise.amplitude(1.0);


    // Craquements légers (VOLUMES AUGMENTÉS)
    popOscLight.begin(WAVEFORM_TRIANGLE);
    popOscLight.frequency(150);
    popOscLight.amplitude(1.2);  // Augmenté
    popEnvelopeLight.attack(0);
    popEnvelopeLight.hold(1);
    popEnvelopeLight.decay(8);
    popEnvelopeLight.sustain(0);


    // Craquements profonds (VOLUMES AUGMENTÉS)
    popOscDeep.begin(WAVEFORM_SAWTOOTH);
    popOscDeep.frequency(40);
    popOscDeep.amplitude(1.5);  // Augmenté
    popEnvelopeDeep.attack(0);
    popEnvelopeDeep.hold(3);
    popEnvelopeDeep.decay(25);
    popEnvelopeDeep.sustain(0);


    // Rayures (VOLUMES AUGMENTÉS)
    scratchNoise.amplitude(1.3);  // Augmenté
    scratchEnvelope.attack(0);
    scratchEnvelope.hold(8);
    scratchEnvelope.decay(40);
    scratchEnvelope.sustain(0);


    // Needle Drop (subtil et réaliste)
    needleDropOsc.begin(WAVEFORM_SAWTOOTH);
    needleDropOsc.frequency(120);
    needleDropOsc.amplitude(0.6);
    needleDropEnvelope.attack(5);      // Attaque douce
    needleDropEnvelope.hold(30);       // Court
    needleDropEnvelope.decay(200);     // Déclin progressif
    needleDropEnvelope.sustain(0.05);
    needleDropEnvelope.release(150);
   
    needleDropNoise.amplitude(0.4);    // Bruit d'impact
   
    needleDropMixer.gain(0, 1.0);      // Oscillateur
    needleDropMixer.gain(1, 0.7);      // Bruit
    needleDropMixer.gain(2, 0.0);
    needleDropMixer.gain(3, 0.0);


    // Mixers des effets
    cracklesMixer.gain(0, 1.0);  // Légers
    cracklesMixer.gain(1, 1.0);  // Profonds
    cracklesMixer.gain(2, 1.0);  // Rayures
    cracklesMixer.gain(3, 0.0);
   
    effectsCombiner.gain(0, 1.0);  // Crackles
    effectsCombiner.gain(1, 0.0);  // DJ Scratch (activé quand nécessaire)
    effectsCombiner.gain(2, 0.0);
    effectsCombiner.gain(3, 0.0);
}


  // ===== MODE NORMAL (pas d'effet) =====
void MyDSP::setNormalMode() {
    mixerL.gain(0, 0.0);  // Filtré OFF
    mixerL.gain(1, 1.0);  // Direct ON
    mixerL.gain(2, 0.0);  // Bruit OFF
    mixerL.gain(3, 0.0);  // Effets OFF


    mixerR.gain(0, 0.0);
    mixerR.gain(1, 1.0);
    mixerR.gain(2, 0.0);
    mixerR.gain(3, 0.0);
  }


  // ===== MODE VINYL =====
void MyDSP::setVinylMode() {

  if (currentType == VINYL_78){
    // Mix 85% filtré + 15% direct pour un son plus étouffé
    // (moins de signal direct = son plus "muffled")
    // Son très étouffé
    baseEqFreqL = 600;
    baseEqFreqR = 650;
    baseNoiseGain = 0.03;
    mixerL.gain(0, 0.85);
    mixerL.gain(1, 0.15);  // Réduit de 25% à 15%
   
    mixerR.gain(0, 0.85);
    mixerR.gain(1, 0.15);
  }
  else { // VINYL_33

    // Son plus propre
    baseEqFreqL = 1000;
    baseEqFreqR = 1250;
    baseNoiseGain = 0.015;

    mixerL.gain(0, 0.7);
    mixerL.gain(1, 0.3);
 
    mixerR.gain(0, 0.7);
    mixerR.gain(1, 0.3);

  }
   
    updateNoiseLevels();
    updateEffectsLevels();
  }


  // ===== MISE À JOUR DU BRUIT =====
void MyDSP::updateNoiseLevels() {
    float timeInSeconds = millis() / 1000.0;
    float noiseVariation = (sin(timeInSeconds * 0.3) + 1.0) * 0.5;
    float dynamicNoiseGain = baseNoiseGain + (noiseVariation * 0.02 * vinylIntensity);
   
    mixerL.gain(2, dynamicNoiseGain);
    mixerR.gain(2, dynamicNoiseGain * 0.95);
  }


  // ===== FLUCTUATION DE QUALITÉ (WOW & FLUTTER) + PITCH INSTABLE =====
void MyDSP::updateVinylFluctuation() {
    if (!vinylEnabled) return;
   
    float timeInSeconds = millis() / 1000.0;
   
    float wow;
    float flutter;

    if (currentType == VINYL_78) {
      wow = sin(timeInSeconds * 0.4) * 150.0;
      flutter = sin(timeInSeconds * 4.0) * 60.0;
    }else { // 33 tours
    wow = sin(timeInSeconds * 0.3) * 60.0;
    flutter = sin(timeInSeconds * 3.0) * 20.0;
    }
   
    // Micro-variations aléatoires pour plus de réalisme
    float microVar = sin(timeInSeconds * 8.0) * 20.0;  // ±20 Hz
   
    // Appliquer aux filtres avec légère différence stéréo
    vinylEqL.frequency(baseEqFreqL + wow + flutter + microVar);
    vinylEqR.frequency(baseEqFreqR + wow * 0.95 + flutter * 1.05 + microVar * 0.9);
   
    // NOUVEAU : Variation du delay pour simuler pitch instable
    // Le delay varie légèrement pour créer l'effet de variation de vitesse
    float delayVariation = sin(timeInSeconds * 0.6) * 3.0;  // ±3ms
    float newDelayTime = 5.0 + delayVariation;  // 2-8ms
   
    // Limiter les variations brusques
    if (abs(newDelayTime - currentDelayTime) < 2.0) {
      currentDelayTime = newDelayTime;
      delayL.delay(0, currentDelayTime);
      delayR.delay(0, currentDelayTime * 1.02);  // Légère différence stéréo
    }
  }


  // ===== MISE À JOUR DES EFFETS =====
void MyDSP::updateEffectsLevels() {
    if (!needleDropActive) {
      // Gain augmenté pour que les pops soient plus présents
      mixerL.gain(3, 0.5 * vinylIntensity);
      mixerR.gain(3, 0.5 * vinylIntensity);
    }
  }


  // ===== TOGGLE VINYL =====
void MyDSP::toggleVinyl() {
    vinylEnabled = !vinylEnabled;
   
    if (vinylEnabled) {
      Serial.println("[VINYL] ON");
      setVinylMode();
      triggerNeedleDrop();  // Needle drop au lieu de DJ scratch
    } else {
      Serial.println("[VINYL] OFF");
      setNormalMode();
    }
  }

void MyDSP::toggleVinylType() {

  if (currentType == VINYL_78) {
    currentType = VINYL_33;
    Serial.println("[TYPE] 33");
  } else {
    currentType = VINYL_78;
    Serial.println("[TYPE] 78");
  }

  if (vinylEnabled) {
    setVinylMode();
  }
}

  // ===== CRAQUEMENTS LÉGERS =====
void MyDSP::triggerLightPop() {
    int freq = random(100, 300);
    float vol = (float)random(70, 130) / 100.0;  // AUGMENTÉ (0.7-1.3)
   
    popOscLight.frequency(freq);
    popOscLight.amplitude(vol);
    popEnvelopeLight.noteOn();
    popEnvelopeLight.noteOff();
  }


  // ===== CRAQUEMENTS PROFONDS =====
void MyDSP::triggerDeepPop() {
    int freq = random(30, 80);
    float vol = (float)random(100, 160) / 100.0;  // AUGMENTÉ (1.0-1.6)
   
    popOscDeep.frequency(freq);
    popOscDeep.amplitude(vol);
    popEnvelopeDeep.noteOn();
    popEnvelopeDeep.noteOff();
   
    //Serial.println("  * DEEP POP *");
}


  // ===== RAYURES =====
void MyDSP::triggerScratch() {
    float vol = (float)random(80, 150) / 100.0;  // AUGMENTÉ (0.8-1.5)
   
    scratchNoise.amplitude(vol);
    scratchEnvelope.noteOn();
    scratchEnvelope.noteOff();
   
    //Serial.println("  *** SCRATCH ***");
  }


  // ===== NEEDLE DROP =====
void MyDSP::triggerNeedleDrop() {
    needleDropActive = true;
    needleDropStartTime = millis();
   
    effectsCombiner.gain(1, 1.0);  // Activer le needle drop
    mixerL.gain(3, 0.3);  // Volume modéré
    mixerR.gain(3, 0.3);
   
    // Fréquence de départ plus basse pour le "thump"
    needleDropOsc.frequency(80);
    needleDropEnvelope.noteOn();
   
   // Serial.println("  >>> Needle Drop <<<");
  }


void MyDSP::updateNeedleDrop() {
    if (!needleDropActive) return;
   
    unsigned long elapsed = millis() - needleDropStartTime;
   
    // Sweep de fréquence subtil (80 -> 50 Hz)
    if (elapsed < 400) {
      float progress = (float)elapsed / 400.0;
      float freq = 80 - (progress * 30);  // Descend à 50 Hz
      needleDropOsc.frequency(freq);
    }
    else if (elapsed > 400) {
      //Fin du needle drop
      needleDropEnvelope.noteOff();
      effectsCombiner.gain(1, 0.0);
      needleDropActive = false;
      updateEffectsLevels();
    }
  }


  // ===== GESTION DES CRAQUEMENTS =====
void MyDSP::updateCrackles() {
    if (!vinylEnabled || needleDropActive) return;
   
    unsigned long currentTime = millis();
   
    // Craquements légers (fréquents et plus présents)
    if (currentTime - lastLightPopTime > nextLightPopInterval) {
      if (random(100) < 80) {  // 80% de chance (augmenté)
        triggerLightPop();
      }
      nextLightPopInterval = random(80, 600);  // Plus fréquent
      lastLightPopTime = currentTime;
    }
   
    // Craquements profonds (moins fréquents)
    if (currentTime - lastDeepPopTime > nextDeepPopInterval) {
      if (random(100) < (50 * vinylIntensity)) {  // 50% de chance max (augmenté)
        triggerDeepPop();
      }
      nextDeepPopInterval = random(1200, 3500);
      lastDeepPopTime = currentTime;
    }
   
    // Rayures (rares)
    if (currentTime - lastScratchTime > nextScratchInterval) {
      if (random(100) < (25 * vinylIntensity)) {  // 25% de chance max (augmenté)
        triggerScratch();
      }
      nextScratchInterval = random(3500, 9000);
      lastScratchTime = currentTime;
    }
  }


  // ===== MISE À JOUR GLOBALE =====
void MyDSP::update(float intensity) {
    vinylIntensity = intensity;
    
    if (vinylEnabled) {
      updateNoiseLevels();
      updateEffectsLevels();
      updateVinylFluctuation();  // Fluctuation de qualité + pitch instable
    }
   
    updateNeedleDrop();  // Needle drop
    updateCrackles();
  }


  // ===== GESTION DU VOLUME =====
void MyDSP::updateVolume(float volumeLevel) {
    // volumeLevel vient du potentiomètre (0.0 - 1.0)
    // On map ça sur une plage raisonnable : 0.0 - 0.8
    float vol = volumeLevel * 0.8;

    if (abs(vol - lastVol) > 0.02) {
      sgtl5000_1.volume(vol);
      lastVol = vol;
    }
  }


  // ===== GETTERS =====
bool MyDSP::isVinylEnabled() { return vinylEnabled; }

