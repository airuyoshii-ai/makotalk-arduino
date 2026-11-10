// SPDX-License-Identifier: CC-BY-4.0
// ============================================================================
//  MakoTalkImage.h — 著作情報付き音声辞書イメージ (MKTV v3) のローダー
//
//  Version : 1.0.0
//  Created : 2026-11-07 17:40 JST
//
//  辞書の先頭区画に著作情報 (クレジット・告知文・規約) を持たせ、辞書と
//  クレジットを1つのデータにする。ローダーは著作情報を検査できない辞書を
//  受け付けない。Flash 組み込み (voice_<id>_image.h) と、SD/LittleFS 上の
//  .mktv ファイルを、同じ形式・同じ処理で扱う。ハード非依存 (PC でも動作)。
//
//  ── イメージ形式 MKTV v3 (リトルエンディアン、先頭は 4 バイト境界) ──
//   off size 内容
//    0   4   "MKTV"
//    4   2   版 = 3
//    6   2   flags (MKT_VFLAG_*)
//    8   4   イメージ全長 [バイト]
//   12   4   CRC32 (zlib 互換。このフィールドを 0 とみなしてイメージ全体を計算)
//   16   4/4 著作区画 オフセット / 長さ
//   24   4/4 音声区画 オフセット / 長さ
//   32   4/4 PCM オフセット (4 境界) / サンプル数
//   40   8   予約 (0)
//
//   著作区画: 項目の列。各項目 = tag(1) 予約(1) len(2) data(len)、次項目は 2 境界。
//     0x01 音声ID           文字列
//     0x02 表示クレジット   文字列 (必須)
//     0x03 ASCII表記        文字列
//     0x04 告知発話         speed(2) + 文字列 (必須)
//     0x05 権利者           文字列
//     0x06 規約・著作文書   規約URL 文字列 + 著作文書パス 文字列
//     0x07 クレジット画像   w(2) h(2) 1bpp ((w+7)/8*h バイト)
//     0x00 終端
//     文字列は UTF-8・NUL 終端 (NUL を len に含む)。
//
//   音声区画 (4 境界): unitCount(2) sampleRate(2) f0Base(2) 予約(2)
//     素片表 unitCount × 12 バイト (MKT_Unit と同一レイアウト)
//     カナ表 unitCount × [len(1) + 文字列 + NUL]
//
//  CRC32 が防ぐのは偶発的な欠落・改変であり、意図的な改ざん (CRC 再計算) は
//  防げない。著作情報を正規の使い方で必ず告知させるための仕組みである。
// ============================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "MakoTalkTypes.h"
#include "MakoTalkVoiceInfo.h"

#define MKT_IMAGE_VERSION    3
#define MKT_IMAGE_HEADER     48
#define MKT_IMAGE_MAX_UNITS  512

typedef struct {
  MKT_Voice     voice;   // 合成エンジンへ渡す辞書 (イメージ内を直接指す)
  MKT_VoiceInfo info;    // 著作情報 (イメージ内を直接指す)
} MKT_LoadedVoice;

// zlib 互換 CRC32。crc に前回値を渡すと続きから計算する。
uint32_t mktCrc32(const uint8_t* data, size_t len, uint32_t crc = 0);

// イメージを検査して out を埋める。kanaTable は素片数以上の要素を持つ領域
// (呼び出し側が確保し、辞書を使う間は保持する)。
// 成功時 nullptr、失敗時はエラー文字列 (UTF-8) を返す。
const char* mktLoadImage(const uint8_t* image, size_t len,
                         MKT_LoadedVoice* out,
                         const char** kanaTable, uint16_t kanaCap);
