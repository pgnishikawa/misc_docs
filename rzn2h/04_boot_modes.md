# 動作モード / ブートモード

出典: ユーザーズマニュアル「4. 動作モード」p.187-216、「17.7.6」p.769-770、「45. OTP」。

## 前提

- 本 LSI は**外付けフラッシュメモリからの起動が前提**。7 種のブートモードを MD2:0 で選択。
- セキュリティ対応品は**セキュアブート**（暗号化でユーザープログラムを保護）も選択可。
- ブートは **Cortex-R52 CPU0** が実行 → 外部メモリからローダプログラムを TCM (BTCM) または SYSRAM に展開 → その先頭へ分岐。
  - **セカンドブート CPU** は `BOOTCPU_FLG` で Cortex-R52 CPU0（→BTCM 展開）または Cortex-A55 Core0（→SYSRAM 展開）を選択。

## モード設定端子

### MD2 MD1 MD0（動作モード選択、リセット解除時ラッチ）

| MD2 | MD1 | MD0 | モード | ブートペリフェラル / 使用端子 | 対応電圧 |
|-----|-----|-----|--------|------------------------------|----------|
| L | L | L | xSPI0 ブート（x1 シリアルフラッシュ）| xSPI0 CS0: XSPI0_CKP, XSPI0_CS0#, XSPI0_IO0/1 | 3.3 V or 1.8 V |
| L | L | H | xSPI0 ブート（x8 シリアルフラッシュ / HyperFlash）| xSPI0 CS0: XSPI0_CKP/CKN, CS0#, IO0-7, DS, RESET0# | 3.3 V or 1.8 V |
| L | H | L | xSPI1 ブート（x1 シリアルフラッシュ）| xSPI1 CS0: XSPI1_CKP, CS0#, IO0/1 | 3.3 V or 1.8 V |
| L | H | H | eSD ブート | SDHI1: SD1_CLK, SD1_CMD, SD1_DATA0-3 | 3.3 V |
| H | L | L | eMMC ブート | SDHI0: SD0_CLK, SD0_CMD, SD0_DATA0-7, SD0_RST# | 3.3 V or 1.8 V |
| H | L | H | **SCI (UART) ブート**（フラッシュライタ用）| SCI0: TXD0, RXD0 | — |
| H | H | L | **USB ブート**（フラッシュライタ用）| USBf: USB_QDP, USB_QDM | — |
| H | H | H | 予約（設定禁止）| — | — |

### その他のモード端子（すべてリセット解除時ラッチ、遷移中変更禁止）

| 端子 | 機能 | Low | High |
|------|------|-----|------|
| `MDV` | ブートペリフェラル I/O 電圧 | 1.8 V | 3.3 V。対象は xSPI0(x1,x8)/xSPI1(x1)/eMMC。eSD/SCI/USB は無関係 |
| `MDW0` | CPU0 ATCM wait | 0 wait（CPU 500 MHz 時のみ有効）| 1 wait（500/1000 MHz。**1000 MHz 運用時は High 必須**）|
| `MDW1` | CPU1 ATCM wait | 同上 | 同上 |
| `MDD` | **ハッシュ JTAG 認証**（セキュリティ品でのみ有効。非セキュリティ品では無視）| 通常モード（ハッシュ JTAG 認証 **無効**）| **ハッシュモードによる JTAG 認証** |
| `MDX` | — | **常時 Low 固定** | （禁止）|

- これらの端子は**ストラップオプションとして他の周辺機能端子と兼用**。I/O 電圧は対応する `VDD1833_n` ドメイン電圧。
- 576 ピン FCBGA のボール割付は [08_jtag_bringup_troubleshooting.md](08_jtag_bringup_troubleshooting.md) §4 の表を参照
  （MD0=A22, MD1=B21, MD2=A20, MDV=A15, MDW0=B20, MDW1=A21 は ETH0/ETH1_TX* 兼用 / MDD=AD7 は XSPI0_RESET0# 兼用 / MDX=AC1）。
- 実際のラッチ値は `MD_MON`（`0x8029_4100`）の `MD2MON`/`MD1MON`/`MD0MON`/`MDVMON`/`MDW1MON`/`MDW0MON`/`MDDMON` で確認可。`MDP` bit8: 0=RZ/T2H 729ピン、1=RZ/N2H 576/669ピン。`MD_MON` は RES# デアサート時にラッチ。

## ブート処理フロー（4.6.1）

1. MD2:0 で指定されたブートペリフェラル（xSPI / SDHI）を設定。
2. 外部メモリから**ローダ用パラメータ**を読み出し、`CHECK_SUM` を検証。
3. ローダ用パラメータに従いブートペリフェラルを高速化設定。
4. 外部メモリから**ローダプログラム**を読み出し。
5. セカンドブート CPU が A55 の場合、A55 Core0 のリセット解除・R52 CPU0 は WFI へ。セカンドブート CPU がローダプログラム先頭へ分岐。

- SCI/USB ブートは (1)〜(4) の代わりにホスト PC から S-record（S0/S3/S7）でローダプログラムをダウンロード。

## ローダ用パラメータ（xSPI ブート時、オフセット）

| オフセット | パラメータ | 内容 |
|-----------|-----------|------|
| `0x00` | `CACHE_FLG` | `0x1` で R52 I1/D1 キャッシュ有効（高速化） |
| `0x04` | `WRAPCFG_V` | xSPI WRAPCFG 設定値（bit13 フラグ）|
| `0x08` | `COMCFG_V` | xSPI COMCFG 設定値 |
| `0x0C` | `BMCFG_V` | xSPI BMCFG.PREEN |
| `0x10` | `RESTORE_FLG` | `0x2236_0679` でブート後に xSPI/SDHI 設定を初期値へ戻す |
| `0x14` | `LDR_ADDR_NML` | 外部メモリ内ローダプログラム先頭番地（xSPI0: `0x4000_004C`〜`0x40FF_FE00`、xSPI1: `0x5000_004C`〜`0x50FF_FE00`）|
| `0x18` | `LDR_SIZE_NML` | ローダプログラムサイズ（**512 バイトの倍数**。最大 BTCM 52 KB / SYSRAM 2036 KB）|
| `0x1C` | `DEST_ADDR_NML` | 展開先（R52 CPU0: BTCM `0x0010_2000`〜`0x0010_EFFF` / A55 Core0: SYSRAM `0x1000_0000`〜`0x101F_CFFF`。4 バイトアライン。Thumb なら bit0=1）|
| `0x28` | `CS0_SIZE` | xSPI CS0 デバイスサイズ（< `0x0800_0000`）|
| `0x2C` | `LIOCFGCS0_V` | xSPI LIOCFGCS0（bit14 フラグ）|
| `0x30` | `PLL0_SSC_CTR_V` | A55 セカンドブート時の PLL0 SSC 設定 |
| `0x34` | `BOOTCPU_FLG` | `0`=Cortex-R52 CPU0、`1`=Cortex-A55 Core0 |
| `0x44` | `ACCESS_SPEED` | 外部メモリアクセス速度（SCKCR の FSELXSPIn/DIVSELXSPIn）|
| `0x48` | `CHECK_SUM` | オフセット `0x00`〜`0x44` の各値を上位/下位 16bit に分け合算（unsigned long）|

- eSD/eMMC ブート時は `0x04`〜`0x0C` は予約（0）。eSD ブートはセクタ 1〜7 にローダ用パラメータ多重化、セクタ 8 以降にローダプログラム。eMMC はセクタ 1 にパラメータ、セクタ 2 以降にプログラム。

## SCI ブート（MD=101b）

- CPU0CLK 500 MHz、調歩同期、8N1、**115200 bps**。
- 手順: 電源投入 → LSI が `SCI Download mode (Normal SCI boot)` を送出 → PC から S0「HDR NM」→ LSI `--- Load Program to RAM ---` → PC が S3 でローダプログラム → S7 で終了アドレス → LSI `--- Start Boot Program on RAM ---` → BTCM/SYSRAM で実行。
- SCI ブート端子（17.7.6.3）: **RXD0 = P27_4、TXD0 = P27_5**。
- エラー条件: SCI 通信エラー / S0,S3,S7 以外のレコード / チェックサムエラー / BTCM,SYSRAM 領域外アドレス / サイズ超過 / 認証失敗。

## USB ブート（MD=110b）

- CPU0CLK 500 MHz、USB 高速・ファンクション・**CDC**。
- 手順: 電源投入 → エニュメレーション → PC から S0「OPEN」→ LSI が `USB Download mode (Normal USB boot)` → 以降 SCI と同様（S0 HDR NM → S3 → S7）。
- 端子: USB_QDP / USB_QDM（`USB_VBUSIN` はブートに未使用）。
- 注意: ローダプログラム実行までは USB ケーブルを抜かない。

## 認証（セキュリティ機構）

### JTAG デバッグ認証（OTP `AUTHMODEJ`, OTP addr 0x0E9）
- `B2B1B0`（各 3bit の OR）= `000`:認証なし / `001`:レベル1 / `01x`:レベル2 / `1xx`:JTAG 永久禁止。
- レベル1/2 は 128bit ID を OCD `OIRn` に書き、OTP の `JID1P`(0x0EB-0x0EE) / `JID2P`(0x0EF-0x0F2) と比較。詳細は [07_debug_interface.md](07_debug_interface.md)。
- **セキュリティ品はハッシュ認証。`MDD` 端子でハッシュ JTAG 認証を有効化。**

### SCI/USB ブート認証（OTP `AUTHMODES`, OTP addr 0x1CB）
- `B1B0` = `00`:認証なし / `01`:認証（`SIDP` 0x0F3-0x0F6 に 128bit ID）/ `1x`:SCI/USB ブート永久禁止。
- 認証時は "SCI/USB authentication boot (plaintext)" フローで S0「HDR PL」→ 16 バイトパスワード → OTP ID と検証。

### OTP メモリマップ（非セキュリティ品、45.4 表 45.4 抜粋）

| 領域 | ビット幅 | OTP アドレス | R/W |
|------|---------|-------------|-----|
| WORDLOCK 制御領域 | 1024 | 0x001–0x01F | R/W |
| ユニーク ID | 64 | 0x0E5–0x0E6 | R |
| JTAG 認証モード (AUTHMODEJ) | 9 | 0x0E9 | R/W |
| JTAG 認証レベル1 平文 ID (JID1P) | 128 | 0x0EB–0x0EE | W |
| JTAG 認証レベル2 平文 ID (JID2P) | 128 | 0x0EF–0x0F2 | W |
| SCI/USB ブート認証モード (AUTHMODES) | 6 | 0x1CB | R/W |
| SCI/USB ブート認証平文 ID (SIDP) | 128 | 0x0F3–0x0F6 | W |
| アンチロールバックカウンタ | 320 | 0x1B1–0x1BA | R/W |
| 型名 | 32 | 0x1D9 | R |
| 製品バージョン | 32 | 0x1DA | R |
| 温度センサコード High/Low | 12/12 | 0x1DC / 0x1DD | R |
| ユーザー領域 | 16256 | 0x204–0x3FF | R/W |

- OTP は 32bit 幅・1024 ワード。**未書き込みビット = 0、書き込み済み = 1、上書き不可**。
- OTP アクセスはメモリマップされず専用アドレス（0x000–0x3FF）＋レジスタ I/F（`OTP = 0x810C_0000`: `OTPPWR`/`OTPSTR`/`OTPSTAWR`/`OTPADRWR`/`OTPDATAWR`/`OTPADRRD`/`OTPDATARD`）。

## 使用上の注意（4.7）

- **4.7.1 分割 I/O ドメイン**: イーサ／xSPI／SDHI 用に `VDD1833_n` で 1.8/3.3 V 対応。I/O バッファは印加電圧を自動検出するが、**ブートペリフェラルの I/O 電圧はブート ROM が最適化できるよう `MDV` で表示が必要**。
  - VDD1833_0-3 = ETH0-3、VDD1833_4 = xSPI0、VDD1833_5 = xSPI1、VDD1833_6 = SDHI0（SD/eMMC）、VDD1833_7 = SDHI1（SD）。RGMII は 1.8 V のみ、MII/RMII は 3.3 V のみ。
- **4.7.2 eSD ブート後リセット**: 3.3 V→1.8 V に変えた後にリセットする場合、SD 初期化のため電源サイクル（1.8V→Off→3.3V）が必要。
- **4.7.3 xSPI x1 ブートのシリアルフラッシュリセット**: フラッシュ構成を 1S-1S-1S 以外に変えた後リセットが入るとブート不能 → フラッシュへの外部 HW リセットが必要。
