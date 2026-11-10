// SPDX-License-Identifier: CC-BY-4.0
// ============================================================================
//  MakoTalkVoiceInfo.h — 音声辞書ごとの著作・クレジット情報
//
//  Version : 2.0.0
//  Created : 2026-11-07 16:10 JST
//  Updated : 2026-11-07 17:40 JST
//
//  変更履歴
//    1.0.0  2026-11-07  初版。クレジットを別ヘッダー (voice_<id>_info.h) で保持。
//    2.0.0  2026-11-07  クレジットを辞書イメージ (MKTV v3) の先頭区画へ移動。
//                       本構造体はローダー (MakoTalkImage) が辞書から復元する。
//
//  ハード非依存。表示機器・I2S には触れない。
//  文字列はすべて UTF-8・NUL 終端で、辞書イメージ内を直接指す (コピーしない)。
//  creditBitmap は 1bpp、行優先、1行 = (bitmapWidth+7)/8 バイト、MSB が左端。
//  Adafruit_GFX::drawBitmap() など一般的な 1bpp 描画関数でそのまま描ける。
// ============================================================================
#pragma once
#include <stdint.h>

#define MKT_VFLAG_NO_STANDALONE  0x0001  // 辞書を単体で配布・販売してはならない

typedef struct {
  const char*    voiceId;        // 0x01 例: "default"
  const char*    creditDisplay;  // 0x02 表示用クレジット。例: "VOICEVOX:ずんだもん" (必須)
  const char*    creditAscii;    // 0x03 日本語を描けない機器・シリアル向け
  const char*    creditSpeech;   // 0x04 告知の発話文 (MakoTalk 記法、必須)
  uint16_t       speechSpeed;    // 0x04 告知の発話速度 (setSpeed 値)
  const char*    rightsHolder;   // 0x05 権利者
  const char*    termsUrl;       // 0x06 主たる公式規約 URL
  const char*    noticePath;     // 0x06 リポジトリ内の著作文書
  const uint8_t* creditBitmap;   // 0x07 creditDisplay を描画した 1bpp ビットマップ
  uint16_t       bitmapWidth;
  uint16_t       bitmapHeight;
  uint16_t       flags;          // MKT_VFLAG_*
  uint32_t       imageCrc32;     // 辞書イメージ全体の CRC32 (告知済みフラグの照合に使用)
} MKT_VoiceInfo;
