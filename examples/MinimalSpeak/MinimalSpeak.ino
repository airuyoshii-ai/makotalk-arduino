#include <MakoTalk.h>
#include <MakoTalkVoiceDefault.h>

constexpr int BCLK = 5;
constexpr int LRCLK = 6;  // WS: I2Sの左右クロック。I2CのSDAではありません。
constexpr int DATA = 4;

void setup() {
  Serial.begin(115200);
  if (!mbegin(MAKOTALK_VOICE_DEFAULT, BCLK, LRCLK, DATA, I2S_NUM_1)) {
    Serial.println(mlastError());
    return;
  }
  msetSpeed(130);
  if (!mspeak("あいうえお")) Serial.println(mlastError());
}

void loop() {}
