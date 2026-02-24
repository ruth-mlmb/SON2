#ifndef VINYL_H
#define VINYL_H

#include <Arduino.h>

enum VinylType {
  VINYL_33,
  VINYL_78
};

// Forward declaration des objets audio globaux
class MyDSP {
private:
  bool vinylEnabled;
  float vinylIntensity;

  unsigned long lastLightPopTime;
  unsigned long lastDeepPopTime;
  unsigned long lastScratchTime;
  unsigned long nextLightPopInterval;
  unsigned long nextDeepPopInterval;
  unsigned long nextScratchInterval;

  bool needleDropActive;
  unsigned long needleDropStartTime;

  float currentDelayTime;
  float baseNoiseGain;
  float baseEqFreqL;
  float baseEqFreqR;

  float lastVol;

  VinylType currentType;


public:
  MyDSP();

  void begin();
  void setupAudioObjects();

  void setNormalMode();
  void setVinylMode();

  void updateNoiseLevels();
  void updateVinylFluctuation();
  void updateEffectsLevels();

  void toggleVinyl();
  void toggleVinylType();

  void triggerLightPop();
  void triggerDeepPop();
  void triggerScratch();

  void triggerNeedleDrop();
  void updateNeedleDrop();

  void updateCrackles();

  void update(float intensity);
  void updateVolume(float volumeLevel);

  bool isVinylEnabled();
};

#endif