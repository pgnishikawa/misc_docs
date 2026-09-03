# RZ/N2H 概要

出典: ユーザーズマニュアル ハードウェア編 R01UH1039JJ0130 Rev.1.30、「特長」(p.73)〜「1. 概要」(p.74-89)

## 位置づけ

- FPU / NEON を備える **Cortex-A55 クワッド MPCore** ＋ **デュアル Cortex-R52** の高性能 ASSP。
- 産業用途（モータ制御、産業用イーサネット、EtherCAT、エンコーダ I/F）向けに周辺機能を統合。
- RZ/T2H と RZ/N2H は同一ダイのパッケージ違い＋一部機能差（後述）。

## CPU

| 項目 | Cortex-A55 | Cortex-R52 |
|------|-----------|-----------|
| 構成 | クワッド／デュアル／シングル MPCore × 1 プロセッサ | シングルコア × 2 プロセッサ (CPU0, CPU1) |
| コア revision | r2p0 | r1p4 |
| 動作周波数 | コア 600/1200 MHz、DSU 500/1000 MHz | 各 CPU 500/1000 MHz |
| アーキテクチャ | Arm v8.2-A、NEON/FPU、暗号拡張（セキュリティ品のみ） | Arm v8-R、Thumb/Thumb-2、NEON/FPU |
| アドレス空間 | 32 GB (35 bit) | 4 GB (32 bit) |
| L1 | I 32 KB(パリティ) / D 32 KB(ECC)（コアごと） | I 16 KB / D 16 KB（ECC、各 CPU） |
| L2 / L3 | L2 = 0、L3 = 1024 KB (ECC) | — |
| TCM | — | ATCM 512 KB(ECC) / BTCM 64 KB(ECC) / CTCM 0（各 CPU）。ATCM は 500 MHz で 0 wait、1000 MHz で 1 wait |
| MPU | — | 2 ステージ MPU (EL2/EL1)、各 24 領域 |

- ATCM の wait は **リセット中のみ** 端子 (CPU0=MDW0, CPU1=MDW1) で 0/1 wait を選択（[04_boot_modes.md](04_boot_modes.md)）。
- DCLS（デュアルコアロックステップ）は非サポート。

## メモリ

| メモリ | 仕様 |
|--------|------|
| 内蔵システム SRAM | 2.0 MB（512 KB × 4 ユニット）、ECC (SEC-DED)、250 MHz、エラーインジェクション対応 |
| ブート ROM | 128 KB（0x0_1100_0000〜、[03_memory_map.md](03_memory_map.md)） |
| OTP | ワンタイムプログラマブル。ユニーク ID／認証設定／トリミング／ブートモード設定／ユーザー領域。上書き保護＋冗長 |
| LPDDR4 SDRAM I/F | 3.2 Gbps、32 bit 幅、ランク 1/2、最大 64 Gb、インライン ECC。**LPDDR4X 非対応** |
| 外部アドレス空間（BSC） | 最大 4 CS 領域 (CS0/CS2/CS3/CS5)、8/16/32 bit、最高 125 MHz |

## クロック（詳細は [05_clocks_reset.md](05_clocks_reset.md)）

- 外部クロック／発振子入力: **25 MHz**（EtherCAT 使用時は ±25 ppm）
- CPU: A55 600/1200 MHz、R52 500/1000 MHz
- システムバス: A-Bus 400 MHz、R-Bus 250 MHz
- 周辺: PCLKAH 400 / PCLKAM 200 / PCLKAL 100 MHz（A 系）、PCLKH 250 / PCLKM 125 / PCLKL 62.5 MHz（R 系）
- ADC クロック 62.5 MHz、外部バス 125 MHz、LOCO 1 MHz

## 割り込み (ICU)（マニュアル 12 章 p.305〜）

- Cortex-A55 は GIC-600、Cortex-R52 CPU0/CPU1 は各 GIC に接続。
- 周辺機能割り込み: **394 要因**／外部割り込み: 16 (IRQ0〜IRQ15)／ソフトウェア割り込み: 16／システムエラー: 1。
- GIC に 32 レベルの優先順位設定可。DMAC・ELC への起動トリガも接続。

## データ転送・イベント

- **DMAC**: 16 ch × 3 ユニット。転送モード single/block。転送サイズ ユニット0 は 1〜64 B、ユニット1/2 は 1〜32 B。起動要因はソフト／外部 DREQ／外部割り込み／周辺割り込み。
- **ELC（イベントリンクコントローラ）**: 最大 632（マニュアル本文では最大 632 イベント信号）をモジュール動作に連動。CPU スタンバイ中もリンク動作可。

## 主な周辺機能一覧

### タイマ
| モジュール | 数量 |
|-----------|------|
| MTU3 | 1 ユニット 9 ch（16 bit × 8 ＋ 32 bit × 1）、最高 250 MHz。相補 PWM／リセット同期 PWM／位相計数 等 |
| GPT（32 bit） | 56 ch（5ch×9 ＋ 7ch×1 ＋ 4ch×1）＝ 11 ユニット。LLPP 動作時最高 500 MHz |
| CMT（16 bit） | 2 ch × 3 ユニット（6 ch） |
| CMTW（32 bit） | 1 ch × 2 ユニット |
| WDT | 14 bit × 6 ch |
| POE3 | 1 ユニット（MTU3 出力の Hi-Z 制御） |
| POEG | 3 ユニット（GPT 出力禁止制御） |
| RTC | 1 ユニット（2000〜2099 年、BCD、うるう年補正） |

### 通信
| モジュール | 数量・要点 |
|-----------|-----------|
| イーサネット MAC (GMAC) | 1 ポート × 3 ユニット。10/100/1000M、IEEE1588、EEE、TSN (802.1Qbv/Qbu/802.3br)、MII/RMII/RGMII、8 RX/8 TX キュー・DMA ch |
| イーサネットスイッチ (ETHSW) | 1 ユニット 3 外部ポート。HW スイッチング、QoS、VLAN、IEEE1588、DLR、PRP、フレームプリエンプション、TDMA スケジューラ |
| EtherCAT スレーブ (ESC) | 1 ch（3 ポート）。Beckhoff IP コア。MII 推奨（RMII/RGMII も可） |
| USB2.0 HS | 1 ポート。ホスト／ファンクション／OTG。HS(480M)/FS/LS。DMAC 2 ch 内蔵。RAM: ホスト 1 KB / ファンクション 8 KB |
| SCI / SCIE | 6 ch ＋ エンコーダ用 12 ch。調歩同期／クロック同期／簡易 I2C／簡易 SPI／スマートカード／マンチェスタ。RS-485 制御あり |
| I2C (IIC) | 3 ch。最大 400 kbps。I2C/SMBus、マルチマスタ |
| CAN-FD | 2 ch。ISO 11898-1(2015)。公称 1 Mbps／データ 8 Mbps。合計 192 メッセージバッファ、受信ルール最大 256 |
| SPI | 4 ch。4/32 bit、32 bit × 4 段 FIFO、1 転送で最大 4 フレーム |
| xSPI | 2 ch。JESD251。1/4/8 pin SDR/DDR（1S-1S-1S 〜 8D-8D-8D）、OctaFlash/RAM・HyperFlash/RAM、XiP、最大 256 MB |
| SD/MMC ホスト (SDHI) | 2 ch（ch0 最大 8 bit、ch1 最大 4 bit）。SD/SDHC/SDXC、UHS-I 各種、eMMC(HS200) |
| PCI Express Gen3 | 1 ユニット。Gen1/2/3、RC/EP、1 レーン×2 ポート または 2 レーン×1 ポート。AER、ペイロード最大 256 B。**ASPM L1-Substate 非対応** |
| シリアルホスト I/F (SHOSTIF) | 外部ホスト用スレーブ。4 線 SPI／Dual/Quad/Octal 拡張 SPI。最大 32 bit × 64 バースト |
| メールボックス／セマフォ (MBXSEM) | セマフォ 8／32 bit メールボックス 4（双方向）。外部ホスト⇔A55/R52 |

### アナログ・産業用
| モジュール | 数量・要点 |
|-----------|-----------|
| 12 bit ADC (ADC12) | 3 ユニット（ユニット0/1: 4 ch、**ユニット2: RZ/N2H は 15 ch**／RZ/T2H は 6 ch）。0.32 µs/ch、S/H、ダブルトリガ、MTU3/ELC/外部トリガ |
| 温度センサ (TSU) | 1 ch、相対精度 ±2 ℃(Typ) |
| ΔΣ I/F (DSMIF) | 3 ch × 10 ユニット（RZ/T2H）／**RZ/N2H は 8 ユニット 23 ch**。外部 ΔΣ モジュレータ接続。Sinc 1〜3 次。POE3/POEG に直接エラー接続 |
| 三角関数ユニット (TFU) | 2 ユニット。sin/cos、arctan/hypot_k を同時計算 |
| エンコーダ I/F | EnDat 2.2 / BiSS-C / A-format / HIPERFACE DSL（RZ/T2H 各 16 ユニット、**RZ/N2H 各 14 ユニット**）＋ ENCOUT 1 ユニット |

### セーフティ／セキュリティ／その他
| モジュール | 要点 |
|-----------|------|
| CRC | 2 ch。8/16/32 bit。多項式 5 種（32-Ethernet, CRC-32C, CRC-16, CRC-CCITT, CRC-8） |
| CLMA（クロックモニタ） | 7 ユニット。入力クロック／PLL／LOCO の異常周波数監視、発振停止検出 |
| DOC（データ演算回路） | 1 ユニット。16 bit 比較／加算／減算 |
| レジスタライトプロテクション | 重要レジスタの誤書き換え防止 |
| マスタ MPU | A55/R52 以外のマスタ（DMAC, USB, GMAC, CoreSight, SHOSTIF, LCDC, SDHI, PCIE）に対するメモリ保護 |
| 独立セーフティ周辺 | GPT 4ch / SCI 1ch / IIC 1ch / SPI 1ch / CRC 1 / RTC 1 / GPIO / ECC SRAM。EL2 MPU で保護可能。R-Bus セーフティ空間 (0x0_8100_0000) にマップ |
| セキュリティ（オプション品） | セキュアブート、JTAG 認証、SCI/USB ブート認証、暗号アクセラレータ（AES128/192/256、ECC256、RSA1024/2048/3072、SHA-1/2、HMAC/CMAC/GMAC、ECDSA/RSASSA）、TRNG、A55 暗号拡張、Arm TrustZone |
| デバッグ | CoreSight。JTAG/SWD、ETF トレース、ETR |

### ディスプレイ
- **LCDC**: パラレル RGB 出力。2 面ブレンド、ディザ(RGB666)／クリッピング／ガンマ補正 LUT。入力 RGB/ARGB/YCbCr 各種、出力 RGB666/RGB888。WXGA 1280×800 @60fps。

## 電源電圧（[06_hardware_design_guide.md](06_hardware_design_guide.md) に端子別詳細）

| 電圧 | 用途 |
|------|------|
| 0.8 V | コア（デジタル＋アナログ: PLL, TSU, OTP, USB, PCIE, ADC） |
| 1.1 V | DDR (DDR_VDDQ) |
| 1.8 V | PLL, OSC, USB, ADC, TSU, OTP, PCIE, DDR, RGMII |
| 3.3 V | GPIO(3.3 V 固定ドメイン), USB, OSC, RMII/MII, その他周辺 |
| 1.8 V / 3.3 V 選択 | xSPI, SDHI, GPIO（1.8/3.3 V 選択ドメイン VDD1833_n） |

動作温度: **Tj = -40〜+125 ℃**

## 製品ラインナップ

| 製品 | 型名 | パッケージ | Cortex-A55 | Cortex-R52 | セキュリティ |
|------|------|-----------|-----------|-----------|--------------|
| RZ/N2H | R9A09G087**M48**GBG | 576 ピン FCBGA (21×21mm, 0.8mm) | クワッド | CPU×2 | 使用可 |
| RZ/N2H | R9A09G087**M28**GBG | 576 ピン FCBGA | デュアル | CPU×2 | 使用可 |
| RZ/N2H | R9A09G087**M08**GBG | 576 ピン FCBGA | シングル | CPU×2 | 使用可 |
| RZ/N2H | R9A09G087**M44**GBG | 576 ピン FCBGA | クワッド | CPU×2 | 使用不可 |
| RZ/N2H | R9A09G087**M24**GBG | 576 ピン FCBGA | デュアル | CPU×2 | 使用不可 |
| RZ/N2H | R9A09G087**M04**GBG | 576 ピン FCBGA | シングル | CPU×2 | 使用不可 |
| RZ/N2H | R9A09G087**M48**GBA | 669 ピン FCBGA (17×17mm, 0.5mm) | クワッド | CPU×2 | 使用可 |
| RZ/N2H | R9A09G087**M28**GBA | 669 ピン FCBGA | デュアル | CPU×2 | 使用可 |
| RZ/N2H | R9A09G087**M08**GBA | 669 ピン FCBGA | シングル | CPU×2 | 使用可 |
| RZ/T2H | R9A09G077MxxGBG | 729 ピン FCBGA (23×23mm, 0.8mm) | — | — | — |

- 型名の `M4x` = セキュリティ非対応、`Mx8/Mx4` の末尾で A55 コア数（8=クワッド, 4=デュアル…実際は表の通り M48/M28/M08=セキュリティ有、M44/M24/M04=無）。
- TrustZone は全型名で使用可（上表「セキュリティ」列は TrustZone を除く暗号機能等の可否）。
- RZ/N2H MPU パッケージタイプは `MD_MON.MDP = 1`（RZ/T2H 729 ピンは 0）。

## RZ/N2H と RZ/T2H の主な違い（機能の比較 p.85-86）

| 項目 | RZ/T2H (729 ピン) | RZ/N2H (576 / 669 ピン) |
|------|------------------|------------------------|
| パッケージ | 729 FCBGA | 576 FCBGA / 669 FCBGA |
| GPIO 入出力端子数 | 287 | 189 / 189 |
| ADC12 ユニット2 | 6 ch | **15 ch** |
| DSMIF | 10 ユニット (30 ch) | **8 ユニット (23 ch)**（ユニット6・9、ユニット8 ch2 は必須外部信号不足で不可） |
| EnDat 2.2 / BiSS-C / A-format | 各 16 ユニット | **各 14 ユニット**（ユニット8・15 は不可） |
| HIPERFACE DSL | 16 ユニット | **14 ユニット**（同上。一部外部信号不可） |
| GMAC / ETHSW / SPI / xSPI | フル | 数量同じだが**一部外部信号が使用不可** |

- CPU、システム SRAM 2.0 MB、外部バス 32 bit、DMAC 3 ユニット、MTU3 9 ch、GPT 56 ch、CMT/CMTW/WDT、CANFD 2 ch、IIC 3 ch、PCIE、SDHI 2 ch、TFU 2、CRC 2、CLMA 7、DOC 1、OTP、SHOSTIF、MBXSEM、ELC、DDRSS 32 bit、LCDC 1 は共通。

## ブロック図（p.88-89 のテキスト表現）

- **Cortex-A55 クラスタ**（コア0〜3 ＋ 各 L1 32/32 KB ＋ 共有 L3 1 MB ＋ DSU）→ ACE-M 経由で内部メインバス A。GIC-600。
- **Cortex-R52 CPU0 / CPU1**（各 TCM 512/64 KB）→ AXI-S/AXI-M で内部メインバス R、LLPP で LLPP バス。各 GIC。
- **内部メインバス A** に接続: PCIE(2Lx1P/1Lx2P)、USB2.0、GMAC(ユニット1,2)、DDRSS、CoreSight(ETR/AXI-AP)、LCDC、SDHI(ユニット0=SD/eMMC, ユニット1=SD)、デバッグ APB。
- **内部メインバス R** に接続: GMAC(ユニット0)、ETHSW(3 ポート)、ESC(3 ポート)、DMAC(ユニット0/1/2 各 16 ch)、システム SRAM、xSPI(2 ch)、BSC、RSIP、OTP、ブート ROM、SHOSTIF、MBXSEM、ICU/ELC、ノンセーフティ／セーフティ周辺群。
- **LLPP バス**（Cortex-R52 CPU0/CPU1 アクセス優先）: MTU3、GPT(ユニット0〜8＝30ch/…)、TFU、POE3、POEG、ADC12(ユニット0/1)、DSMIF、エンコーダ I/F SS、SCIE、ENCOUT。
- I/O ポートは ポート0〜35。
