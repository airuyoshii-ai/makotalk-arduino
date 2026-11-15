// render.cpp v1.0.0 2026-11-14 JST  host render: 実ライブラリ mspeak() をWAV化 (I2S/NVSはモック)
#include "MakoTalk.h"
#include "MakoTalkVoiceDefault.h"
#include "Preferences.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
static void wav(const std::string&p,const std::vector<int16_t>&d){std::ofstream f(p,std::ios::binary);uint32_t n=d.size()*2,sr=16000,br=32000,fs=16,ch=0x00010001,x;
 f.write("RIFF",4);x=36+n;f.write((char*)&x,4);f.write("WAVEfmt ",8);f.write((char*)&fs,4);uint16_t pcm=1,c=1,ba=2,bits=16;f.write((char*)&pcm,2);f.write((char*)&c,2);f.write((char*)&sr,4);f.write((char*)&br,4);f.write((char*)&ba,2);f.write((char*)&bits,2);f.write("data",4);f.write((char*)&n,4);f.write((char*)d.data(),n);}
int main(int,char**argv){
 mbegin(MAKOTALK_VOICE_DEFAULT,5,6,4);
 std::ifstream in(argv[1]);std::string line;
 while(std::getline(in,line)){if(line.empty()||line[0]=='#')continue;
  std::stringstream s(line);std::string name,sp,pi,vo,kana;
  std::getline(s,name,'|');std::getline(s,sp,'|');std::getline(s,pi,'|');std::getline(s,vo,'|');std::getline(s,kana);
  msetSpeed(std::stoi(sp));msetPitch(std::stoi(pi));msetVolume(std::stoi(vo));
  captured.clear();bool ok=mspeak(kana.c_str());
  std::cout<<name<<" ok="<<ok<<" n="<<captured.size()<<(ok?"":std::string(" err=")+mlastError())<<"\n";
  if(ok)wav(std::string(argv[2])+"/"+name+".wav",captured);}
 mend();}
