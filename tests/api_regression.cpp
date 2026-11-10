#include "MakoTalk.h"
#include "MakoTalkVoiceDefault.h"
#include "MakoTalkText.h"
#include "Preferences.h"
#include <assert.h>
#include <string.h>
#include <iostream>
#include <vector>
int main(){
 std::string normalized;const char* error=nullptr;
 assert(mktNormalizeKana("あいうえお",normalized,&error)&&normalized=="アイウエオ");
 assert(mktNormalizeKana("きゃ、ぎゅ。ふぅ！",normalized,&error)&&normalized=="キャ、ギュ。フゥ!");
 assert(!mktNormalizeKana("漢字",normalized,&error)&&normalized.empty());
 assert(!mktNormalizeKana("123",normalized,&error));
 assert(!mktNormalizeKana("a",normalized,&error));
 assert(!mktNormalizeKana("\xe3\x81",normalized,&error));
 assert(!mktNormalizeKana("\xed\xa0\x80",normalized,&error));
 assert(!mktNormalizeKana(nullptr,normalized,&error));
 assert(!mspeak("あ"));
 assert(mbegin(MAKOTALK_VOICE_DEFAULT,5,6,4));assert(saved&&marks==1&&!captured.empty());
 msetSpeed(130);captured.clear();assert(mspeak("あいうえお"));
 std::vector<int16_t> hiragana=captured;assert(hiragana.size()>1000);
 captured.clear();assert(mspeak("アイウエオ"));assert(captured==hiragana);
 captured.clear();assert(!mspeak("漢字")&&captured.empty()&&strlen(mlastError()));
 assert(!mspeak("ヸ")&&captured.empty());
 std::string longText;for(int i=0;i<193;i++)longText+="ア";
 assert(!mspeak(longText.c_str())&&captured.empty());
 longText.clear();for(int i=0;i<80;i++)longText+="ア";
 msetSpeed(40);assert(!mspeak(longText.c_str())&&captured.empty());
 msetSpeed(130);assert(mspeak("あいうえお")&&!strlen(mlastError()));
 for(int16_t n:captured)assert(abs(int(n))<=12288);
 captured.clear();mend();assert(installedPorts==0);
 // Same dictionary and stored credit: no repeated boot speech.
 assert(mbegin(MAKOTALK_VOICE_DEFAULT,5,6,4));assert(captured.empty()&&marks==1);mend();
 // Broken image rejected before I2S install or announcement flag updates.
 std::vector<uint8_t> bad(MKT_VOICE_DEFAULT_IMAGE,MKT_VOICE_DEFAULT_IMAGE+MKT_VOICE_DEFAULT_IMAGE_LEN);bad.back()^=1;
 MakoTalkVoiceImage broken={bad.data(),bad.size()};assert(!mbegin(broken,5,6,4));assert(!installedPorts&&marks==1);
 // Failed initial credit output must not mark the image as announced.
 saved=false;marks=0;failWrites=true;assert(!mbegin(MAKOTALK_VOICE_DEFAULT,5,6,4));assert(!saved&&!marks&&!installedPorts);
 failWrites=false;assert(mbegin(MAKOTALK_VOICE_DEFAULT,5,6,4));assert(saved&&marks==1);mend();
 assert(!stereoErrors);
 std::cout<<"PASS: hiragana=katakana PCM, kana validation, no partial speech, error recovery, CRC, startup credit, failed output, stereo/peak\n";
}
