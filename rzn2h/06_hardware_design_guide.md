# ハードウェアデザインガイド 要点

出典: アプリケーションノート「RZ/T2H, RZ/N2H グループ ハードウェアデザインガイド」R01AN7419JJ0120 Rev.1.20（2026.6.15、全 56 ページ）。

RZ/N2H 搭載ボード設計時の参考。回路例のダンピング抵抗・容量値は一例で、実機波形で最終決定すること。

## 1. 電源

### 1.1 電源一覧（表 1.1）

| 項目 | 電源端子名 | Min | Typ | Max |
|------|-----------|-----|-----|-----|
| 3.3 V I/O | `VDD33`, `VDD1833_0`〜`_7`(3.3V モード) | 3.135 | 3.3 | 3.465 V |
| 1.8 V I/O | `VDD1833_0`〜`_7`(1.8V モード), `VDDP_18_33`, `VDDP_18_0`〜`_7` | 1.71 | 1.8 | 1.89 V |
| Core | `VDD08` | 0.76 | 0.8 | 0.84 V |
| グランド | `VSS` | — | 0 | — |
| 水晶 | `VDD33_X` | 3.135 | 3.3 | 3.465 V |
| 水晶 | `VDDP_18_X` | 1.71 | 1.8 | 1.89 V |
| PLL | `VDD18_PLL0`〜`_4` | 1.71 | 1.8 | 1.89 V |
| PLL | `VDD08_PLL0`〜`_4` | 0.76 | 0.8 | 0.84 V |
| TSU | `AVDD18A_TSU` / `DVDD08A_TSU` | 1.71 / 0.76 | 1.8 / 0.8 | 1.89 / 0.84 V |
| OTP | `OTPVDD18` / `OTPVDD08` | 1.71 / 0.76 | 1.8 / 0.8 | 1.89 / 0.84 V |
| USB | `USB_USVDD33` / `USB_USVDD18` / `USB_USDVDD` | 3.135 / 1.71 / 0.76 | 3.3 / 1.8 / 0.8 | 3.465 / 1.89 / 0.84 V |
| PCIe | `PCIE_VDD18A_CMN`, `_L0`, `_L1` / `PCIE_VDD08A_L0`, `_L1` | 1.71 / 0.76 | 1.8 / 0.8 | 1.89 / 0.84 V |
| LPDDR4 | `DDR_VAA` / `DDR_VDDQ` | 1.71 / 1.06 | 1.8 / 1.1 | 1.89 / 1.17 V |
| ADC12 | `AVDDIO_ADC0`〜`2` / `AVDD_ADC0`〜`2` | 1.71 / 0.76 | 1.8 / 0.8 | 1.89 / 0.84 V |

- デジタル電源とアナログ電源はできるだけ分離。
- **全電源・全 GND 端子を接続すること。開放端子があると動作保証外。**（未使用モジュールの専用電源端子も給電）

### 1.2 電源投入／遮断シーケンス

- **投入順: `0.8 V (VDD08)` → `1.8 V (VDD18, AVDD)` → `1.1 V` → `3.3 V (DDR_VDDQ, VDD33)`。100 ms 以内に完了。**
- **遮断順: `1.1 V` と `3.3 V` を先に → その後 `0.8 V` と `1.8 V`。100 ms 以内。**
- 立ち上がり時間 40 µs 以上、立ち下がり 10 µs 以上。電源電圧・リセット信号は単調変化。負電圧禁止。
- **電源投入中は `RES#` を Low に保持。** `RES#` が High 駆動中は `EXTAL/XTAL` または `EXTCLKIN` に安定クロック供給が必要。

主要タイミング（表 1.2）: `Trisepwr` 40 µs〜30 ms、各段間遅延 0〜100 ms、`Tdlyresetu`（3.3 V 確立 →`RES#`立ち上げ）**10 ms 以上**、`Trisereset` ≤150 µs、`Tdlyresetd`（`RES#`立ち下げ → 3.3 V 立ち下げ）10 µs 以上。

- **1.2.1 GreenPAK 例**: `SLG7RN47598` で各レギュレータ EN を制御。0.8 V→1.8 V→(1.1 V & 3.3 V) の EN を GATE_B/C/D で段階制御。
- **1.2.2 PMIC 例**: 別掲。
- **1.2.3 リセット回路**: `SLG7RN46360V` 系でリセット I/F を生成。`RES#` / `TRST#` / `RSTOUT0#` / `RSTOUT1#` / `RSTdly#`（1 µs typ 遅延）。デバッガの nSRST/nTRST を接続。**リセット IC はオープンドレイン型**。

## 2. 動作モード（[04_boot_modes.md](04_boot_modes.md) と重複）

- `MDn`（MD2:0）で 7 ブートモード。`MDV` で 1.8/3.3 V、`MDD` でハッシュ JTAG 認証、`MDWn` で ATCM wait。
- モード入力電圧: リセット中は `VDD1833_n` ドメインに合わせ `VIH33/VIL33` または `VIH18/VIL18` を満たす。
  - VIH = VDD1833 × 0.7 以上、VIL = VDD1833 × 0.3 以下。
- **モードホールド時間 `tMDH` = 250 ns（`RES#` 解除後、モード端子を保持する時間）。**
- **2.4 注意**: `MDn/MDV/MDD/MDWn` は Ethernet 端子と兼用。RGMII 使用時は高周波なので注意。ストラップ用プルアップ/プルダウン抵抗は **LSI から 1.5 cm 以内**に配置。

## 3. 発振回路

- クロック端子: `EXTAL`(Xin) / `XTAL`(Xout)。EXTAL 入力周波数 **25.00 MHz ±50 ppm**（EtherCAT 時 ±25 ppm）。
- **外部クロック入力時**: `EXTAL` は抵抗（100 kΩ）で VSS へ、`XTAL` はオープン、`XTALSEL` は抵抗で VSS へ（Low）。クロックは `EXTCLKIN` へ入力。
- **水晶振動子接続時**: `EXTCLKIN` は抵抗で VSS へ、`XTALSEL` は抵抗（10 kΩ）で VDD33 へ（High）。
  - 帰還抵抗 `Rif` は内蔵していないため **外付け帰還抵抗 `Rf`（1 MΩ）が必須**。制限抵抗 `Rd` は 0 Ω 推奨（実装不要、水晶特性次第で要る場合あり）。`CL1`/`CL2` は 10 pF が参考値。
- **3.3.1 レイアウト**: 水晶と CL1/CL2 は Xin/Xout 端子直近。クロック配線は水晶周辺回路の GND でシールド（GND 幅 ≥0.3 mm、隣接信号間隔 0.3〜2.0 mm）。水晶周辺 GND と DGND は分離し、LSI 近くの GND と 1 点接続。中間層配線禁止。

## 4. フラッシュメモリ

| I/O ドメイン | 電源ピン | 電圧 |
|-------------|---------|------|
| xSPI0 | `VDD1833_4` | 3.3 V or 1.8 V |
| xSPI1 | `VDD1833_5` | 3.3 V or 1.8 V |

- **4.1 xSPIn x1 ブート**: 起動時 1S-1S-1S でアクセスしシステムソフトウェアリセット。
  - アプリでフラッシュを 1S-1S-1S 以外に変えた場合、LSI 単独リセットではフラッシュが 1S コマンドを受けられず起動不能。
  - **HW リセット付きフラッシュ**: ソフトリセット前に 1S-xx-xx に戻す ＋ システムリセット時にフラッシュも HW リセット（図 4.1、`RESET#` を接続、15 Ω 直列）。
  - **HW リセット無しフラッシュ**: 1S-1S-1S のまま使うか 1S-xx-xx（コマンドは 1S）に留める（図 4.2）。
- **4.2 xSPI0 x8 ブート**: 8D-8D-8D プロファイル 2.0 でアクセス、`XSPI0_RESET0#` で HW リセット。**リードレイテンシサイクル 10 の HyperFlash を選定**。`XSPI0_RESET0#` は **MDD と共通端子**なので、MDD 設定に応じてプル抵抗を付ける（図 4.3 注）。

## 5. Ethernet

| I/O ドメイン | 電源ドメイン | MII/RMII | RGMII |
|-------------|-------------|----------|-------|
| ETH0 | `VDD1833_0` | 3.3 V | 1.8 V |
| ETH1 | `VDD1833_1` | 3.3 V | 1.8 V |
| ETH2 | `VDD1833_2` | 3.3 V | 1.8 V |
| ETH3 | `VDD1833_3` | 3.3 V | 1.8 V |

- MII / RGMII / RMII の 3 モード。各回路例（図 5.1〜5.3）。ダンピング 22 Ω 例。MDC/MDIO は 10 kΩ プルアップ。
- **5.1.1 レイアウト**: `ETHn_TXCLK` と `ETHn_TXD[3:0]`、`ETHn_RXCLK` と `ETHn_RXD[3:0]` は等長配線。
- **5.2 EtherCAT**:
  - PHY アドレスは ESC ポート 0,1,2 の順に連番（`ECATOFFADR` でベース変更可、初期 0）。
  - MAC-PHY I/F は **MII 推奨**（RMII/RGMII は PHY 内部遅延で EtherCAT 精度が落ちる可能性）。
  - `ESC_PHYLINK0/1/2` に PHY の Link LED を接続（`PHYLNK` でアクティブレベル変更、初期 Low アクティブ）。
  - `ESC_RESETOUT#` を PHY リセットへ（初期状態は GPIO モードなので、GPIO 出力で制御後 PinMux 変更）。
  - `ETHn_REFCLK` 25 MHz を PHY の 25 MHz 入力へ。ESC の TXCLK は PHY TXCLK と接続推奨（Automatic TX shift compensation。REFCLK が Main clock 基準なら必須）。
  - EEPROM: `ESC_I2CCLK`/`ESC_I2DATA`。16k ビット超は `ECATOPMOD` 設定変更が必要。
  - LED: `ESC_LEDRUN`/`ESC_LEDERR`/`ESC_LEDSTER`、`ESC_LINKACTn`。

## 6. eMMC / SD

- SDHI0 = eMMC または SD、SDHI1 = SD のみ。
- eMMC は SDHI0 のみ。eSD ブートは SDHI1 に固定割付（変更不可）。
- LVS カードは 3.3 V ⇔ 1.8 V 電源切替対応が必要。同一電源ドメイン（`VDD1833_6`/`_7`）の未使用汎用ポートにも電圧切替が及ぶので注意。
- eMMC の HS200/HS400 対応時は VCCQ=1.8V の eMMC を選定し `VCC1833_6` と VCCQ を 1.8 V に。
- ダンピング 15 Ω 例、プルアップ 47 kΩ/10 kΩ 例。`SDn_PWEN` で SD カード電源 ON/OFF、`SD1_IOVS` で電圧選択、`RSTOUT1#`（オープンドレイン）で電源制御。
- SD0: P12_0〜P12_5 = CLK/CMD/DATA0-3、P12_6〜P13_2 = DATA4-7/RST#（eMMC 8bit 時）。
- SD1: P16_5〜P17_2 = CLK/CMD/DATA0-3。

## 7. PCIe

- エッジコネクタ付きボードは全体厚 1.57 mm（0.062 inch）。
- TX 端子近くに **0.22 µF AC カップリングコンデンサ**。差動は等長。差動インピーダンス TYP 100 Ω(±5%) / シングルエンド 50 Ω(±5%)（または 85 Ω / 42.5 Ω も可）。
- 推奨電源フィルタ（図 7.1）: `PCIE_VDD18A_CMN` / `PCIE_VDD18A_Ln` / `PCIE_VDD08A_Ln` に FB 10 Ω@100 MHz ＋ 4.7 µF/1 µF/0.1 µF。
- `PERST#` / `WAKE#` はオープンドレイン出力 → ボード側でプルアップ必要。REFCLK 49.9 Ω/1%。
- Root Complex / End Point の回路例（図 7.2 / 7.3）。25 MHz リファレンスクロック源（`5V41236` / `RC19202` 等）。

## 8. LPDDR4

別ドキュメント参照:
| ドキュメント | 資料番号 |
|-------------|---------|
| User's Manual: Hardware | R01UH1039JJ**** |
| PCB Verification Guide for LPDDR4 | R01AN7260EJ**** |
| PCB Design Guideline for LPDDR4 | R01AN7268EJ**** |

## 9. USB2.0

- デジタル/アナログ電源プレーン分離。各電源にデカップリング（チップ側 0.1〜2.2 µF、レギュレータ側 10〜47 µF）。
- L/C/R 合計: インダクタンス ≤4 nH、容量 ≤5 pF、抵抗 ≤1 Ω。
- `USB_USVDD33`/`USB_USVDD18`/`USB_USDVDD` を他電源と共用時は FB で分離（推奨: 600 Ω@100 MHz ＋ 47/10/2.2/0.1 µF）。
- `USB_VBUSIN`（30 kΩ±1%）、`USB_TXRTUNE`（200 Ω±1%）の外付けは端子直近。TXRTUNE の外付け抵抗に並列容量禁止、下層 GND。
- DP/DM: 差動 90 Ω(±10%)/シングルエンド 45 Ω(±10%)。等長・平行・等幅・同一層（長さ差 ≤1 mm）。両側 GND シールド、隣接 GND リターンビア。
- 回路例: ホスト（図 9.2、`ISL6186` で VBUS 制御、VBUS 付加容量 ≥120 µF、例 150 µF）、ファンクション（図 9.3）、OTG（図 9.4、外部 OTG トランシーバ）。

## 10〜13. ADC / PLL / OTP / TSU（電源フィルタ）

| モジュール | 端子 | 推奨フィルタ |
|-----------|------|-------------|
| ADC | `AVDDIO_ADCn`, `AVDDREF_ADCn`, `AVDD_ADCn`（n=0-2）| FB 30 Ω@100 MHz ＋ 10 µF/0.1 µF。AN 入力は `AVSSIO_ADCn` でシールド、高速信号と交差禁止 |
| PLL | `VDD18_PLLn`, `VDD08_PLLn`（n=0-4）| FB 30 Ω@100 MHz ＋ 10 µF/0.1 µF |
| OTP | `OTPVDD18`, `OTPVDD08` | 10 µF/0.1 µF（1.8V 側）、0.1 µF（0.8V 側）|
| TSU | `AVDD18A_TSU`, `DVDD08A_TSU` | FB 30 Ω@100 MHz ＋ 10 µF/0.1 µF。配線 L ≤3 nH、R ≤300 mΩ |

## 14. 未使用端子の処理（表 14.1）

| 分類 | 端子 | 処理 |
|------|------|------|
| Clock | `XTAL` | 外部クロック使用時はオープン |
| | `EXTAL` | 外部クロック使用時はプルダウン（VSS） |
| | `EXTCLKIN` | 水晶使用時はプルダウン（VSS） |
| Debug | `TRST#` | プルダウン、または `RES#` と同信号 |
| | `TCK` (P08_3) | プルダウン |
| | `TMS` (P08_1) | プルアップ（VDD33） |
| | `TDI` (P08_2) | プルアップ（VDD33） |
| | `TDO` (P08_4) | オープン |
| System | `RSTOUT#` (P08_5) | オープン |
| | **`MDX`** | **常時 プルダウン（VSS）** |
| | `BSCANP` | プルダウン（VSS）※デバッグ時 Low |
| ADC12 | `AN000`〜`AN003`, `AN100`〜`AN103`, `AN200`〜`AN214` | オープン |
| | `AVDDREF_ADCn` | `AVDDIO_ADCn` に接続 |
| USB | `USB_QDP`, `USB_QDM`, `USB_OTG_ID` | オープン |
| | `USB_VUBUSIN`, `USB_TXRTUNE` | プルダウン or オープン |
| PCIE | `PCIE_REFCLK_*`, `PCIE_RXD*`, `PCIE_TXD*` | オープン |
| DDRSS | `DDR_ZN` | `DDR_VDDQ` に接続 |
| | `DDR_*`（その他） | オープン |
| Other | その他 | オープン / VDD33 プルアップ / VSS プルダウン。`PMm` の対応ビットをリセット後の値「不使用(Hi-Z 入力保護)」に設定 |

- 注: 未使用モジュールはスタンバイ/低電力モードに（リセット解除後この状態。状態変更しないこと）。
- `MDX` は常に未使用処理（プルダウン）。

## 15. その他バイパスコンデンサ

- **I/O 電源**: `VDD1833_n`（n=0-7）各ボール 2 個に対し 0.1 µF を LSI 直近。`VDD33` も同様。
- **コア電源 `VDD08`**: LSI モデル（RZ/N2H 576pin: Cchip 174.49 nF, Rpkg 0.613 mΩ, Lpkg 0.0335 nH / 669pin: Rpkg 0.647 mΩ, Lpkg 0.0272 nH）で、ターゲットインピーダンス（0–3 MHz: 10 mΩ、3–100 MHz: 220 mΩ）を満たすよう配置。参考例: 6800 pF ×1、0.01 µF ×6、0.1 µF ×5。パッケージ下部は小容量優先。
