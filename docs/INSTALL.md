# Arduino IDEの設定と配線

## 対象

確認済みはESP32-S3、Quad Flash16MB、Octal PSRAM8MB、MAX98357Aです。
Arduinoボード名は互換プリセットであり、メーカー／基板型式の同定ではありません。
UNO R3・従来Nano・AVR向けではありません。

| IDE項目 | 設定 |
|---|---|
| ボードパッケージ | esp32 by Espressif Systems 3.3.12 |
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240MHz |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
| USB CDC On Boot | Disabled |
| USB Mode | Hardware CDC and JTAG |
| Upload Mode | UART0 / Hardware CDC |
| Serial Monitor | 115200 baud |

物理Flash16MBに対し、従来の実機試験と同じ8MB用パーティションを選んでいます。
全16MBを利用する構成ではありません。ボードを変更すると設定が初期値へ戻る場合があるため、全項目を再確認してください。

この開発端末ではCH340側の**COM5のみ**を使います。他の利用者の環境では自分の対象ボードのポートを選びます。
このプロジェクトの検証では他のUSB機器へ接続しません。

## MAX98357Aとの配線

| I2S信号 | サンプルGPIO | アンプ側 |
|---|---:|---|
| BCLK | 5 | BCLK |
| LRCLK / WS | 6 | LRC |
| DATA | 4 | DIN |

I2SはBCLK・LRCLK・DATAです。I2CのSDA/SCLとは別です。MCLKは使いません。
電源・GNDはモジュールの仕様に合わせ、スピーカーはアンプの差動出力間へ接続します。
確認済み構成のGAINはGNDです。LCD・マイク・Wi-Fiは最小例では使いません。

## 起動反復・無音

### RTCWDT_RTC_RSTで繰り返し再起動する

この端末に「ESP32S3 Dev Module Octal (WROOM2)」とFlash Mode=OPIを選ぶと、物理Flashと一致しません。
**PSRAMがOctalでもFlashはQuadの場合があります。** 通常のボード・QIO・OPI PSRAMへ合わせ、通常の書き込みをやり直します。
ブートローダーも設定へ合わせる必要があり、アプリ領域だけの再書き込みでは不十分です。
`mode:DOUT`という文字列だけで異常と断定せず、ボード設定と実機仕様を照合してください。

全Flash消去・NVS消去は不要です。この開発プロジェクトでは禁止しています。

### スケッチが大きすぎて書き込めない

サンプルは約1.56MBです。4MB Defaultのアプリ領域約1.2MBには収まりません。
Flash SizeとPartition Schemeを表の設定へ合わせてください。

### 初期化エラーが表示される

`mlastError()`を確認します。PSRAM設定、辞書のCRC・著作情報、I2Sピン・既存ポート使用を確認してください。
I2Sはマスター送信、16kHz、16bit、左右同値ステレオです。

### 一度話した後、何も起きない

最小例は`setup()`で1回だけ話します。正常です。再起動か、自分のスケッチから再度呼び出してください。
発話中はブロックするため、アプリ側の処理と必要に応じて調整してください。

### 初回のクレジットを聞き直したい

`mannounceCredit()`を使います。告知のためにFlashやNVSを消去する必要はありません。
