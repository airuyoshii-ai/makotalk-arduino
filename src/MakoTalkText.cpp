#include "MakoTalkText.h"
#include <stdint.h>

static void utf8(std::string& out,uint32_t c){
 if(c<128)out.push_back(char(c));
 else if(c<2048){out.push_back(char(0xc0|(c>>6)));out.push_back(char(0x80|(c&63)));}
 else{out.push_back(char(0xe0|(c>>12)));out.push_back(char(0x80|((c>>6)&63)));out.push_back(char(0x80|(c&63)));}
}
bool mktNormalizeKana(const char* input,std::string& output,const char** error){
 output.clear();*error=nullptr;
 if(!input||!*input){*error="Empty kana text";return false;}
 const unsigned char* p=(const unsigned char*)input;
 while(*p){
  uint32_t c;unsigned n;
  if(*p<128){c=*p;n=1;}
  else if(*p>=0xc2&&*p<=0xdf){c=*p&31;n=2;}
  else if(*p>=0xe0&&*p<=0xef){c=*p&15;n=3;}
  else{*error="Unsupported or invalid UTF-8; use full-width kana";output.clear();return false;}
  for(unsigned i=1;i<n;i++){
   if(p[i]<0x80||p[i]>0xbf){*error="Invalid UTF-8";output.clear();return false;}
   c=(c<<6)|(p[i]&63);
  }
  if((n==3&&(c<0x800||(c>=0xd800&&c<=0xdfff)))){*error="Invalid UTF-8";output.clear();return false;}
  p+=n;
  if(c>=0x3041&&c<=0x3096)c+=0x60;
  if(c==0xff01)c='!';if(c==0xff1f)c='?';
  if(c==' '||c=='\t'||c=='\r'||c=='\n'||c==0x3000)c='_';
  if(c=='.')c=0x3002;if(c==',')c=0x3001;
  bool control=c=='_'||c=='\''||c=='!'||c=='?';
  if(!control&&!(c>=0x30a1&&c<=0x30fa)&&c!=0x30fc&&c!=0x3001&&c!=0x3002){
   *error="Kana only: kanji, Latin text, numbers and combining marks need explicit readings";output.clear();return false;
  }
  utf8(output,c);
  if(output.size()>4096){*error="Text too long; split the sentence";output.clear();return false;}
 }
 return !output.empty();
}
