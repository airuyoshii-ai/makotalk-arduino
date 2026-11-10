// SPDX-License-Identifier: CC-BY-4.0
// ============================================================================
//  MakoTalkImage.cpp — MKTV v3 辞書イメージのローダー (ハード非依存)
//
//  Version : 1.0.0
//  Created : 2026-11-07 17:40 JST
//
//  形式の定義は MakoTalkImage.h を参照。Arduino.h に依存しない。
// ============================================================================
#include "MakoTalkImage.h"
#include <string.h>

static_assert(sizeof(MKT_Unit) == 12, "MKT_Unit layout must be 12 bytes (MKTV v3)");

// --- CRC32 (zlib 互換、4bit テーブル: 64 バイトで 1.2MB を数十 ms) ---------
uint32_t mktCrc32(const uint8_t* p, size_t n, uint32_t crc) {
  static const uint32_t T[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
    0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
    0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    crc = (crc >> 4) ^ T[crc & 15];
    crc = (crc >> 4) ^ T[crc & 15];
  }
  return ~crc;
}

static uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

// 区画 [off, off+len) がイメージ内に収まるか
static bool inside(uint32_t off, uint32_t len, size_t total) {
  return off <= total && len <= total - off;
}

// data[0..len) が NUL 終端の文字列か (NUL が len 内にある)
static const char* asString(const uint8_t* d, uint16_t len) {
  if (len == 0 || d[len - 1] != 0) return nullptr;
  return (const char*)d;
}

const char* mktLoadImage(const uint8_t* img, size_t len, MKT_LoadedVoice* out,
                         const char** kanaTable, uint16_t kanaCap) {
  if (!img || !out || !kanaTable) return "引数が不正です";
  if (((uintptr_t)img & 3) != 0) return "辞書イメージが4バイト境界にありません";
  if (len < MKT_IMAGE_HEADER || memcmp(img, "MKTV", 4) != 0) return "MKTV 辞書ではありません";
  if (rd16(img + 4) != MKT_IMAGE_VERSION) return "MKTV v3 ではありません (著作情報の無い旧形式は使用できません)";
  if (rd32(img + 8) != len) return "辞書イメージの長さが一致しません";

  // CRC: フィールド (12..15) を 0 とみなして全体を計算
  static const uint8_t zero4[4] = {0, 0, 0, 0};
  uint32_t crc = mktCrc32(img, 12);
  crc = mktCrc32(zero4, 4, crc);
  crc = mktCrc32(img + 16, len - 16, crc);
  if (crc != rd32(img + 12)) return "辞書イメージの CRC32 が一致しません (欠落または改変)";

  const uint32_t cOff = rd32(img + 16), cLen = rd32(img + 20);
  const uint32_t vOff = rd32(img + 24), vLen = rd32(img + 28);
  const uint32_t pOff = rd32(img + 32), pCnt = rd32(img + 36);
  if (!inside(cOff, cLen, len) || !inside(vOff, vLen, len) || (pOff & 3) || (vOff & 3) ||
      pCnt > (len - pOff) / 2 || !inside(pOff, pCnt * 2, len))
    return "辞書イメージの区画が不正です";

  // --- 著作区画 ---
  MKT_VoiceInfo& in = out->info;
  memset(&in, 0, sizeof(in));
  in.flags = rd16(img + 6);
  in.imageCrc32 = crc;
  for (uint32_t p = cOff; p + 4 <= cOff + cLen;) {
    const uint8_t tag = img[p];
    const uint16_t l = rd16(img + p + 2);
    const uint8_t* d = img + p + 4;
    if (tag == 0x00) break;
    if (p + 4 + l > cOff + cLen) return "著作区画の項目長が不正です";
    switch (tag) {
      case 0x01: in.voiceId = asString(d, l); break;
      case 0x02: in.creditDisplay = asString(d, l); break;
      case 0x03: in.creditAscii = asString(d, l); break;
      case 0x04: if (l > 2) { in.speechSpeed = rd16(d); in.creditSpeech = asString(d + 2, l - 2); } break;
      case 0x05: in.rightsHolder = asString(d, l); break;
      case 0x06: {
        in.termsUrl = asString(d, l) ? (const char*)d : nullptr;
        if (in.termsUrl) {
          const size_t a = strlen(in.termsUrl) + 1;
          if (a < l) in.noticePath = asString(d + a, (uint16_t)(l - a));
        }
      } break;
      case 0x07: if (l >= 4) {
        const uint16_t w = rd16(d), h = rd16(d + 2);
        if ((uint32_t)((w + 7) / 8) * h == (uint32_t)(l - 4)) {
          in.bitmapWidth = w; in.bitmapHeight = h; in.creditBitmap = d + 4;
        }
      } break;
      default: break;  // 未知の項目は読み飛ばす (将来の拡張用)
    }
    p += 4 + l;
    p = (p + 1) & ~1u;
  }
  if (!in.creditDisplay || !in.creditDisplay[0]) return "著作情報 (表示クレジット) がありません";
  if (!in.creditSpeech || !in.creditSpeech[0] || in.speechSpeed == 0) return "著作情報 (告知発話) がありません";
  if (!in.creditAscii) in.creditAscii = in.creditDisplay;

  // --- 音声区画 ---
  if (vLen < 8) return "音声区画が不正です";
  const uint8_t* v = img + vOff;
  const uint16_t n = rd16(v), sr = rd16(v + 2), f0 = rd16(v + 4);
  if (n == 0 || n > MKT_IMAGE_MAX_UNITS || n > kanaCap) return "素片数が不正です";
  if (f0 == 0 || sr % f0) return "サンプリング周波数と基準F0が不正です";
  if (8u + 12u * n > vLen) return "素片表が区画を超えています";
  const MKT_Unit* units = (const MKT_Unit*)(v + 8);
  for (uint16_t i = 0; i < n; i++) {
    const MKT_Unit& u = units[i];
    if (!u.length || u.offset + u.length > pCnt || u.voicedStart > u.length || u.vowel > 5)
      return "素片表の値が不正です";
  }
  uint32_t k = 8u + 12u * n;
  for (uint16_t i = 0; i < n; i++) {
    if (k >= vLen) return "カナ表が区画を超えています";
    const uint8_t l = v[k];
    if (l == 0 || k + 1 + l + 1 > vLen || v[k + 1 + l] != 0) return "カナ表が不正です";
    kanaTable[i] = (const char*)(v + k + 1);
    k += 1 + l + 1;
  }

  MKT_Voice& voice = out->voice;
  voice.pcm = (const int16_t*)(img + pOff);
  voice.pcmLen = pCnt;
  voice.units = units;
  voice.kana = kanaTable;
  voice.unitCount = n;
  voice.sampleRate = sr;
  voice.f0Base = f0;
  return nullptr;
}
