# MakoTalk — Arduinoで日本語を話す

ESP32-S3上で動く、かな入力の日本語音声合成ライブラリです。
**ライブラリを読み込む → 音声辞書を選ぶ → I2Sピンを指定する → `mspeak()`で話す。**
PCからのWAV転送・クラウド・ネット接続は不要です。

```cpp
#include <MakoTalk.h>
#include <MakoTalkVoiceDefault.h>

void setup() {
  Serial.begin(115200);
  // BCLK=5、LRCLK(WS)=6、DATA=4、I2S1
  if (!mbegin(MAKOTALK_VOICE_DEFAULT, 5, 6, 4, I2S_NUM_1)) {
    Serial.println(mlastError());
    return;
  }
  msetSpeed(130);
  if (!mspeak("あいうえお")) Serial.println(mlastError());
}

void loop() {}
```

> **公開前確認中の候補です。** ソフトウェア利用条件案は[LICENSE](LICENSE)。公開状態は[PUBLICATION_CHECKLIST.md](PUBLICATION_CHECKLIST.md)。

## 用意するもの

- ESP32-S3：今回確認した構成はFlash16MB・OPI PSRAM8MB。
- MAX98357A等の16bit I2S入力アンプ、スピーカー、電源と配線。
- Arduino IDEと「esp32 by Espressif Systems」**3.3.12**。
- このライブラリ一式。追加のライブラリマネージャー操作は不要です。

2026-11-10、権利者によるArduinoライブラリ最小例の実機発話確認済みです。
全ESP32製品や他社ボードの動作保証を意味しません。[確認範囲](docs/VALIDATION.md)

## 導入

1. GitHubの「Code → Download ZIP」でフォルダー一式を取得します。
2. Arduino IDEの「スケッチ → ライブラリをインクルード → .ZIP形式のライブラリをインストール」で導入します。
3. 「ファイル → スケッチ例 → MakoTalk → MinimalSpeak」を開きます。
4. [ボード設定・配線](docs/INSTALL.md)を合わせて書き込みます。

`MinimalSpeak.ino`に実際の`setup()`・`loop()`があります。発話は起動時に1回です。
もう一度聞く場合は通常のリセット、または自分のスケッチから`mspeak()`を呼びます。

### 重要：FlashとPSRAMは別設定

この端末は**Quad Flash＋Octal PSRAM**です。

| 項目 | 確認済み端末の設定 |
|---|---|
| Board | **ESP32S3 Dev Module**（通常のもの） |
| Flash Mode | **QIO 80MHz** |
| PSRAM | **OPI PSRAM** |
| Flash Size | **16MB** |
| Partition Scheme | **8M with spiffs (3MB APP/1.5MB SPIFFS)** |
| USB CDC On Boot | Disabled |

**WROOM2用のOctalボード＋OPI Flashとは異なります。** 実際に誤設定で起動反復が発生しました。
ボード変更後は容量・パーティションも再確認してください。4MBのDefaultアプリ領域は約1.2MBで、約1.56MBのサンプルが入りません。
設定修正後は通常のスケッチ書き込みをやり直します。全Flash消去やNVS消去は不要です。
実際にOctal Flashを搭載する別ボードでは、そのボード固有の仕様を優先してください。

## 入力できるもの

- ひらがな・カタカナ、句読点、長音、アクセント・疑問・感嘆の記号。
- 漢字・英数字の読み推定は行いません。`"32"`ではなく`"さんじゅうに"`と指定します。
- ひらがな正規化は文字変換です。助詞や単語の読み替え・感情の自動推定は行いません。
- 未収録音・不正な入力・過長文はエラーにし、黙って読み飛ばしたり途中だけ発話したりしません。

[API・台詞の書き方](docs/API.md) ／ [仕組みとフォルダー構造](docs/ARCHITECTURE.md)

## 作者・音声・利用条件

**MakoTalk／原著作者：まこと（Makoto）**<br>
**既定音声：VOICEVOX:ずんだもん**（ノーマル由来・123素片）

音声にはソフトウェアとは別の条件があります。[音声・著作情報](docs/VOICE_AND_CREDITS.md)を確認してください。
初回だけの音声告知に加え、配布物・紹介文・動画等にも適切なクレジットを添えてください。

ソフトウェア利用条件案は、個人の学習や非商用での普及を認め、作者表示・改変共有を求めるものです。
**営利企業の利用は、評価・試作を含めて事前相談**とします。学校・広告付き紹介動画・実費頒布の扱いも[LICENSE](LICENSE)に記載しています。
既存のCC BY 4.0ファイルと第三者音声は[NOTICE.md](NOTICE.md)で適用範囲を分けています。
営利企業の利用を制限するため、OSI定義のオープンソースとは表示しません。

相談窓口案：[GitHub Issues](https://github.com/airuyoshii-ai/makotalk-arduino/issues)。問い合わせだけで利用許諾が成立するわけではありません。

## さらに読む

- [配線・IDE設定・トラブル対処](docs/INSTALL.md)
- [API](docs/API.md)
- [内部構造・MKTV v3・CRCと著作情報検査](docs/ARCHITECTURE.md)
- [音声の出典・クレジット](docs/VOICE_AND_CREDITS.md)
- [検証結果と未確認範囲](docs/VALIDATION.md)
- [変更履歴](CHANGELOG.md)
- [改善提案](CONTRIBUTING.md)
