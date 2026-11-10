#include "MakoTalk.h"
#include "MakoTalkText.h"
#include <math.h>
#include <string.h>

void MakoTalkSpeaker::end(){
 _ready=false;_engine.end();
 if(_installed){i2s_driver_uninstall(_port);_installed=false;}
}
bool MakoTalkSpeaker::begin(const MakoTalkVoiceImage& image,int bclk,int lrclk,int data,i2s_port_t port,float seconds){
 end();_error.clear();
 if((port!=I2S_NUM_0&&port!=I2S_NUM_1)||bclk<0||lrclk<0||data<0||bclk==lrclk||bclk==data||lrclk==data){_error="Invalid I2S port or pins";return false;}
 if(!isfinite(seconds)||seconds<8.f||seconds>30.f){_error="Buffer must be 8..30 seconds to preserve the startup credit";return false;}
 const char* err=mktLoadImage(image.data,image.bytes,&_checked,_kana,MKT_IMAGE_MAX_UNITS);
 if(err){_error=err;return false;}
 if(_checked.voice.sampleRate!=16000){_error="This I2S adapter requires a 16 kHz voice";return false;}
 _port=port;
 i2s_config_t c={};c.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX);c.sample_rate=16000;
 c.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;c.bits_per_chan=I2S_BITS_PER_CHAN_16BIT;
 c.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;c.communication_format=I2S_COMM_FORMAT_STAND_I2S;
 c.dma_buf_count=4;c.dma_buf_len=128;c.tx_desc_auto_clear=true;
 if(i2s_driver_install(port,&c,0,nullptr)!=ESP_OK){_error="I2S install failed or port already used";return false;}
 _installed=true;
 i2s_pin_config_t pins={};pins.mck_io_num=I2S_PIN_NO_CHANGE;pins.bck_io_num=bclk;pins.ws_io_num=lrclk;pins.data_out_num=data;pins.data_in_num=I2S_PIN_NO_CHANGE;
 if(i2s_set_pin(port,&pins)!=ESP_OK){_error="I2S pin setup failed";end();return false;}
 _engine.setVolume(70);
 const MKT_Output out={writeOutput,creditOutput,this};
 if(!_engine.beginImage(image.data,image.bytes,out,seconds)){
  _error=_engine.lastError();end();return false;
 }
 _ready=true;return true;
}
bool MakoTalkSpeaker::validateKana(const std::string& text){
 const char* p=text.c_str();unsigned morae=0;
 while(*p){
  if(*p=='\''||*p=='!'||*p=='?'){p++;continue;}
  if(*p=='_'){p++;morae++;}
  else if(!strncmp(p,"。",3)||!strncmp(p,"、",3)||!strncmp(p,"ー",3)||!strncmp(p,"ッ",3)){p+=3;morae++;}
  else{
   size_t longest=0;
   for(unsigned i=0;i<_checked.voice.unitCount;i++){
    size_t n=strlen(_checked.voice.kana[i]);if(n>longest&&!strncmp(p,_checked.voice.kana[i],n))longest=n;
   }
   if(!longest){_error="Voice does not contain a kana sequence in this text";return false;}
   p+=longest;morae++;
  }
  if(morae>MKT_MAX_MORAE){_error="Too many morae; split the sentence";return false;}
 }
 if(!morae){_error="No speakable kana";return false;}
 return true;
}
bool MakoTalkSpeaker::speak(const char* kana){
 _error.clear();if(!_ready){_error="Call mbegin() first";return false;}
 std::string text;const char* err=nullptr;
 if(!mktNormalizeKana(kana,text,&err)){_error=err?err:"Kana conversion failed";return false;}
 if(!validateKana(text))return false;
 const int16_t* pcm=nullptr;size_t n=0;
 // Never play or silently accept a truncated phrase.
 if(!_engine.synth(text.c_str(),&pcm,&n)||strlen(_engine.lastError())){_error=_engine.lastError();return false;}
 if(!_engine.play(pcm,n)){if(_error.empty())_error=_engine.lastError();return false;}
 return true;
}
bool MakoTalkSpeaker::announceCredit(){
 _error.clear();if(!_ready){_error="Call mbegin() first";return false;}
 if(!_engine.announceCredit()){if(_error.empty())_error=_engine.lastError();return false;}
 return true;
}
bool MakoTalkSpeaker::writeOutput(const int16_t* p,size_t n,void* ctx){return ((MakoTalkSpeaker*)ctx)->write(p,n);}
void MakoTalkSpeaker::creditOutput(const MKT_VoiceInfo& v,void*){
 Serial.printf("MakoTalk voice: %s\nTerms: %s\nNotice: %s\n",v.creditDisplay,v.termsUrl,v.noticePath);
}
bool MakoTalkSpeaker::write(const int16_t* pcm,size_t count){
 int peak=1;double rmsMax=1;
 for(size_t o=0;o<count;o+=320){size_t n=(count-o<320?count-o:320);double sq=0;
  for(size_t i=0;i<n;i++){int v=pcm[o+i];if(abs(v)>peak)peak=abs(v);sq+=double(v)*v;}
  double rms=sqrt(sq/n);if(rms>rmsMax)rmsMax=rms;
 }
 float gain=fminf(1.f,fminf(12288.f/peak,5000.f/float(rmsMax)));int16_t stereo[512];
 for(size_t o=0;o<count;o+=256){size_t n=(count-o<256?count-o:256);
  for(size_t i=0;i<n;i++)stereo[2*i]=stereo[2*i+1]=int16_t(pcm[o+i]*gain);
  size_t written=0;if(i2s_write(_port,stereo,n*4,&written,pdMS_TO_TICKS(1000))!=ESP_OK||written!=n*4){_error="I2S output failed";return false;}
 }
 memset(stereo,0,sizeof(stereo));
 for(int i=0;i<5;i++){size_t n=0;if(i2s_write(_port,stereo,sizeof(stereo),&n,pdMS_TO_TICKS(1000))!=ESP_OK||n!=sizeof(stereo)){_error="I2S drain failed";return false;}}
 i2s_zero_dma_buffer(_port);return true;
}
MakoTalkSpeaker& makotalk(){static MakoTalkSpeaker instance;return instance;}
bool mbegin(const MakoTalkVoiceImage& v,int b,int l,int d,i2s_port_t p,float seconds){return makotalk().begin(v,b,l,d,p,seconds);}
bool mspeak(const char* s){return makotalk().speak(s);}
bool mannounceCredit(){return makotalk().announceCredit();}
const char* mlastError(){return makotalk().lastError();}
void msetSpeed(uint16_t n){makotalk().setSpeed(n);}
void msetPitch(uint8_t n){makotalk().setPitch(n);}
void msetVolume(uint8_t n){makotalk().setVolume(n);}
void mend(){makotalk().end();}
