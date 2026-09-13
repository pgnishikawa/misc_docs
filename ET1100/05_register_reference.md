# 5. レジスタ早見表

[← 目次](README.md)

出典: `ethercat_et1100_datasheet_v2i1.pdf` Table 7（Register overview）、および
`ethercat_esc_datasheet_sec2_registers_v3.3.pdf`（詳細ビット定義）。**最終的な
ビット定義・リセット値は必ず原本（Section II）を参照してください。** ここでは
ソフトウェア実装時によく参照するものを中心に抜粋しています。

## 5.1 基本情報（起動時の疎通確認に使う）

| アドレス | サイズ | 名称 | ET1100 期待値・備考 |
|---|---|---|---|
| `0x0000` | 1 | Type | ESC種別コード |
| `0x0001` | 1 | Revision | |
| `0x0002:0x0003` | 2 | Build | |
| `0x0004` | 1 | FMMUs supported | **8** |
| `0x0005` | 1 | SyncManagers supported | **8** |
| `0x0006` | 1 | RAM size [KByte] | **8** |
| `0x0007` | 1 | Port descriptor | 各ポートの種別(EBUS/MII/未実装) |
| `0x0008:0x0009` | 2 | ESC features supported | FMMU方式・DC有無等のフラグ |

## 5.2 アドレス設定

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0010:0x0011` | 2 | Configured station address |
| `0x0012:0x0013` | 2 | Configured station alias |

## 5.3 書き込み保護

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0020` | 1 | Write register enable |
| `0x0021` | 1 | Write register protection |
| `0x0030` | 1 | ESC write enable |
| `0x0031` | 1 | ESC write protection |

## 5.4 リセット

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0040` | 1 | ESC reset ECAT（3フレーム連続コマンドで実リセット、Section I 17.5） |
| `0x0041` | 1 | ESC reset PDI（ET1100 では未実装） |

## 5.5 データリンク層・EtherCAT 状態機械

| アドレス | サイズ | 名称 | 備考 |
|---|---|---|---|
| `0x0100:0x0101` | 2 | ESC DL control | |
| `0x0102:0x0103` | 2 | Extended ESC DL control | |
| `0x0108:0x0109` | 2 | Physical read/write offset | |
| `0x0110:0x0111` | 2 | **ESC DL status** | bit0 = **PDI operational**（起動待ちで見る） |
| `0x0120:0x0121` | 2 | **AL control** | マスタからの状態遷移要求（下位4bit: 1=Init,2=PreOp,3=Boot,4=SafeOp,8=Op） |
| `0x0130:0x0131` | 2 | **AL status** | スレーブの現在状態。bit4=エラー表示 |
| `0x0134:0x0135` | 2 | **AL status code** | 状態遷移エラーの詳細コード（ETG.1020参照） |

## 5.6 PDI 設定（本ドキュメントの中心）

| アドレス | サイズ | 名称 | 備考 |
|---|---|---|---|
| `0x0140` | 1 | **PDI0 control** | bit[7:0] = PDI種別（8=16bit非同期, 9=8bit非同期 等）。**EEPROM設定、実行時変更不可** |
| `0x0141` | 1 | ESC configuration A0 | Device emulation 等 |
| `0x0150` | 1 | **PDI0 configuration** | PDI種別ごとに意味が変わる（非同期µC時：BUSY/IRQ極性・ドライバ種別、BHE極性、RD極性） |
| `0x0151` | 1 | DC sync/latch configuration | DC SyncSignalの割り込みマッピング等 |
| `0x0152:0x0153` | 2 | Extended PDI0 configuration | Read BUSY delay、内部書込タイミング等 |

## 5.7 割り込み

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0200:0x0201` | 2 | ECAT event mask |
| `0x0204:0x0207` | 4 | **AL event mask**（PDI割り込みのマスク） |
| `0x0210:0x0211` | 2 | ECAT event request |
| `0x0220:0x0223` | 4 | **AL event request**（PDI割り込みの要因。ISR内で必ず読み切る） |

## 5.8 エラーカウンタ（デバッグ用）

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0300:0x0307` | 4×2 | RX error counter[3:0] |
| `0x0308:0x030B` | 4×1 | Forwarded RX error counter[3:0] |
| `0x030C` | 1 | ECAT processing unit error counter |
| `0x030D` | 1 | PDI0 error counter（**PDIバスアクセスエラー時に増加**。3.4節の不正アクセス検出に有用） |
| `0x0310:0x0313` | 4×1 | Lost link counter[3:0] |

## 5.9 ウォッチドッグ

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0400:0x0401` | 2 | Watchdog divider |
| `0x0410:0x0411` | 2 | Watchdog time PDI0 |
| `0x0420:0x0421` | 2 | Watchdog time process data |
| `0x0440:0x0441` | 2 | Watchdog status process data |
| `0x0442` | 1 | Watchdog counter process data |
| `0x0443` | 1 | Watchdog counter PDI0 |

## 5.10 SII EEPROM インタフェース／PHY 管理

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0500:0x050F` | 16 | SII EEPROM interface（I²C経由のEEPROM読み書き制御をレジスタ経由でも可能） |
| `0x0510:0x0515` | 6 | PHY management interface（MDIO 経由の PHY レジスタアクセス） |

## 5.11 FMMU（8チャネル、各16バイト）

`0x0600 + N*16`（N=0..7）。1チャネルのレイアウト（Section I Table 24）：

| オフセット | サイズ | 名称 |
|---|---|---|
| `0x0:0x3` | 4 | Logical start address |
| `0x4:0x5` | 2 | Length (bytes) |
| `0x6` | 1 | Logical start bit |
| `0x7` | 1 | Logical stop bit |
| `0x8:0x9` | 2 | Physical start address |
| `0xA` | 1 | Physical start bit |
| `0xB` | 1 | Type（read/write/read-write） |
| `0xC` | 1 | Activate（1=有効） |

## 5.12 SyncManager（8個、各8バイト）

`0x0800 + N*8`（N=0..7）。1個のレイアウト（Section I Table 25）：

| オフセット | サイズ | 名称 |
|---|---|---|
| `0x0:0x1` | 2 | Physical start address |
| `0x2:0x3` | 2 | Length |
| `0x4` | 1 | Control register（モード：buffered/mailbox、方向 等） |
| `0x5` | 1 | Status register |
| `0x6` | 1 | Activate |
| `0x7` | 1 | PDI control |

典型構成例（あくまで一例、実際は EEPROM/マスタ設定次第）：SM0=Mailbox Out、
SM1=Mailbox In、SM2=Process Data Out、SM3=Process Data In。

## 5.13 Distributed Clocks（概要のみ）

| アドレス | 名称 |
|---|---|
| `0x0900:0x090F` | DC – receive times |
| `0x0910:0x0917` | DC – system time |
| `0x0920:0x0927` | DC – system time offset |
| `0x0980` | DC – cyclic unit control |
| `0x0981` | DC – activation |
| `0x098E/0x098F` | DC – SYNC0/SYNC1 status |
| `0x09A8/0x09A9` | DC – latch0/1 control |

必要になった時点で Section I 第9章を参照してください（本ドキュメントでは詳細割愛）。

## 5.14 その他メモリ領域

| アドレス | サイズ | 名称 |
|---|---|---|
| `0x0E00:0x0E07` | 8 | Power-on values |
| `0x0F00:0x0F03` | 4 | Digital I/O output data（Digital I/O PDI使用時） |
| `0x0F80:0x0FFF` | 128 | **User RAM**（EtherCAT機能に影響しない汎用領域。バス疎通確認に便利） |
| `0x1000`〜 | 8192 (8KB) | **プロセスデータ RAM** |
