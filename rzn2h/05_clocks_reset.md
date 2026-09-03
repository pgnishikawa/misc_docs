# クロック発生回路 / リセット

出典: ユーザーズマニュアル「6. リセット」p.221-235、「7. クロック発生回路」p.236-262、「8. CLMA」p.263-270。

## リセット

### リセット要因（表 6.1）

| リセット名 | 要因 |
|-----------|------|
| RES# 端子リセット | RES# 端子に Low |
| システムソフトウェアリセット | `SWRSYS` レジスタ設定 |
| Cortex-A55 クラスタソフトウェアリセット | `SWR55C` |
| Cortex-A55 Core0/1/2/3 ソフトウェアリセット | `SWR550`/`SWR551`/`SWR552`/`SWR553`（電源投入後 1 回だけ解除）|
| Cortex-R52 CPU0/CPU1 ソフトウェアリセット | `SWRCPU0` / `SWRCPU1` |
| エラーリセット | ICU からのリセット要求 |

- RES# 端子リセットは「コールド（電源投入）」と「ウォーム（ウォッチドッグ等）」で初期化対象が少し異なる。
- **動作モード（MD2:0 のラッチ）は、端子リセット状態（RES# と TRST# の両方が Low）が解除されたときに確定**（表 6.2 注1）。ソフトウェアリセットやエラーリセットでは動作モードは再ラッチされず、前回 RES# 解除時のモードを保持（注2）。
- リセット状態フラグ: `RSTSR0`（`.TRF` RES#端子、`.SWRSF` システムSW、`.SWR55C/550..553`、`.SWR0F/1F` R52、`.ERRF` エラー）。
- `RSTOUT#` 端子は各リセット中 Low 出力（期間は 6.4.9）。
- 関連レジスタは書き込み保護対象（`PRCRS.PRC1`: `SWRSYS`, `SWRCPU0/1`, `SWR55C`, `SWR550-553`, `MRCTLI` 等 / `PRCRN.PRC1`: `RSTSR0`, `MRCTLA/E/M` 等）。→ [07 参照は不要、11 章]

### リセット関連端子

| 端子 | I/O | 機能 |
|------|-----|------|
| `RES#` | 入力 | リセット信号入力。Low でリセット状態 |
| `TRST#` | 入力 | オンチップエミュレータ用テストリセット。`RES#` と両方 Low で TAP＋デバッグ回路リセット |
| `RSTOUT#` | 出力（P08_5 兼用）| 外部リセット信号出力 |

## クロック発生回路 (CGC)

### 発振源

| 項目 | 仕様 |
|------|------|
| メインクロック発振器 (MOSC) | 発振子 / 外部クロック入力 **25 MHz**（EtherCAT 使用時は ±25 ppm、通常 ±50 ppm）|
| 接続端子 | 発振子: `EXTAL` / `XTAL`（`XTALSEL = 1`）、外部クロック: `EXTCLKIN`（`XTALSEL = 0`）|
| 低速オンチップオシレータ (LOCO) | 1 MHz ±20% |
| JTAG 用外部クロック (`TCK`) | 最高 50 MHz |
| 発振停止検出 | `CLMA6` がメインクロック異常検出 → システムクロック源を LOCO へ切替、各 PLL 出力は LOCO/フリーラン、MTU3/GPT 端子 Hi-Z |

### PLL

| PLL | 入力 | 逓倍 | 出力 | SSC | 用途 | 異常検出 |
|-----|------|------|------|-----|------|----------|
| PLL0 | MOSC 25 MHz | ×48 | **1200 MHz** | SSC/非SSC 選択可 | Cortex-A55 CPU クロック | CLMA0 |
| PLL1 | MOSC 25 MHz | ×40 | **1000 MHz** | 非SSC 固定 | Cortex-R52 CPU、メインバス R、周辺クロック | CLMA1 |
| PLL2 | MOSC 25 MHz | ×32 | **800 MHz** | SSC/非SSC 選択可 | DDRSS、SDHI クロック | CLMA2 |
| PLL3 | PLL4 の 1/50（48 MHz）| 設定可 | 25〜430 MHz | 非SSC 固定 | LCDC クロック | CLMA3 |
| PLL4 | MOSC 25 MHz | ×96 | **2400 MHz** | 非SSC 固定 | メインバス A、周辺クロック | CLMA4 |

- 異常検出時は該当 PLL 出力が MOSC に切替、PLL0/1/4 では MTU3/GPT 端子が Hi-Z。

### 主な内部クロック（表 7.2 抜粋、周波数はリセット値 → 最大）

| クロック | ソース | 供給先 | 周波数 |
|----------|--------|--------|--------|
| CA55 Core0-3 (CA55CnCLK) | PLL0 ÷1 or ÷2 | Cortex-A55 各コア | 600 / **1200** MHz |
| CA55 DSU (CA55SCLK) | PLL1 ÷1 or ÷2 | A55 DSU | 500 / **1000** MHz |
| CR52 CPU0/1 (CR52CnCLK) | PLL1 ÷1 or ÷2 | Cortex-R52 各 CPU | 500 / **1000** MHz |
| PCLKAH | PLL4 ÷6 | メインバス A、PCIE、LCDC(clk_a)、GMAC1/2(ACLK)、DDRSS | **400 MHz** |
| PCLKAM | PLL4 ÷12 | USB、GMAC1/2(HCLK)、SDHI(ACLK,IMCLK) | **200 MHz** |
| PCLKAL | PLL4 ÷24 | LCDC(clk_p) | **100 MHz** |
| PCLKH | PLL1 ÷4 | メインバス R、SYSRAM、LLPP、MTU3、GPT、TFU、DSMIF(バス)、xSPI(バス)、DMAC、MPU、CANFD(ramclk)、GMAC0(ACLK)、SHOSTIF、ELC、RSIP、MBXSEM | **250 MHz** |
| PCLKM | PLL1 ÷8 | CRC、SCI/SPI(バス)、GPT(バス)、CANFD(バス)、ETHSW(バス)、GMAC0(HCLK)、OTP、GPIO、**CLMA**、**ICU** | **125 MHz** |
| PCLKL | PLL1 ÷16 | POEG(バス)、IIC、DOC、CMT、CMTW、WDT、RTC、TSU、DDRSS(PHY_reg) | **62.5 MHz** |
| PCLKADC | PLL1 ÷16 | ADC12（変換）| 50 MHz（リセット値 62.5）|
| PCLKGPTL | PLL1 ÷2 | GPT（LLPP 動作）| **500 MHz** |
| BSC_CLK / CKIO | PLL1 ÷8〜÷32 | 外部バス | 125 / 83.3 / 62.5 / 50 / … MHz（リセット値 50）|
| SDHI_clkhs | PLL2 | SDHI(clk_hs) | 800 MHz |
| DFICLK | PLL2 | DDRSS | 800 MHz |
| XSPI_CLKn | PLL4 ÷18〜÷192 | xSPIn（動作）| 133 / 100 / 75 / 50 / 37.5 / 25 / **12.5**(リセット値) MHz |
| USB_CLK | PLL4 ÷48 | USB | 50 MHz |
| ETCLKA/B/C/D/E | PLL1 ÷5/÷8/÷10/÷20/÷40 | ETHSW / GMAC・RGMII / ESC / RGMII-RMII conv / ESC・conv | 200 / 125 / 100 / 50 / 25 MHz |
| ETHn_REFCLK | PLL1 ÷40 or MOSC | 外部 Ether PHY | 25 MHz |
| RMIIn_REFCLK | PLL1 ÷20 | 外部 Ether PHY | 50 MHz |
| PCLKRTC | MOSC ÷128 | RTC | 195.3 kHz |
| PCLKENDAT / PCLKBISS / PCLKHDSL / PCLKAFMT | PLL4 ÷24 / PLL4 ÷30 / PLL1 ÷32 / PLL1 ÷30 | エンコーダ I/F | 100 / 80 / 75 / 80 MHz |
| LOCO | — | — | 1 MHz |

### クロック関連端子

| 端子 | I/O | 機能 |
|------|-----|------|
| `XTAL` | 出力 | 水晶振動子接続。外部クロック時は `EXTAL` を Low に |
| `EXTAL` | 入力 | 水晶振動子接続 / 外部クロック時 Low。電圧が VDD18(1.8V) を超えないこと |
| `EXTCLKIN` | 入力 | 外部クロック入力。水晶振動子接続時は Low |
| `XTALSEL` | 入力 | メインクロック源選択（Low=EXTCLKIN、High=XTAL/EXTAL）|
| `CKIO` | 出力 | 外部バスクロック出力 |

- 書き込み保護: `PRCRN.PRC0`（`SCKCR`, `SCKCR3`, `SCKCR4`）/ `PRCRS.PRC0`（`SCKCR2`, `PMSEL`, `PLL0EN`, `PLL0_SSC_CTR`, `LOCOCR`, `PLL2EN`, `PLL2_SSC_CTR`, `PLL3EN`, `PLL3_VCO_CTR0/1`）。

## クロックモニタ回路 (CLMA)（8 章）

- 7 ユニット（CLMA0〜6）。入力クロック（MOSC）/ PLL / LOCO の出力周波数異常を監視。
- CLMA6: メインクロック発振停止検出 → LOCO へフォールバック。
- サンプリングクロック: CLMA0/1/2/4 = MOSC÷2（12.5 MHz）、CLMA3 = MOSC÷16、CLMA5 = MOSC÷256。
- モニタクロック: CLMA0=PLL0÷16(75MHz)、CLMA1=PLL1÷16(62.5MHz)、CLMA2=PLL2÷16(50MHz)、CLMA3=PLL3÷4、CLMA4=PLL4÷32(75MHz)、CLMA5=LOCO(1MHz)、CLMA6=MOSC÷2。
