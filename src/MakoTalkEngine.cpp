/*
 * ============================================================================
 *  MakoTalk for ESP32-S3
 *  音声記号列 → 波形 リアルタイム規則合成エンジン (実装)
 *
 *  File     : MakoTalkEngine.cpp
 *  Version  : 2.1.0 (原実装 0.2.0)
 *  Created  : 2026-08-01 09:40 JST
 *  Target   : ESP32-S3 (PSRAM必須)
 *             原本の方式を継承し、本ライブラリは Arduino-ESP32 core 3.3.12 で確認。
 *             旧内部I2Sはスタブ。play()/speak() は登録したMKT_Outputから出力する。
 *
 *  ----------------------------------------------------------------------------
 *  変更履歴
 *    v0.2.0  2026-08-01 JST  可聴性の改善。
 *                            - モーラ長を「子音長+母音下限」で下支え
 *                              (v0.1はツで母音が12msしか残らなかった)
 *                            - 出力ゲインを辞書の新しい正規化に合わせて調整
 *    v0.1.0  2026-07-31 JST  初版。
 *                            - UTF-8カタカナ最長一致パーサ
 *                            - 東京式アクセント句のF0コンター生成
 *                            - TD-PSOLA (固定ピッチマーク前提の軽量版)
 *                            - 長音の定常部ループ、促音/ポーズ処理
 *
 *  配布版の変更 (原本との差分。詳細は NOTICE.md / PROVENANCE.md)
 *    2026-11-04 JST  setSpeed を uint16_t・40..300 へ拡張 (従来範囲の結果は不変)。
 *    2026-11-07 JST  コメントのみ修正 (Target 表記、lastError() の挙動注記)。
 *    2026-11-07 JST  配布版 2.0.0: beginImage() (MKTV v3 検査・クレジット自動告知)、
 *                    出力先登録による play()/speak()、announceCredit()、
 *                    synth() 開始時の lastError() 消去 (B-1 修正)。合成アルゴリズムは不変。
 * ============================================================================
 */

#include "MakoTalkEngine.h"
#include "port_store.h"
#include "MakoTalkLegacyShim.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

static MKT_InactiveI2S s_i2s;
static float*   s_hann     = nullptr;   // ハン窓テーブル (長さ 2*P0)
static int      s_hannSize = 0;

// ---------------------------------------------------------------------------
MakoTalk::MakoTalk() {}

MakoTalk::~MakoTalk() { end(); }

// ---------------------------------------------------------------------------
bool MakoTalk::allocBuffers(float maxSeconds) {
  _cap = (size_t)(maxSeconds * _v->sampleRate);

  _work = (float*)heap_caps_malloc(_cap * sizeof(float), MALLOC_CAP_SPIRAM);
  if (!_work) _work = (float*)malloc(_cap * sizeof(float));

  _out = (int16_t*)heap_caps_malloc(_cap * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  if (!_out) _out = (int16_t*)malloc(_cap * sizeof(int16_t));

  if (!_work || !_out) {
    _err = "作業バッファの確保に失敗 (PSRAMを有効にしてください)";
    return false;
  }

  // ハン窓テーブル (幅 2*P0)
  if (s_hann) { free(s_hann); s_hann = nullptr; }
  s_hannSize = _p0 * 2;
  s_hann = (float*)malloc(s_hannSize * sizeof(float));
  if (!s_hann) { _err = "窓テーブルの確保に失敗"; return false; }
  for (int i = 0; i < s_hannSize; i++) {
    s_hann[i] = 0.5f * (1.0f - cosf((float)i * 2.0f * (float)M_PI / (float)s_hannSize));
  }
  return true;
}

// ---------------------------------------------------------------------------
bool MakoTalk::beginBufferOnly(const MKT_Voice* voice, float maxSeconds) {
  if (!voice || !voice->pcm || voice->unitCount == 0) {
    _err = "素片辞書が不正です";
    return false;
  }
  _v = voice;

  if (_v->f0Base == 0 || (_v->sampleRate % _v->f0Base) != 0) {
    // P0 が整数にならないと固定ピッチマーク前提が崩れる
    _err = "sampleRate は f0Base の整数倍である必要があります";
    return false;
  }
  _p0 = _v->sampleRate / _v->f0Base;

  // 基本母音の素片番号を解決 (長音・母音連続で使用)
  static const char* kBase[6] = {"ア", "イ", "ウ", "エ", "オ", "ン"};
  for (int v = 0; v < 6; v++) {
    _vowelUnit[v] = -1;
    for (int i = 0; i < _v->unitCount; i++) {
      if (strcmp(_v->kana[i], kBase[v]) == 0) { _vowelUnit[v] = i; break; }
    }
  }
  return allocBuffers(maxSeconds);
}

// ---------------------------------------------------------------------------
bool MakoTalk::begin(const MKT_Voice* voice,
                          int pinBclk, int pinLrck, int pinDout,
                          float maxSeconds) {
  if (!beginBufferOnly(voice, maxSeconds)) return false;

  s_i2s.setPins(pinBclk, pinLrck, pinDout, -1, -1);
  if (!s_i2s.begin(I2S_MODE_STD, _v->sampleRate,
                   I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    _err = "I2Sの初期化に失敗";
    return false;
  }
  _i2sReady = true;
  return true;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  配布版 2.0.0: 著作情報付き辞書での初期化とクレジット告知
// ---------------------------------------------------------------------------
bool MakoTalk::beginImage(const uint8_t* image, size_t imageLen, const MKT_Output& out,
                          float maxSeconds) {
  if (!out.write) { _err = "出力先 (MKT_Output::write) が未登録です"; return false; }
  if (!_kanaBuf) _kanaBuf = (const char**)malloc(sizeof(const char*) * MKT_IMAGE_MAX_UNITS);
  if (!_kanaBuf) { _err = "カナ表の確保に失敗"; return false; }
  const char* e = mktLoadImage(image, imageLen, &_loaded, _kanaBuf, MKT_IMAGE_MAX_UNITS);
  if (e) { _err = e; return false; }
  _outDev = out;
  if (!beginBufferOnly(&_loaded.voice, maxSeconds)) return false;
  if (_outDev.onCredit) _outDev.onCredit(_loaded.info, _outDev.ctx);
  if (!mktCreditAnnounced(_loaded.info.imageCrc32)) {
    if (!announceCredit()) return false;
    mktMarkCreditAnnounced(_loaded.info.imageCrc32);
  }
  _err = "";
  return true;
}

bool MakoTalk::announceCredit() {
  if (!_loaded.info.creditSpeech) { _err = "著作情報がありません (beginImage が未実行)"; return false; }
  const uint16_t sp = _speed; const uint8_t pt = _pitch;   // 利用者の設定を退避
  _speed = constrain(_loaded.info.speechSpeed, 40, 300);
  _pitch = 100;
  const bool ok = speak(_loaded.info.creditSpeech);
  _speed = sp; _pitch = pt;
  return ok;
}

void MakoTalk::end() {
  if (_i2sReady) { s_i2s.end(); _i2sReady = false; }
  if (_work) { free(_work); _work = nullptr; }
  if (_out)  { free(_out);  _out  = nullptr; }
  if (s_hann){ free(s_hann); s_hann = nullptr; }
  if (_kanaBuf) { free(_kanaBuf); _kanaBuf = nullptr; }
  _cap = 0;
}

void MakoTalk::setSpeed(uint16_t v) { _speed = constrain(v, 40, 300); }
void MakoTalk::setPitch(uint8_t v)  { _pitch  = constrain(v, 40, 250); }
void MakoTalk::setVolume(uint8_t v) { _volume = constrain(v, 0, 100); }

// ---------------------------------------------------------------------------
//  カタカナ最長一致
// ---------------------------------------------------------------------------
int MakoTalk::matchKana(const char* p, int* byteLen) const {
  int best = -1, bestLen = 0;
  for (int i = 0; i < _v->unitCount; i++) {
    const char* k = _v->kana[i];
    int l = (int)strlen(k);
    if (l > bestLen && strncmp(p, k, l) == 0) { best = i; bestLen = l; }
  }
  *byteLen = bestLen;
  return best;
}

// ---------------------------------------------------------------------------
//  東京式アクセントのF0コンター生成
//    acc = 0        : 平板型   (1モーラ目のみ低)
//    acc = 1        : 頭高型   (1モーラ目のみ高)
//    acc = 2..n     : 中高/尾高 (2モーラ目〜accモーラ目が高)
// ---------------------------------------------------------------------------
void MakoTalk::applyProsody(int s, int e, int acc, bool rising, bool exclaim) {
  const int n = e - s;
  if (n <= 0) return;

  const float base = (float)_v->f0Base * ((float)_pitch / 100.0f);
  const float accentRatio = exclaim ? MKT_EXCLAIM_ACCENT_RATIO : MKT_ACCENT_RATIO;
  const float declination = exclaim ? MKT_EXCLAIM_DECLINATION  : MKT_DECLINATION;

  auto level = [&](int i) -> float {
    bool high;
    if (acc == 0)      high = (i > 0);
    else if (acc == 1) high = (i == 0);
    else               high = (i > 0 && i < acc);
    float f = base * (high ? accentRatio : 1.0f);
    // 句内の自然下降(感嘆時は勢いを保つため下降を弱める)
    f *= (1.0f - declination * (float)i / (float)n);
    return f;
  };

  for (int i = 0; i < n; i++) {
    const float a = level(i);
    const float b = (i + 1 < n) ? 0.5f * (a + level(i + 1)) : a * 0.94f;
    _morae[s + i].f0Start = a;
    _morae[s + i].f0End   = b;
  }
  // 感嘆時は '?' が無くても語尾を跳ね上げる(「やったー!」らしい抑揚の要)
  if (rising || exclaim) {
    const float r = exclaim ? MKT_EXCLAIM_RISING_RATIO : 1.45f;
    _morae[e - 1].f0End = _morae[e - 1].f0Start * r;
  }
  // 感嘆時はテンポを上げて勢いを出す(モーラ長を短縮)
  if (exclaim) {
    for (int i = 0; i < n; i++) {
      _morae[s + i].durSmp = (uint16_t)(_morae[s + i].durSmp * MKT_EXCLAIM_DUR_SCALE);
    }
  }
}

// ---------------------------------------------------------------------------
//  音声記号列パーサ
// ---------------------------------------------------------------------------
bool MakoTalk::parse(const char* s) {
  _nMorae = 0;
  int  phraseStart = 0;
  int  accentPos   = 0;
  bool rising      = false;
  bool exclaim     = false;
  int  prevVowel   = -1;

  const float spd     = 100.0f / (float)_speed;
  const int   moraSmp = (int)(MKT_MORA_MS_DEFAULT * spd * _v->sampleRate / 1000.0f);
  const int   sokuSmp = (int)(MKT_SOKUON_MS      * spd * _v->sampleRate / 1000.0f);
  const int   minVowSmp = (int)(MKT_MIN_VOWEL_MS * spd * _v->sampleRate / 1000.0f);

  // v0.2: モーラ長は「標準長」と「子音長+母音下限」の大きい方にする。
  //       v0.1 は標準長から子音長を引いていたため、摩擦音や破擦音では
  //       母音がほぼ消滅していた（ツで12ms）。
  auto moraDuration = [&](int unitIdx) -> int {
    if (unitIdx < 0) return moraSmp;
    const int head = (int)_v->units[unitIdx].voicedStart;
    const int need = head + minVowSmp;
    return (need > moraSmp) ? need : moraSmp;
  };

  auto push = [&](int unitIdx, int dur) {
    if (_nMorae >= MKT_MAX_MORAE) return;
    _morae[_nMorae].unitIdx = (int16_t)unitIdx;
    _morae[_nMorae].durSmp  = (uint16_t)dur;
    _morae[_nMorae].f0Start = (float)_v->f0Base;
    _morae[_nMorae].f0End   = (float)_v->f0Base;
    _nMorae++;
  };

  const char* p = s;
  while (*p) {
    const unsigned char c = (unsigned char)*p;

    // --- ASCII制御記号 ---
    if (c == '\'') { accentPos = _nMorae - phraseStart; p++; continue; }
    if (c == '?')  { rising = true; p++; continue; }
    if (c == '!')  { exclaim = true; p++; continue; }
    if (strncmp(p, "！", 3) == 0) { exclaim = true; p += 3; continue; }  // 全角も可
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { p++; continue; }

    // --- 句境界 ---
    int pauseMs = 0;
    if (c == '_')                       { pauseMs = MKT_PAUSE_SHORT_MS; p += 1; }
    else if (strncmp(p, "、", 3) == 0)  { pauseMs = MKT_PAUSE_LONG_MS;  p += 3; }
    else if (strncmp(p, "。", 3) == 0)  { pauseMs = MKT_PAUSE_LONG_MS;  p += 3; }
    if (pauseMs) {
      applyProsody(phraseStart, _nMorae, accentPos, rising, exclaim);
      push(-1, (int)(pauseMs * spd * _v->sampleRate / 1000.0f));
      phraseStart = _nMorae;
      accentPos = 0; rising = false; exclaim = false; prevVowel = -1;
      continue;
    }

    // --- 促音 (1モーラとして数える) ---
    if (strncmp(p, "ッ", 3) == 0) { push(-1, sokuSmp); prevVowel = -1; p += 3; continue; }

    // --- 長音 (直前モーラの母音を1モーラ分継続) ---
    if (strncmp(p, "ー", 3) == 0) {
      { const int lu = (prevVowel >= 0) ? _vowelUnit[prevVowel] : -1;
        push(lu, moraDuration(lu)); }
      p += 3;
      continue;
    }

    // --- カタカナ ---
    int bl = 0;
    const int ui = matchKana(p, &bl);
    if (ui < 0) {                       // 未知文字はスキップ
      p += (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
      continue;
    }
    push(ui, moraDuration(ui));
    prevVowel = _v->units[ui].vowel;
    p += bl;
  }

  applyProsody(phraseStart, _nMorae, accentPos, rising, exclaim);
  return _nMorae > 0;
}

// ---------------------------------------------------------------------------
//  1モーラのレンダリング (TD-PSOLA)
//    素片は基準F0で平坦化済みなので、ピッチマークは
//      voicedStart + k * P0
//    に必ず存在する。よってマークテーブルは不要。
// ---------------------------------------------------------------------------
void MakoTalk::renderMora(const Mora& m, int32_t outPos) {
  if (m.unitIdx < 0) return;                       // 無音

  const MKT_Unit& u = _v->units[m.unitIdx];
  const int16_t*  src = _v->pcm + u.offset;
  const int       P0  = _p0;
  const int32_t   cap = (int32_t)_cap;

  // ---- 無声先行部はそのまま加算 (短いフェードインでクリック回避) ----
  int head = (int)u.voicedStart;
  if (head > (int)m.durSmp) head = (int)m.durSmp;
  const int fade = 32;
  for (int i = 0; i < head; i++) {
    const int32_t o = outPos + i;
    if (o < 0 || o >= cap) continue;
    const float g = (i < fade) ? (float)i / (float)fade : 1.0f;
    _work[o] += (float)src[i] * g;
  }

  // ---- 有声部 ----
  const int srcVoiced = (int)u.length - (int)u.voicedStart;
  const int nMarks    = srcVoiced / P0;
  if (nMarks < 2) return;

  const int loopK  = (nMarks < 8) ? nMarks : 8;     // 長音時にループさせる周期数
  const int outLen = (int)m.durSmp - head;
  if (outLen <= 0) return;

  float ts = (float)outLen / (float)srcVoiced;      // 時間伸縮率
  if (ts < 0.05f) ts = 0.05f;

  const int32_t base = outPos + head;
  float outT = 0.0f;

  while (outT < (float)outLen) {
    const float r  = outT / (float)outLen;
    float f0 = m.f0Start + (m.f0End - m.f0Start) * r;
    if (f0 < 50.0f)  f0 = 50.0f;
    if (f0 > 500.0f) f0 = 500.0f;
    const float Pt = (float)_v->sampleRate / f0;

    // 対応する素片側ピッチマーク
    int mi = (int)(outT / ts / (float)P0 + 0.5f);
    if (mi >= nMarks) mi = nMarks - loopK + ((mi - nMarks) % loopK);
    const int center = (int)u.voicedStart + mi * P0;

    // 窓長を変えずに配置間隔だけ変えるので、振幅は Pt/P0 で補正する
    const float gain = Pt / (float)P0;
    const int32_t dst = base + (int32_t)outT;

    for (int k = -P0; k < P0; k++) {
      const int s = center + k;
      if (s < 0 || s >= (int)u.length) continue;
      const int32_t o = dst + k;
      if (o < 0 || o >= cap) continue;
      _work[o] += (float)src[s] * s_hann[k + P0] * gain;
    }
    outT += Pt;
  }
}

// ---------------------------------------------------------------------------
// 注記: 配布版 2.0.0 で開始時に _err を消去する。成功しても打ち切りが起きた場合は
//       lastError() に理由が入る (戻り値は true)。
bool MakoTalk::synth(const char* symbols, const int16_t** out, size_t* outSamples) {
  _err = ""; _nMorae = 0; _outLen = 0;
  if(out)*out=nullptr; if(outSamples)*outSamples=0;
  if(!symbols){_err="Null symbols";return false;}
  if (!_v || !_work) { _err = "begin() が済んでいません"; return false; }
  _err = "";   // 配布版 2.0.0: 前回のエラー・打ち切り表示を持ち越さない (B-1)
  const uint32_t t0 = micros();

  if (!parse(symbols)) { _err = "発話可能なモーラがありません"; return false; }

  // 総サンプル数 (末尾に窓の食み出し分を確保)
  size_t total = (size_t)_p0 * 2;
  for (int i = 0; i < _nMorae; i++) total += _morae[i].durSmp;
  if (total > _cap) {
    total = _cap;
    _err = "発話が長すぎるため打ち切りました";
    size_t used=0;int kept=0;
    for(int i=0;i<_nMorae && used<total;i++){
      if(_morae[i].durSmp>total-used)_morae[i].durSmp=(uint16_t)(total-used);
      used+=_morae[i].durSmp;kept++;
    }
    _nMorae=kept;
  }

  memset(_work, 0, total * sizeof(float));

  int32_t pos = 0;
  for (int i = 0; i < _nMorae; i++) {
    renderMora(_morae[i], pos);
    pos += _morae[i].durSmp;
    if ((size_t)pos >= total) break;
  }

  // float → int16 (音量適用 + クリップ)
  // v0.2: 辞書側で全素片を共通ゲイン正規化したため、ここは素通しに近くてよい。
  //       モーラ重畳分のヘッドルームだけ残す。
  const float g = ((float)_volume / 100.0f) * 0.95f;
  for (size_t i = 0; i < total; i++) {
    float v = _work[i] * g;
    if (v >  32767.0f) v =  32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    _out[i] = (int16_t)v;
  }

  _outLen = total;
  _lastMicros = micros() - t0;
  if (out)        *out = _out;
  if (outSamples) *outSamples = _outLen;
  return true;
}

// ---------------------------------------------------------------------------
bool MakoTalk::play(const int16_t* pcm, size_t samples) {
  if (_outDev.write) {
    if (!_outDev.write(pcm, samples, _outDev.ctx)) { _err = "出力先への書き込みに失敗"; return false; }
    return true;
  }
  if (!_i2sReady) { _err = "I2Sが未初期化です"; return false; }
  s_i2s.write((uint8_t*)pcm, samples * sizeof(int16_t));
  return true;
}

// ---------------------------------------------------------------------------
bool MakoTalk::speak(const char* symbols) {
  const int16_t* pcm = nullptr;
  size_t n = 0;
  if (!synth(symbols, &pcm, &n)) return false;
  if (strlen(lastError())) return false; // Never mark a truncated credit as announced.
  return play(pcm, n);
}

// ---------------------------------------------------------------------------
//  リップシンク用アクセサ
// ---------------------------------------------------------------------------
uint8_t MakoTalk::visemeVowel(int i) const {
  if (i < 0 || i >= _nMorae) return 6;
  const int16_t idx = _morae[i].unitIdx;
  if (idx < 0) return 6;                    // ポーズ/促音 = 無音
  return _v->units[idx].vowel;              // 0=a 1=i 2=u 3=e 4=o 5=ん
}

uint32_t MakoTalk::visemeStartSample(int i) const {
  uint32_t pos = 0;
  const int n = (i < _nMorae) ? i : _nMorae;
  for (int k = 0; k < n; k++) pos += _morae[k].durSmp;
  return pos;
}
