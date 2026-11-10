/*
 * ============================================================================
 *  MakoTalk for ESP32-S3
 *  音声記号列 → 波形 リアルタイム規則合成エンジン (ヘッダ)
 *
 *  File     : MakoTalkEngine.h
 *  Version  : 2.1.0 (原ヘッダ 1.1.0)
 *  Created  : 2026-08-01 18:30 JST
 *  Target   : ESP32-S3 (N16R8 想定 / PSRAM必須)
 *             原本は Arduino-ESP32 core 3.x (ESP_I2S) 前提。
 *             本ライブラリは core 3.3.12 で確認し、
 *             I2S出力はMakoTalkSpeakerの登録出力先がlegacy I2Sで行う。
 *  License  : 独自実装。AquesTalk等の既存製品のコード/データは一切不使用。
 *
 *  方式:
 *    CVモーラ素片接続 + TD-PSOLA によるピッチ/継続長制御。
 *    素片辞書は基準F0で平坦化済みのため、ピッチマーク位置は
 *    voiced_start + k * P0  (P0 = SR / F0_BASE) で自明に求まる。
 *    → ピッチマークテーブル不要、RAM/Flash ともに大幅節約。
 *
 *  音声記号列 (独自表記):
 *    カタカナ  : モーラ
 *    '         : 直前のモーラがアクセント核 (直後で下降)
 *    ー        : 長音 (直前モーラを1モーラ分延長)
 *    ッ        : 促音 (無音)
 *    _         : 短ポーズ + アクセント句境界
 *    、        : 長ポーズ + アクセント句境界
 *    ?         : 直前の句を上昇調にする
 *    !         : 直前の句を感嘆調にする(ピッチ変化幅を誇張。半角/全角どちらも可)
 *    例) "コンニチワ"  "オハヨ'ー"  "ゲンザイ_ジュ'ーニジ_ハンデス"  "ヤッタ'ー!"
 *
 *  ----------------------------------------------------------------------------
 *  変更履歴 (時系列順。v1.0.0〜v1.1.0 の個別記録は原本に無し)
 *    v0.1.0  2026-07-31 JST  初版。
 *    v0.2.0  2026-08-01 JST  可聴性の改善。
 *                            - 母音長の下限保証 (MKT_MIN_VOWEL_MS) を追加
 *                            - 既定モーラ長 112 → 150ms (早口の解消)
 *                            - ポーズ/促音長を微調整
 *    v0.4.0  2026-08-01 JST  破裂音の閉鎖長回復に合わせて標準モーラ長を
 *                            150→160msに変更(等時性の維持)。
 *
 *  配布版の変更 (原本との差分。詳細は NOTICE.md / PROVENANCE.md)
 *    2026-11-04 JST  setSpeed の引数・保持値を uint8_t→uint16_t、上限を 300 へ拡張。
 *                    従来の設定値での合成結果は変更なし。
 *    2026-11-07 JST  コメントのみ修正 (変更履歴の順序、Target 表記、
 *                    setPitch の範囲表記、core 2.x での begin()/speak()/play() の注記)。
 *    2026-11-07 JST  配布版 2.0.0: 著作情報付き辞書 (MKTV v3) と出力先登録。
 *                    - 公開の初期化は beginImage() のみ。辞書先頭の著作情報を検査し、
 *                      未告知 (Flash 消去後・辞書差し替え後) ならクレジットを自動発話。
 *                    - PCM 出力先 (MKT_Output) を登録し、speak()/play() をそれ経由で有効化。
 *                    - MKT_Unit / MKT_Voice 定義を MakoTalkTypes.h へ移動 (内容は同一)。
 *                    - synth() 開始時に lastError() を消去 (レビュー指摘 B-1)。
 *                    - 旧 begin()/beginBufferOnly() は内部用 (private) へ移動。
 * ============================================================================
 */
#pragma once

#include <Arduino.h>
#include "MakoTalkTypes.h"      // MKT_Unit / MKT_Voice (原本の定義をここから移動)
#include "MakoTalkVoiceInfo.h"  // 著作情報
#include "MakoTalkImage.h"      // MKTV v3 ローダー

// ---------------------------------------------------------------------------
//  PCM 出力先 (配布版 2.0.0)。機種依存の出力 (I2S 等) はアプリ側のこの関数に閉じる。
// ---------------------------------------------------------------------------
typedef struct {
  // 合成済み PCM (int16 mono, 辞書の sampleRate) を出力する。完了まで戻らないこと。
  bool (*write)(const int16_t* pcm, size_t samples, void* ctx);
  // 初期化のたびに著作情報を通知する (任意。表示機器への描画を推奨)。告知発話の前に呼ばれる。
  void (*onCredit)(const MKT_VoiceInfo& info, void* ctx);
  void* ctx;
} MKT_Output;

// ---------------------------------------------------------------------------
//  韻律パラメータの既定値
//
//  v0.2 の変更:
//    v0.1 は「1モーラ=112ms から子音長を引く」方式だったため、
//    ツ(子音100ms)は母音が12msしか残らず発話に聞こえなかった。
//    v0.2 は母音長の下限 MKT_MIN_VOWEL_MS を保証し、
//    さらに既定モーラ長を 112→150ms に伸ばして早口を解消する。
// ---------------------------------------------------------------------------
#define MKT_MORA_MS_DEFAULT   160   // 1モーラの標準長 [ms]  (v0.1:112 / v0.2:150)
                                    // v0.4: 破裂音の閉鎖を50msに戻した結果、
                                    // 最長子音84ms+母音下限75ms=159ms が必要になった。
                                    // 160にすることで全モーラが等長(等時性)を保つ。
#define MKT_MIN_VOWEL_MS       75   // 母音部の最低長 [ms]   (v0.2で追加)
#define MKT_PAUSE_SHORT_MS    150   // '_'
#define MKT_PAUSE_LONG_MS     330   // '、'
#define MKT_SOKUON_MS          95   // 'ッ'
#define MKT_ACCENT_RATIO     1.28f  // 高ピッチ / 低ピッチ の比
#define MKT_DECLINATION      0.10f  // 句内での自然下降量
#define MKT_EXCLAIM_ACCENT_RATIO  2.10f  // 感嘆(!)時の高/低ピッチ比(誇張)
#define MKT_EXCLAIM_DECLINATION   0.02f  // 感嘆時は句内下降をほぼ無くす(勢いを保つ)
#define MKT_EXCLAIM_RISING_RATIO  1.85f  // 感嘆時、語尾を跳ね上げる倍率(?が無くても適用)
#define MKT_EXCLAIM_DUR_SCALE     0.85f  // 感嘆時はテンポを速める(モーラ長を短縮)
#define MKT_MAX_MORAE         192   // 1発話あたりの最大モーラ数

// ---------------------------------------------------------------------------
//  合成エンジン
// ---------------------------------------------------------------------------
class MakoTalk {
 public:
  MakoTalk();
  ~MakoTalk();

  // 著作情報付き辞書 (MKTV v3) で初期化する。配布版で唯一の公開初期化。
  //   1) 辞書イメージを検査 (CRC32・著作情報の必須項目)。不正なら false。
  //   2) 作業バッファを確保 (maxSeconds 分、PSRAM 優先)。
  //   3) out.onCredit があれば著作情報を通知 (毎回)。
  //   4) 未告知 (Flash 消去後・辞書差し替え後) ならクレジットを out.write へ発話し、
  //      告知済みフラグを保存 (port_store.h)。
  // image は辞書を使う間ずっと有効であること (Flash 上の定数配列など)。
  bool beginImage(const uint8_t* image, size_t imageLen, const MKT_Output& out,
                  float maxSeconds = 8.0f);

  void end();

  // クレジットを強制的に発話する (「この音声について」等のメニュー用)。
  bool announceCredit();
  const MKT_VoiceInfo& voiceInfo() const { return _loaded.info; }

  // 音声記号列を合成して出力先へ再生 (完了までブロックする)。
  bool speak(const char* symbols);

  // 合成のみ。out に内部バッファ(int16 mono)を返す。次の合成まで有効。
  bool synth(const char* symbols, const int16_t** out, size_t* outSamples);

  // 生成済みバッファを出力先へ流す (登録が無く core 3.x の I2S が有効なら I2S へ)
  bool play(const int16_t* pcm, size_t samples);

  // --- パラメータ (いずれも 100 が標準) ---
  void setSpeed(uint16_t v);  //  40..300 大きいほど速い（配布版拡張）
  void setPitch(uint8_t v);   //  40..250  大きいほど高い (実装の constrain 範囲)
  void setVolume(uint8_t v);  //   0..100

  uint32_t lastSynthMicros() const { return _lastMicros; }
  const char* lastError() const { return _err; }

  // --- リップシンク用: 直前の synth()/speak() 結果のモーラ単位タイムライン ---
  //     モーラiの音声は出力PCM中の [visemeStartSample(i), +visemeDurSamples(i)) に対応する。
  int      visemeCount() const { return _nMorae; }
  uint8_t  visemeVowel(int i) const;        // 0=a 1=i 2=u 3=e 4=o 5=ん 6=無音(ポーズ/促音)
  uint32_t visemeStartSample(int i) const;
  uint32_t visemeDurSamples(int i) const { return (i >= 0 && i < _nMorae) ? _morae[i].durSmp : 0; }

 private:
  // 原本の初期化 (配布版では内部用。著作情報を経ない利用を防ぐため private)。
  // begin() は core 3.x の ESP_I2S 前提。本配布版 (core 2.x) ではスタブにより常に false。
  bool begin(const MKT_Voice* voice,
             int pinBclk, int pinLrck, int pinDout,
             float maxSeconds = 8.0f);
  bool beginBufferOnly(const MKT_Voice* voice, float maxSeconds = 8.0f);

  MKT_LoadedVoice  _loaded   = {};
  const char**     _kanaBuf  = nullptr;
  MKT_Output       _outDev   = {nullptr, nullptr, nullptr};

  struct Mora {
    int16_t  unitIdx;   // -1 = 無音
    uint16_t durSmp;    // 出力継続長 [サンプル]
    float    f0Start;
    float    f0End;
  };

  bool  parse(const char* symbols);
  void  applyProsody(int phraseStart, int phraseEnd, int accentPos, bool rising, bool exclaim);
  int   matchKana(const char* p, int* byteLen) const;
  void  renderMora(const Mora& m, int32_t outPos);
  bool  allocBuffers(float maxSeconds);

  const MKT_Voice* _v        = nullptr;
  float*           _work     = nullptr;   // 合成用 float バッファ (PSRAM)
  int16_t*         _out      = nullptr;   // 出力 int16 バッファ (PSRAM)
  size_t           _cap      = 0;         // バッファ容量 [サンプル]
  size_t           _outLen   = 0;

  Mora             _morae[MKT_MAX_MORAE];
  int              _nMorae   = 0;

  uint16_t         _speed    = 100;
  uint8_t          _pitch    = 100;
  uint8_t          _volume   = 80;

  int              _p0       = 128;       // 基準ピッチ周期 [サンプル]
  int              _vowelUnit[6] = {-1, -1, -1, -1, -1, -1};  // ア/イ/ウ/エ/オ/ン の素片番号
  bool             _i2sReady = false;
  uint32_t         _lastMicros = 0;
  const char*      _err      = "";
};
