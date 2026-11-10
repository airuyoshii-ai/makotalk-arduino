// ============================================================================
//  MakoTalkTypes.h — 素片辞書のデータ構造 (ハード非依存)
//
//  Version : 1.0.0
//  Created : 2026-11-07 17:40 JST
//
//  原本 MakoTalk.h の MKT_Unit / MKT_Voice 定義を、Arduino.h に依存しない
//  このファイルへ移した (配布版の変更。定義内容は原本と同一)。
//  ローダー (MakoTalkImage) と PC 上の試験から参照するため。
// ============================================================================
#pragma once
#include <stdint.h>

typedef struct {
  uint32_t offset;       // MKT_Voice::pcm 内の開始サンプル位置
  uint16_t length;       // 素片の全サンプル数
  uint16_t voicedStart;  // 有声区間の開始位置 (素片内相対サンプル)
  uint8_t  vowel;        // 0=a 1=i 2=u 3=e 4=o 5=N
  uint8_t  flags;        // 予約
} MKT_Unit;

typedef struct {
  const int16_t*      pcm;
  uint32_t            pcmLen;
  const MKT_Unit*     units;
  const char* const*  kana;       // units と同順のカタカナ表記 (UTF-8)
  uint16_t            unitCount;
  uint16_t            sampleRate;
  uint16_t            f0Base;
} MKT_Voice;
