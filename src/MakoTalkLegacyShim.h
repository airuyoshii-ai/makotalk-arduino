#pragma once
#include <Arduino.h>
// Private placeholder for the unused engine-owned I2S.
// Registered MKT_Output handles real playback through driver/i2s.h.
#define I2S_MODE_STD 0
#define I2S_DATA_BIT_WIDTH_16BIT 0
#define I2S_SLOT_MODE_MONO 0
class MKT_InactiveI2S { public:
 void setPins(int,int,int,int,int) {}
 bool begin(int,int,int,int) { return false; }
 void end() {}
 size_t write(uint8_t*,size_t) { return 0; }
};
