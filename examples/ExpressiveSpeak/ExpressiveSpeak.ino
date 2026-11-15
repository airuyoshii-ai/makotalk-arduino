/*
  ExpressiveSpeak.ino  v1.2.0  2026-11-15 JST
  変更: v1.1.0 → 公式の最小例に合わせ、#include とピン指定を実際の形に修正。
        基準の速度を、最小例と同じ 130 に変更(数値は試し聴き用の初期値。ESP32-S3 実機は未確認)。

  ■ 使い方
  書き込むと、同じ文を「ふつう → 変えた声」の順に話します。聴き比べて、数字を変えてみてください。

  ■ 3つの数字の意味  (大きいほど…)
  speed  話す速さ。 100=標準。 130=最小例の値。 数字を上げると速く、下げるとゆっくり。 (40〜300)
  pitch  声の高さ。 100=ふつう。 75=かなり低め。 140=高め。 (40〜250)
  vol    音量。 0〜100。 70=初期値。 上げても、ある大きさで頭打ちになる。
  ※ 設定は次に変えるまで残るので、文ごとに3つとも指定しています。

  ■ かな文字列の記号
  '  声が下がる位置(アクセント)。直前の文字のあとで下がる。   ?  文末で上がる(疑問)
  !  元気・強調。高く、速くなる。   _  短い間   、。 長い間
*/
#include <MakoTalk.h>
#include <MakoTalkVoiceDefault.h>

constexpr int BCLK  = 5;
constexpr int LRCLK = 6;   // WS: I2Sの左右クロック。I2CのSDAではありません。
constexpr int DATA  = 4;

// 1文ごとに「速さ・高さ・音量」を決めて話す
void say(const char* kana, uint16_t speed, uint8_t pitch, uint8_t vol) {
  msetSpeed(speed);  msetPitch(pitch);  msetVolume(vol);
  if (!mspeak(kana)) Serial.println(mlastError());
}

// 優しい声: ふつうより少しゆっくり(115)・少し低く(94)・小さめ(60)
void sayGentle(const char* k) { say(k, 115, 94, 60); }

// 嬉しい声: 少し速く(138)・低めに設定(75)。「!」で自然に高くなるため、ふつうより下げて使う
void sayHappy(const char* k)  { say(k, 138, 75, 70); }

void setup() {
  Serial.begin(115200);
  if (!mbegin(MAKOTALK_VOICE_DEFAULT, BCLK, LRCLK, DATA, I2S_NUM_1)) {
    Serial.println(mlastError());
    return;
  }
  say("いか'がでしょうか",  130, 100, 70);   // ふつう
  sayGentle("いか'がでしょうか?");           // 優しい声
  say("こんにちわ",         130, 100, 70);   // ふつう
  sayHappy("こんにちわ!");                   // 嬉しい声
}

void loop() {}
