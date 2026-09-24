# RZ/N2H xSPI1 への Everspin MRAM (EM064LXOAB320IS2T) 8D-8D-8D 接続

[← README（目次）へ戻る](README.md)

出典: RZ/N2Hユーザーズマニュアル R01UH1039JJ0130「37. 拡張シリアルペリフェラルインタフェース
(xSPI)」p.2747-2805、[13_xspi_protocol_modes_and_quad_flash.md](13_xspi_protocol_modes_and_quad_flash.md)、
[09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md)。
MRAM側は `MRAM/EMxxxLX_B_HR Datasheet v3.7_0.pdf`（Everspin Technologies、以下「MRAM DS」）。

対象デバイス: **EM064LXOAB320IS2T**（Everspin、64Mb、Octal SPI、24-BGA、産業用温度グレード
-40〜+85℃、MRAM DS §23 Orderable Part Numbers に掲載の正規品番と一致確認済み）を
RZ/N2H の **xSPI1** に接続し、**8D-8D-8D（Octal DTR）**で読み書きする。

## 結論（要約）

**可能。** RZ/N2H の xSPI（RZ/N2L と同一IPブロック、[13](13_xspi_protocol_modes_and_quad_flash.md)参照）は
8D-8D-8D を含む7種類のプロトコルモードをネイティブサポートしており、このMRAMのxSPIフレーム
構造（コマンド→アドレス→レイテンシ→データ、かつDTRでは1バイトのコマンドを2バイト分
繰り返し送出する方式）は、RZ/N2Hの **「8D-8D-8Dプロファイル1.0」フォーマット
（`CMCFG0CSn.FFMT=01`）と正確に一致**する。さらにこのMRAMは：

- **NORフラッシュのような消去（Erase）が不要**（persistent memory mode、MRAM DS既定値）
- **Write Enable (WREN) は最初の1回だけでよく、以後の書き込みでWELが自動クリアされない**
  （MRAM DS §11、通常のSPI NORとは異なる特有の仕様）

という2点により、[09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) §2.5 で述べた
「**NORフラッシュはメモリマッピングモードでの書き込みができない**」という制約が
**このMRAMには当てはまらない**。つまり **読み出しだけでなく書き込みも、単純なメモリ
マッピングモードでの読み書き（memcpy相当）がそのまま使える**、という結論になる
（詳細は §4）。

## 1. 対象デバイスの確認（MRAM DS §23）

MRAM DS の Orderable Part Numbers 表（Density | IO | Package | Temperature | Pack/Ship | OPN）に
以下の記載を確認した：

| Density | IO | Package | Temperature | OPN |
|---|---|---|---|---|
| 64Mb | Octal | 24-BGA | Industrial | **EM064LXOAB320IS2T** |

- **64Mb = 8MB** の密度
- **Octal**（IO0-7 の8本のデータ線）専用品番（Quad専用品番とは別系統。データシート注記
  「Octal configurations in Quad SPI only devices are not valid」＝Quad専用品ではOctalモード
  自体が無効になるため、Octal品番を選んでいることは正しい前提）
- **単一 1.8V 電源**（MRAM DS 表紙：「1.8V, 200MHz Octal SPI interface」、以下電圧の節も参照）
- 産業用温度グレード（-40℃〜+85℃）

## 2. MRAM の基本特性（一般的なSPI NORフラッシュとの違い）

MRAM DS の記述に基づく（§1 Device Description、§11 Write Operations）：

- **STT-MRAM技術**により、**個々のビットを1回の書き込みで1↔0どちらにも直接変更可能**。
  NOR/NANDフラッシュのような「事前の消去（ブロック/セクタ単位）」が不要。
- **Persistent Memory Mode**（Nonvolatile Configuration Register 8 bit0=1、**工場出荷時の
  既定値**）を選択すると、任意アドレス・任意バイト数を **SRAM的に直接読み書き**できる
  （ページ境界の制約なし。書き込みアドレスがメモリ末尾に達すると先頭へラップする）。
  対して bit0=0（NOR Flash Emulation mode）を選ぶと、レガシーソフトウェア互換のため
  256バイトページ制限・消去コマンドを使うNOR風の挙動になる。**本用途では既定値の
  Persistent Memory Mode をそのまま使う**（§5で再確認）。
- **書き込み速度**：MRAM DS は明示的な「Page Program time (tPP)」を電気特性表に**掲載して
  いない**（NORフラッシュなら通常大きな値で必ず記載される項目）。代わりに「delivers up to
  400MBytes/S reads and writes」「2000x increase in write/program performance...vs NOR
  Flash」「Writes occur very fast」という記述がある。これは書き込みがバストランザクション
  自体の中で完結し、追加の内部プログラム待ち時間をほぼ要しないことを示唆している
  （ただし正式な最大レイテンシ値としての明記はないため、§7「要検証事項」で扱う）。
- **書き込みイネーブル（WEL）の挙動が特有**：MRAM DS §11 に明記：
  > Write (Program) commands will not reset WEL at the completion of the command,
  > allowing for back-to-back writes to memory without loading the WREN command again.
  
  つまり **WREN (0x06) を1回発行すれば、以後の書き込みコマンドはWELを消費しない**
  （WELは Write Disable (0x04) コマンドか、リセット／電源再投入でのみクリアされる）。

## 3. プロトコル互換性の分析

### 3.1 MRAMのxSPIフレーム構造（MRAM DS §4）

MRAM DS は明確に「コマンド → コマンドモディファイア(アドレス) → 初期アクセスレイテンシ →
データ」の4フェーズ構造を規定しており、Cypress/Infineon HyperBus系デバイス特有の
「CA（Command-Address結合48bitフィールド）+ RWDS」方式**ではない**（`HyperBus`/`RWDS`/`CA[47`
等のキーワードはMRAM DS中に一切登場しない。代わりに`DS`という名称の単純なデータ
ストローブ出力がある点はxSPI標準そのもの）。

> 8D-8D-8D means that the command, the command modifier, and data transfers are always
> 8 bits wide DTR. The EMxxxLX allows the option to repeat the command opcode which makes
> an 8D command look like an 8S command, but **it is required to repeat the command opcode**.

これは RZ/N2H の **「8D-8D-8Dプロファイル1.0」フォーマット**の仕様
（[13](13_xspi_protocol_modes_and_quad_flash.md)、`CMCFG0CSn.FFMT=01`：「コマンド2バイト」、
`CDTBUFn.CMD[15:0]`の説明「CMD[15:8]はコマンドフィールドです。CMD[7:0]は拡張フィールド」）と
**正確に一致**する。すなわち：

**`CMD[15:0]` には同じオペコードを上位・下位バイトの両方に設定する**（例: Read Fast の
オペコード `0x0C` なら `CMD[15:0] = 0x0C0C`）。

### 3.2 使用するオペコード（MRAM DS Table 21、OSPI-DTR列が `8d-8d-8d` の行を抜粋）

| 操作 | オペコード | アドレスバイト数 | レイテンシ(ダミー) | 備考 |
|---|---|---|---|---|
| **Read Fast**（4バイトアドレス） | `0x0C` | 4（強制、注1） | DCC（設定可能） | 現在の動作モードに応じて自動的に形が変わる汎用フ​ァストリード |
| Read Fast Octal I/O（4バイトアドレス） | `0xCC` | 4（強制） | DCC | Octal専用の明示的オペコード |
| **Write (Program) 4-byte address** | `0x12` | 4（強制） | 0（ダミーなし） | 汎用ライト（現在のモードに追従） |
| Write (Program) Fast Octal Input 4-byte | `0x84` | 4（強制） | 0 | Octal専用の明示的オペコード |
| Write Enable | `0x06` | 0 | 0 | **初回のみ発行すればよい**（§2参照） |
| Write Nonvolatile Configuration Register | `0xB1` | 0 | 0 | 初期設定（§5） |
| Write Volatile Configuration Register | `0x81` | 0 | 0 | 初期設定（§5、電源投入毎に必要な場合） |
| Read Status Register | `0x05` | 0 | 8サイクル | WIP（Write In Progress）ビット確認用（任意） |

（注1）MRAM DS 表21 Note 5: 「Octal SPI with DTR operations or commands **all require
4-byte address input**. 4-byte addressing does not need to be enabled.」— Octal DTR
コマンドは常に4バイトアドレスになる仕様のため、`CMCFG0CSn.ADDSIZE` は常に **4バイト（`11b`）**
に設定する。

### 3.3 RZ/N2H 側のレジスタ設定（xSPI1 = ユニット m=1）

ベースアドレス: `XSPI1 = 0x801C_0000 + 0x1000 × 1 = 0x801D_0000`

| レジスタ | オフセット（ユニット内） | 設定値 | 意味 |
|---|---|---|---|
| `LIOCFGCS0`（xSPI1のCS0） | `0x050` | `PRTMD[9:0] = 0x3FF` | 8D-8D-8D |
| `CMCFG0CS0` | `0x010` | `FFMT[1:0] = 01b`、`ADDSIZE[1:0] = 11b`（4バイト） | プロファイル1.0、4バイトアドレス |
| `CMCFG1CS0`（読み出し） | `0x014` | `RDCMD[15:0] = 0x0C0C`（または`0xCCCC`）、`RDLATE[4:0] = 16`（MRAM側ダミーサイクル既定値と一致、§5） | Read Fast 4バイトアドレス |
| `CMCFG2CS0`（書き込み） | `0x018` | `WRCMD[15:0] = 0x1212`（または`0x8484`）、`WRLATE[4:0] = 0` | Write 4バイトアドレス、ダミーなし |

`LATEMD`（`LIOCFGCSn` bit10）は既定の0（設定変更可能なレイテンシ、通常フォーマット/
プロファイル1.0向けの標準的な挙動）のままでよい（プロファイル2.0固有の6バイトコマンド
フィールド専用ビットのため）。

## 4. 書き込みについて（★このMRAM特有の優位点）

[09](09_xspi0_x1_boot_and_runtime.md) §2.5 で述べた通り、
RZ/N2Hのメモリマッピングモードの書き込みは「1回のAXI書き込みアクセス→固定1コマンドの
xSPI書き込みトランザクション」という単純な対応にしかならず、**通常のSPI NORフラッシュ**
では以下3点が理由でメモリマッピング書き込みができなかった：

1. Write Enable (WREN) を**毎回**前置する必要がある
2. **イレース**が必要
3. 完了までの**ステータスポーリング**が必要

このMRAMでは：

1. **WRENは最初の1回だけ**でよく（§2）、以後の書き込みはWEL消費なしで継続できる →
   **起動時にマニュアルコマンドモードで`0x06`を1回発行しておけば、以降はメモリマッピング
   書き込みの「固定1コマンド」だけで正しく書き込める**
2. **Persistent Memory Modeではイレース不要**（§2）→ 制約(2)自体が存在しない
3. 書き込み完了時間の扱いは §7 で要検証だが、明示的なtPPが存在しない点・
   「very fast」という記述から、**追加のステータスポーリングなしでも実用上問題ない
   可能性が高い**（確実性を求めるなら、クリティカルな書き込み直後だけマニュアルコマンド
   モードで`0x05`（Read Status Register）のWIPビットを確認する設計にしてもよい）

結果として、**読み出しだけでなく書き込みも、単純なメモリマッピングモードでの
読み書きがそのまま使える**、という一般のSPI NORフラッシュには無い優位性がある。

## 5. 一度だけ必要な初期設定シーケンス

MRAMは電源投入直後、既定では **SPI (1S-1S-1S) with DS** モードで起動する
（Nonvolatile Configuration Register 0 既定値 `0xFF`）。8D-8D-8Dで運用するには、
**MRAM自身にOctal DTRモードへの切り替えを指示**し、**RZ/N2H側の`LIOCFGCSn`等も
それに合わせて切り替える**必要がある。2つの方式がある：

### 方式(a) 実行時に毎回切り替える（Volatile Configuration Register を使用）

1. RZ/N2HのxSpi1を **1S-1S-1S**（`PRTMD=0x000`、リセット直後の既定モードに近い設定）で
   初期化し、マニュアルコマンドモードで以下を発行:
   - `0x81`（Write Volatile Configuration Register 0）で I/O Mode を
     `1110_0111`(`0xE7`, Octal DTR with DS) または `1100_0111`(`0xC7`, Octal DTR w/o DS) に設定
   - 必要なら Register 1（Dummy Cycle Configuration）・Register 5（Address Mode）も確認・設定
   - `0x06`（Write Enable）を1回発行
2. RZ/N2H側の`LIOCFGCSn`/`CMCFG0/1/2CSn`を§3.3の値に切り替える（切り替え前に通信停止手順、
   [13](13_xspi_protocol_modes_and_quad_flash.md) §3を参照）
3. 以降、メモリマッピングモードで8D-8D-8D読み書きが可能

Volatile設定は**電源断で失われる**ため、電源投入のたびに手順1を繰り返す。

### 方式(b) 工場出荷時に不揮発設定してOctal DTRで起動させる（Nonvolatile Configuration Register）

1. 量産前の1回だけ、MRAMを1S-1S-1Sで初期化し `0xB1`（Write Nonvolatile Configuration
   Register 0）で I/O Mode を `0xE7`/`0xC7` に書き込む（さらに Register 8 bit0 を明示的に
   `1`=Persistent Memory Modeに設定しておくと安心。既定値と同じだが明示が望ましい）
2. 以後の**電源投入では、MRAMは最初からOctal DTRモードで起動する**ため、RZ/N2H側も
   リセット直後から`LIOCFGCSn`を8D-8D-8D設定にしてよい（起動時の1S-1S-1Sでのやり取りが
   不要になる）
3. WEL（Write Enable）は電源投入毎にクリアされるため、**`0x06`は毎回の起動時に1回だけ
   発行する**（これは方式(a)/(b)共通で必要）

**方式(b)の方が起動シーケンスが単純**になるため、量産用途では方式(b)を推奨する
（[ET1100/docs/06_implementation_roadmap.md](../../ET1100/docs/06_implementation_roadmap.md)で
扱ったET1100 EEPROMの「工場出荷時に一度だけ確定させる」という考え方と同種のパターン）。

## 6. 信号・電圧面の確認

| 項目 | MRAM側（MRAM DS） | RZ/N2H xSPI1側 | 判定 |
|---|---|---|---|
| データ線 | IO0-7（8本） | `XSPI1_IO0`-`XSPI1_IO7`（P01_4～P01_7, P02_0～P02_3） | ○ |
| クロック | **CK（単相、1本のみ）**。24-BGAのボール表に相補クロック(CK#)は存在しない | `XSPI1_CKP`（P01_0）のみ | **○（好都合な一致）**。RZ/N2Hマニュアル表37.2 注1で確認：`XSPIm_CKN`は**ユニット0（xSPI0）専用ピンであり、xSPI1には物理的に存在しない**。つまりxSPI1は最初から単相クロック出力のみで、CKN未接続を気にする必要すらない（旧版の本ドキュメントは誤ってCKNをxSPI1にも存在するものとして記載していたため訂正） |
| データストローブ | `DS`（出力） | `XSPI1_DS`（P01_3） | ○ |
| チップセレクト | `CS#` | `XSPI1_CS0#`（P01_1） | ○ |
| リセット | `RESET#`（入力、BGAでは専用ピン。**外部プルアップ推奨**とMRAM DSに明記） | **専用リセット出力ピンなし** | **要注意（訂正）**：`XSPIm_RESET0#`も表37.2 注1により**ユニット0専用**で、xSPI1には存在しない。x8ブート時（[09](09_xspi0_x1_boot_and_runtime.md) §1.1）と同じ配線パターンは**流用できない**。MRAM側の`RESET#`はソフトウェア／xSPIコントローラからは制御できないため、**MRAM DS推奨の外部プルアップのみに依存する**か、汎用GPIOで手動制御する設計にすること |
| 電源電圧 | **単一1.8V**（3.3V動作の記載なし） | `VCC1833_5`（xSPI1用ドメイン、P01/P02は`VDD1833_5`ドメインと表17.6/17.7で確認）を**1.8Vに設定** | **1.8V固定が必須**（3.3Vでは動作しない可能性が高い。RZ/N2H側は1.8V/3.3V選択可能なドメインなので、確実に1.8V側を選ぶこと） |
| 端子多重化 | — | `XSPI1_*`は`P01_0`～`P01_7`、`P02_0`～`P02_3`と**汎用GPIO／他周辺機能（MTU3, GTIOC, ENCIF等）との共用ピン**（PFC機能コード`0x1C`）。専用ブートピンではないため**PMCm/PFCm設定が別途必須** | 詳細手順は[15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md) §2 |

## 7. 要検証事項（本ドキュメントだけでは断定できない点）

- **書き込み完了レイテンシの正式値**: MRAM DSの電気特性表に明示的な「Page Program time」
  記載がなく、「非常に高速」という定性的表現のみ。メモリマッピングモードでの無条件
  連続書き込みが実運用上安全かどうかは、Everspinへの直接確認、または実機での
  書き込み直後リードバック検証を推奨する。
- ~~`XSPI1_CKN`を未接続のままにしてよいか~~ → **解決済み**：RZ/N2Hマニュアル表37.2 注1により
  `XSPIm_CKN`はユニット0専用ピンで、xSPI1には端子自体が存在しない。CKN配線・未接続の
  判断は不要（§6参照、旧記載を訂正済み）。
- **MRAM `RESET#`ピンの扱い**: `XSPI1_RESET0#`が存在しないため、電源投入シーケンスや
  ソフトリセットからのMRAM再初期化をRZ/N2H側から能動的に行う手段がない
  （外部プルアップのみ、または汎用GPIOでの手動制御が必要）。基板設計側で
  この制約を踏まえた回路を検討すること。
- **RZ/N2H側で実際に到達可能な最大クロック周波数**: xSPI仕様上の理論値は266MB/s
  （RAW、[13](13_xspi_protocol_modes_and_quad_flash.md)参照）で、8D-8D-8Dでは1クロックあたり
  2バイト転送のため換算で概ね133MHz相当。MRAM自体は200MHzまで対応するため、
  **ボトルネックはRZ/N2H側**になる。正確に設定可能なクロック分周比はクロック発生回路
  （SCKCR、[05_clocks_reset.md](05_clocks_reset.md)）の分周設定を確認すること。
- **CRC/自動キャリブレーション機能の要否**: MRAM DSはCRCコマンド（§20）を持つが、
  本ドキュメントでは初期の read/write 可否判断を主眼としたため詳細検討していない。

## 8. まとめチェックリスト

- [ ] `VCC1833_5`（xSPI1用I/Oドメイン）を **1.8V** に設定した
- [ ] `XSPI1_CKP`をMRAMの`CK`へ接続した（`CKN`は端子自体が存在しないため配線不要）
- [ ] MRAM側`RESET#`に外部プルアップを設けた（`XSPI1_RESET0#`は存在しないためソフト制御不可）
- [ ] `XSPI1_*`信号の`P01`/`P02`ポートでPMCm/PFCmを設定し、周辺機能（PFC=0x1C）へ切り替えた（[15](15_mram_octal_memory_mapped_write_init.md) §2）
- [ ] MRAMのNonvolatile/Volatile Configuration Register 0 を `0xE7`/`0xC7`（Octal DTR）に設定する
      初期化シーケンス（方式(a)または(b)）を実装した
- [ ] Nonvolatile Configuration Register 8 bit0（Persistent Memory Mode）が`1`であることを
      確認した（既定値のはずだが明示設定を推奨）
- [ ] 起動時に Write Enable (`0x06`) を1回発行するシーケンスを実装した（電源投入毎に必要）
- [ ] RZ/N2H側 `LIOCFGCSn.PRTMD=0x3FF`、`CMCFG0CSn.FFMT=01b・ADDSIZE=4byte`、
      `CMCFG1/2CSn.RDCMD/WRCMD`にコマンドバイトを2バイト分（同一値を上位下位に）設定した
- [ ] 書き込み完了の安全マージン（ポーリングの要否）について、実機検証または
      Everspinへの確認で方針を決めた

## 9. 参照

- [15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md) —
  本ドキュメント§5の初期化シーケンスを、レジスタ単位まで落とし込んだ詳細手順
  （モジュール有効化、ピン設定、マニュアルコマンドの具体的なビット値、通信停止フロー、
  メモリマッピング書き込みの検証手順）
- [16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md) —
  ソフトウェア担当者向けMRAM基礎知識まとめ（本ドキュメントの前提知識、リファレンス用）
- [17_mram_est3000_factory_initialization.md](17_mram_est3000_factory_initialization.md) —
  EST3000準拠：工場出荷後に1回だけ必要な初回初期化(DFIM)手順（本ドキュメント・[15](15_mram_octal_memory_mapped_write_init.md)の運用フローとは別に、先行して1回実施が必要）
- [13_xspi_protocol_modes_and_quad_flash.md](13_xspi_protocol_modes_and_quad_flash.md) —
  RZ/N2Hのプロトコルモード列挙値、レジスタ共通仕様
- [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) —
  x8ブート時のCKP/CKN/DS/RESET#ピン利用例（同じ差動クロック構成の実例）
- [03_memory_map.md](03_memory_map.md) — xSPI1メモリマップアドレス（`0x0_5000_0000`）
- [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) §2.5 — メモリマッピング書き込みの一般的制約（NORフラッシュの場合）
- MRAM DS: §1 Device Description, §4 xSPI Signal Protocol, §5.5/5.6 Configuration Registers,
  §8 xSPI Commands and OpCodes (Table 21), §11 Write Operations, §22-23 Electrical
  Specifications / Orderable Part Numbers

---

[← README（目次）へ戻る](README.md)
