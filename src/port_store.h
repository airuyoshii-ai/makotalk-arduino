// SPDX-License-Identifier: CC-BY-4.0
// ============================================================================
//  port_store.h — 「クレジット告知済み」フラグの保存 (機種依存部)
//
//  Version : 1.0.0
//  Created : 2026-11-07 17:40 JST
//
//  告知済みの辞書の CRC32 を不揮発領域へ1つだけ保存する。
//    ・フラグなし (Flash 消去後の初回起動など)        → 告知する
//    ・保存値 != 現在の辞書の CRC32 (音声を差し替えた) → 告知する
//    ・保存値 == 現在の辞書の CRC32                    → 告知しない
//  ESP32 系: Preferences (NVS) の名前空間 "mktcredit"、キー "crc"。
//  NVS を消去すると (esptool erase_flash 等) 次回起動で再び告知する。
//  通常の書き込み (pio upload) は NVS を消去しないため、同じ辞書なら告知しない。
//  他機種へ移植する場合は2関数だけを差し替える (保存手段の無い機種は毎回告知)。
// ============================================================================
#pragma once
#include <stdint.h>

#if defined(ESP_PLATFORM) && defined(ARDUINO)
#include <Preferences.h>
static inline bool mktCreditAnnounced(uint32_t crc) {
  Preferences p;
  if (!p.begin("mktcredit", true)) return false;   // 名前空間なし = 未告知
  const bool done = p.isKey("crc") && p.getUInt("crc", 0) == crc;
  p.end();
  return done;
}
static inline void mktMarkCreditAnnounced(uint32_t crc) {
  Preferences p;
  if (!p.begin("mktcredit", false)) return;
  p.putUInt("crc", crc);
  p.end();
}
#else
static inline bool mktCreditAnnounced(uint32_t) { return false; }
static inline void mktMarkCreditAnnounced(uint32_t) {}
#endif
