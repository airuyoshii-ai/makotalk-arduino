#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include <string>
#include "MakoTalkEngine.h"

struct MakoTalkVoiceImage { const uint8_t* data; size_t bytes; };

// ESP32 transport + kana adapter. The voice image must outlive this object.
class MakoTalkSpeaker {
 public:
  MakoTalkSpeaker() = default;
  ~MakoTalkSpeaker(){end();}
  MakoTalkSpeaker(const MakoTalkSpeaker&)=delete;
  MakoTalkSpeaker& operator=(const MakoTalkSpeaker&)=delete;
  bool begin(const MakoTalkVoiceImage& voice,int bclk,int lrclk,int data,
             i2s_port_t port=I2S_NUM_1,float bufferSeconds=8.f);
  bool speak(const char* kana);
  bool announceCredit();
  void end();
  void setSpeed(uint16_t n){_engine.setSpeed(n);}
  void setPitch(uint8_t n){_engine.setPitch(n);}
  void setVolume(uint8_t n){_engine.setVolume(n);}
  const char* lastError() const{return _error.c_str();}
  const MKT_VoiceInfo& voiceInfo() const{return _engine.voiceInfo();}
 private:
  static bool writeOutput(const int16_t*,size_t,void*);
  static void creditOutput(const MKT_VoiceInfo&,void*);
  bool write(const int16_t*,size_t);
  bool validateKana(const std::string&);
  MakoTalk _engine;
  MKT_LoadedVoice _checked={};
  const char* _kana[MKT_IMAGE_MAX_UNITS]={};
  i2s_port_t _port=I2S_NUM_1;
  bool _installed=false,_ready=false;
  std::string _error;
};

MakoTalkSpeaker& makotalk();
bool mbegin(const MakoTalkVoiceImage&,int bclk,int lrclk,int data,
            i2s_port_t port=I2S_NUM_1,float bufferSeconds=8.f);
bool mspeak(const char* hiraganaOrKatakana);
bool mannounceCredit();
const char* mlastError();
void msetSpeed(uint16_t);
void msetPitch(uint8_t);
void msetVolume(uint8_t);
void mend();
