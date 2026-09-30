# ChainOSCPad MIDI

[ChainOSCPad](https://github.com/shimez/ChainOSCPad)と同じハードウェアを使用する、12キーのMIDIキーボード・ファームウェアです。PlatformIO / Arduino-ESP32でビルドします。

**[Web Installerを開く](https://shimez.github.io/ChainOSCPad-MIDI/installer/)** · [GitHub Releases](https://github.com/shimez/ChainOSCPad-MIDI/releases)

PCのChrome / EdgeからUSB経由でインストールできます。リリース手順とGitHub Pagesの構成は[GITHUB_SETUP.md](GITHUB_SETUP.md)を参照してください。

## 対応ボード

| ボード | PlatformIO環境 | USB MIDI | BLE-MIDI |
|---|---|---|---|
| Seeed Studio XIAO ESP32S3 | `xiao_esp32s3` | ○ | ○ |
| Seeed Studio XIAO ESP32C3 | `xiao_esp32c3` | — | ○ |
| Seeed Studio XIAO ESP32C6 | `xiao_esp32c6` | — | ○ |
| Seeed Studio XIAO ESP32C5 | `xiao_esp32c5` | — | ○ |

S3はUSBとBLEの両方を同時に使用できます。両方で同じDAWに接続する場合は、DAW側で使用する入力ポートを選んでください（両方を有効にすると二重発音になります）。C3/C6/C5のUSBは書き込み・シリアル用です。

## 初期割り当て

- MIDIチャンネル：**1**
- Key 1〜12：**ノート60〜71**（C〜Bの半音階、中央Cのノート番号は60）
- 押す：Note On、離す：Note Off。ベロシティは固定値 **100**
- エンコーダー回転：**CC 1（Modulation）**、0〜127、初期値0、1クリック±1
- エンコーダー押しボタン：**CC 64（Sustain）**、押している間127、離すと0
- BLEデバイス名：**ChainOSCPad MIDI**、同時接続は1台

キー番号は配線上の行優先です。実際のケースの向きに合わせて`include/config.h`の`NOTES`を変更できます。

```text
          COL0   COL1   COL2
ROW0      1/60   2/61   3/62
ROW1      4/63   5/64   6/65
ROW2      7/66   8/67   9/68
ROW3     10/69  11/70  12/71
         キー番号 / MIDIノート番号
```

12キーの同時押しに対応します。スイッチマトリクスは打鍵速度を計測できないため、ベロシティは設定値を使用します。

## 配線

元のChainOSCPadと共通です。ソースでは各ボードの`D0`〜`D10`定義を使用します。

| XIAO | S3 GPIO | C3 GPIO | C6 GPIO | C5 GPIO | 接続先 |
|---|---:|---:|---:|---:|---|
| D0 | 1 | 2 | 0 | 1 | ROW0 |
| D1 | 2 | 3 | 1 | 0 | ROW1 |
| D2 | 3 | 4 | 2 | 25 | ROW2 |
| D3 | 4 | 5 | 21 | 7 | ROW3 |
| D4 | 5 | 6 | 22 | 23 | COL0 |
| D5 | 6 | 7 | 23 | 24 | COL1 |
| D6 | 43 | 21 | 16 | 11 | COL2 |
| D7 | 44 | 20 | 17 | 12 | Encoder A |
| D8 | 7 | 8 | 19 | 8 | Encoder B |
| D9 | 8 | 9 | 20 | 9 | Encoder Push |
| D10 | 9 | 10 | 18 | 10 | Status / Activity LED |

- ROWを1本ずつLOWにし、内部プルアップ付きCOLを読み取ります。非選択ROWはオープンドレインのハイインピーダンス状態です。
- 各キーにはダイオードを1本ずつ使用してください。方向は **`COL →| ROW`**（アノード＝COL、カソード／帯＝ROW）です。ダイオードなしでは複数同時押し時にゴースト入力が発生します。
- エンコーダーの共通端子、押しボタンの反対側はGNDへ。A/B/Pushは内部プルアップを使用します。
- D10のLEDには直列の電流制限抵抗を使用してください。初期値はHIGHで点灯します。極性が逆の場合は`LED_ACTIVE_HIGH = false`にします。

## ビルド・書き込み

VS CodeのPlatformIO拡張機能、またはPlatformIO Coreを使用します。初回はプラットフォーム、ツールチェーン、ライブラリのダウンロードが必要です。

```powershell
# 全4ボードをビルド
pio run

# 使用するボードだけビルド
pio run -e xiao_esp32s3
pio run -e xiao_esp32c3
pio run -e xiao_esp32c6
pio run -e xiao_esp32c5

# 使用するボードの環境名で書き込み
pio run -e xiao_esp32s3 -t upload
# ポートを明示する場合
pio run -e xiao_esp32s3 -t upload --upload-port COM5
pio device monitor --baud 115200
```

PowerShellで`pio`がPATHにない場合は、`& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`のように実行できます。

書き込み用ファイルは`.pio/build/<環境名>/`に生成されます。通常は`-t upload`を使用してください。書き込めない場合はXIAOのBOOTボタンを押したままUSBを接続し、BOOTを離してから書き込みます。書き込み後にRESETし、ポートが変わった場合は再選択してください。

全ボードでArduino-ESP32 3.3.11を提供する**pioarduino 55.03.311**を固定指定しています。C5対応ボード定義と、S3の組み込み`USBMIDI`を使用するためです。BLEはC5/C6とArduino-ESP32 3.3.11の互換性修正を含む**NimBLE-Arduino 2.5.1**です。Windowsでツールチェーンがサブディレクトリ付きで展開された場合も、`scripts/platformio_tools.py`が実行ファイルのパスを補います。

## 接続方法

### USB MIDI（S3）

1. データ通信対応USBケーブルでXIAOをPCへ接続します。
2. DAW／MIDIモニターで`ChainOSCPad MIDI`（OSによっては`TinyUSB MIDI`など）を入力として有効にします。
3. ソフトウェア音源の入力チャンネルを1に設定してキーを押します。

USB MIDI 1.0のクラス準拠デバイスです。S3はTinyUSBモード（`ARDUINO_USB_MODE=0`）を使用し、USB CDCシリアルも有効です。

### BLE-MIDI（全ボード）

1. XIAOへUSBまたは既存ハードウェアの電源から給電します。
2. ホストの**Bluetooth MIDI接続画面**で`ChainOSCPad MIDI`を接続します。
3. DAW側でそのMIDI入力を有効にします。

- macOS：Audio MIDI設定 → MIDIスタジオ → Bluetooth構成から接続します。
- iOS / iPadOS：使用する音楽アプリのBluetooth MIDI接続機能から接続します。
- Windows：BLE-MIDIに対応するアプリ／MIDI環境を使用してください。通常のBluetoothペアリングだけでは、DAWの入力一覧に表示されない構成があります。
- Windows + Edgeで接続済みなのにWeb MIDIの入力が0件の場合：Bluetooth接続後、`edge://restart`でEdgeを再起動してからサイトへ接続してください。KORG BLE-MIDI Driver 1.16系でこの手順による認識を確認しました。
- Android：BLE-MIDI対応アプリの接続画面を使用します。必要に応じてBluetooth権限を許可してください。

標準BLE-MIDIサービスUUIDと13-bitミリ秒タイムスタンプを使用します。切断時は再広告します。LEDの接続判定はBLEリンクだけでなく、ホストによるMIDI通知の購読完了を基準にしています。

## LED表示

| 状態 | 表示 |
|---|---|
| MIDI未接続 | 500 msごとに点滅 |
| USB列挙完了またはBLE通知購読済み | 点灯 |
| MIDI送信受付 | 約40 ms消灯 |

送信表示はファームウェアから通信スタックへの受付を示します。音源での発音確認はMIDIモニター等で行ってください。

## 設定変更

`include/config.h`を編集して再ビルド・書き込みします。

| 設定 | 用途 |
|---|---|
| `NOTES` | 12キーのノート番号（0〜127、重複不可） |
| `CHANNEL` | MIDIチャンネル（1〜16） |
| `VELOCITY` | 固定ベロシティ（1〜127） |
| `ENCODER_CC` / `ENCODER_INITIAL` / `ENCODER_STEP` | 回転のCC番号、初期値、増減量 |
| `ENCODER_DIRECTION` | 回転方向（1または-1） |
| `ENCODER_TRANSITIONS` | 1クリックの遷移数（4、2、1） |
| `BUTTON_CC` | 押しボタンのCC番号 |
| `DEBOUNCE_MS` | キーと押しボタンのデバウンス（初期値5 ms） |
| `DEVICE_NAME` | BLE名／USB製品名 |

設定はコンパイル時固定です。再起動するとCC値は初期値に戻ります。MIDI送信用コントローラーとして動作し、受信メッセージによる設定変更、OSC、Wi-Fi設定画面は実装していません。

## 入力・再接続の処理

- 1 msのESPタイマーで入力タスクを起こし、スキャン間隔をRTOSのtick長から独立させています。エンコーダーはGPIO割り込みで読み取ります。
- 各トランスポートで送信済み状態を独立して管理します。送信バッファが一杯の場合は次回更新で再試行し、離したキーのNote Offを単純に破棄しません。
- 再接続時は設定チャンネルにCC64=0、All Sound Off（CC120）、All Notes Off（CC123）を送信し、その後、現在のキー・押しボタン・エンコーダー値を同期します。キーを押したまま接続するとそのノートが発音します。
- 通信不能中の演奏は蓄積せず、復帰時の現在状態を送ります。通信詰まり中に押して離した短い入力は再現されない場合があります。
- 切断中のホストにはNote Offを送れません。切断直後に音が残る場合は音源側で停止し、再接続して確認してください。

## 検証

実機での確認手順は[TESTING.md](TESTING.md)を参照してください。

## ライセンス

MIT License。元のChainOSCPadの配線・走査方式を参照しています。依存ソフトウェアは[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。
