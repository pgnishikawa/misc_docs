# Everspin MRAM (EM064LXOAB320IS2T) 基礎知識まとめ ― ソフトウェア担当者向け

[← README（目次）へ戻る](README.md)

出典: `MRAM/EMxxxLX_B_HR Datasheet v3.7_0.pdf`（Everspin Technologies、以下「MRAM DS」）。
RZ/N2Hへの実際の接続・初期化手順は
[14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md)（互換性の検証）と
[15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md)
（レジスタ単位の初期化手順）にまとめてある。本ドキュメントは実装の手順書ではなく、
**「このMRAMを触る前に知っておくと実装判断を誤らない」概念・リファレンス集**である。

## 1. MRAMとは何か（STT-MRAM）

- **MRAM (Magnetoresistive RAM)** は、電荷ではなく**磁性体素子の磁化方向**でビットを
  記憶する不揮発メモリ。本製品はその中でも **STT-MRAM (Spin-Transfer Torque MRAM)**
  という方式を使う（MRAM DS表紙「STT-MRAM Persistent Memory」）。
- 実用上の意味は次の3点に集約される：
  1. **消去（Erase）が不要**。NOR/NANDフラッシュは「1→0はビット単位で書けるが、
     0→1に戻すには事前にブロック/セクタ単位で消去が必要」という制約があるが、
     MRAMは**任意のビットを任意の方向へ1回の書き込みで直接反転**できる。
  2. **書き込み回数の制限が事実上ない**（Data endurance: Unlimited read, write and
     erase operations for supported life of product）。NORフラッシュのような
     「セクタあたり10万回」のような摩耗管理・ウェアレベリングの心配が不要。
  3. **速度がSRAM的**。読み出しはもちろん、**書き込みもNORフラッシュのような
     長いプログラム時間（tPP、数百µs単位）を必要とせず**、バストランザクションの
     中でほぼ完結する（400MBytes/S sustained throughput、OSPI DTR 200MHz時）。

つまりソフトウェア視点では、**「不揮発だが、SRAMのように読み書きできるメモリ」**
と捉えるのが最も実態に近い。NORフラッシュのドライバ資産（消去管理、書き込み
ステータスポーリング、摩耗平準化）をそのまま持ち込む必要はない。

データ保持は **10年以上（全温度範囲）**、動作温度は産業用グレードで
**-40℃～+85℃**（本製品の型番`...IS2T`のIndustrial表記と一致）。

## 2. 動作モード：Persistent Memory Mode と NOR Flash Emulation Mode

MRAMは内部的に2つの動作モードを持ち、**Nonvolatile/Volatile Configuration
Register 8 の bit0（Write mode）**で選択する：

| bit0 | モード | 挙動 |
|---|---|---|
| `1`（**既定値**） | **Persistent Memory Mode** | 消去不要。任意アドレス・任意バイト数を直接読み書きできる（SRAM的）。書き込みアドレスがメモリ末尾に達すると先頭へラップする。ページ境界の制約がない |
| `0` | NOR Flash Emulation Mode | 既存のNORフラッシュ向けソフトウェア資産と互換性を保つためのレガシーモード。256バイトページ制限あり、Erase系コマンド（Sector Erase / Bulk Erase等）が意味を持つ |

**通常は既定値のPersistent Memory Modeをそのまま使う**。NOR Flash Emulation Mode は
「既存のSPI NORフラッシュ用ドライバをほぼ変更せずMRAMに置き換えたい」という
移行シナリオ向けの互換機能であり、新規設計では選ぶ理由が薄い。

## 3. Write Enable Latch (WEL) のスティッキー動作 ― 一般的なSPI NORとの最大の違い

一般的なSPI NORフラッシュでは、**書き込み系コマンド（Page Program, Erase等）を
発行するたびに毎回 `WREN (0x06)` を送り直す**必要がある（コマンド完了後にWELが
自動クリアされる仕様のため）。

このMRAMは異なる：

> Write (Program) commands will not reset WEL at the completion of the command,
> allowing for back-to-back writes to memory without loading the WREN command again.
> （MRAM DS §11 Status Register / Write Enable Latch (WEL) Bit）

つまり：

- `WREN`は**電源投入後（またはリセット後）に1回発行すればよい**
- 以後の`Write`コマンドは、WELを消費せず何度でも実行できる
- WELが実際にクリアされるのは、**`Write Disable (0x04)`コマンドの実行時**、
  または **POR／ソフトウェアリセット／ハードウェアリセット時**のみ
  （§4「WELが持続する条件」参照）

これは**RZ/N2Hのメモリマッピングモードにおける書き込み制約**
（[09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) §2.5：
「1回のAXI書き込み→固定1コマンド」しか送れないため、通常のNORフラッシュでは
毎回のWREN前置ができずメモリマッピング書き込みが原理的に不可能）を
**このMRAMだけは回避できる**という、本チップ選定における最大の技術的メリットの
根拠になっている（詳細は[14](14_mram_em064lx_xspi1_octal_connection.md) §4）。

## 4. WELが持続する条件・失われる条件

| イベント | WELへの影響 |
|---|---|
| `Write (Program)`コマンド完了 | **維持される**（クリアされない） |
| VCR/NVCRのI/Oモード変更（例: 1S-1S-1S→Octal DTR切替） | **維持される**（レジスタ書き込みはWELをクリアする操作として仕様に含まれていない） |
| `Write Disable (0x04)` | クリアされる |
| POR（電源投入） | クリアされる（既定`0`） |
| ソフトウェアリセット（`0x66`+`0x99`） | クリアされる |
| ハードウェアリセット（`RESET#`ピン） | クリアされる |

したがって、**プロトコルモード切替（VCR0書き込み）をまたいでもWELの再送は不要**
（[15](15_mram_octal_memory_mapped_write_init.md) §7・§9で実際に踏む手順として反映済み）。
一方、**何らかの理由でMRAMがリセットされた場合は、WRENを再送する必要がある**
点はソフトウェア側で見落としやすいので注意する（後述§7の電源断・リセット系
イベントとの関係を参照）。

## 5. サポートするプロトコルモードの全体像

MRAM DS §4 (xSPI Signal Protocol) によれば、本製品は **SPI / Dual SPI / Quad SPI /
Octal SPI** の全モードを持ち、各モードで **STR（Single Transfer Rate）と
DTR（Double Transfer Rate）**の両方をサポートする（型番・ファミリ次第で上限速度は
異なる。本品番`EMxxxLX`は最大200MHz Octal DTR）。

xSPIの命名規則（`nX-mX-oX` ＝ コマンド-アドレス-データの各フェーズのデータ線数と
S/D）そのものはRZ/N2Hのマニュアルと共通の考え方で、詳細は
[13_xspi_protocol_modes_and_quad_flash.md](13_xspi_protocol_modes_and_quad_flash.md) §1
を参照。本プロジェクトで採用するのは **8D-8D-8D（Octal DTR）**。

MRAM独自の重要な仕様として、**8D-8D-8Dでは「コマンドオペコードを2バイト分
（同じ値を）繰り返し送出する」ことが必須**という制約がある：

> 8D-8D-8D means that the command, the command modifier, and data transfers are
> always 8 bits wide DTR. The EMxxxLX allows the option to repeat the command
> opcode which makes an 8D command look like an 8S command, but **it is required
> to repeat the command opcode**.

これはRZ/N2Hの「8D-8D-8Dプロファイル1.0」フォーマット（`CMCFG0CSn.FFMT=01b`、
コマンドフィールド2バイト）と正確に一致するため相性が良い
（[14](14_mram_em064lx_xspi1_octal_connection.md) §3.1で確認済み）。

## 6. 主要な設定レジスタ（Configuration Registers）

MRAMには**不揮発（Nonvolatile）と揮発（Volatile）で対になった設定レジスタ**が
あり、レジスタ番号・ビット配置は完全に同一（アドレス0x0000～0x0008等）。違いは
反映タイミングだけ：

| | Nonvolatile Configuration Register (NVCR) | Volatile Configuration Register (VCR) |
|---|---|---|
| 反映されるタイミング | **電源投入時／リセット時のみ**、内部設定レジスタへロードされる | **WRITE VOLATILE CONFIGURATION REGISTERコマンド完了直後から即座に反映** |
| 電源断での保持 | 保持される（不揮発） | **保持されない**（電源断でクリア、次回起動時は再度NVCRの値がロードされる） |
| 主な用途 | 工場出荷時に一度だけ確定させたい既定動作モードの設定 | 起動のたびにソフトウェアから動的に切り替えたい設定 |

代表的なレジスタ（番号=アドレス、両系列共通）:

| レジスタ番号 | 名称 | 既定値 | 意味 |
|---|---|---|---|
| 0 | I/O Mode | `0xFF`（SPI with DS） | プロトコルモード選択。`0xE7`=Octal DTR with DS、`0xC7`=Octal DTR w/o DS 等 |
| 1 | Dummy Cycle Configuration | `0x00`（16サイクル） | 高速読み出し時のダミーサイクル数（0～31） |
| 5 | Address Mode | `0xFF`（3バイト） | `0xFE`で4バイトアドレスモードへ |
| 8 | 各種モード設定 | bit0=`1`（Persistent Memory Mode）, bit1=`1`（RPE, Reset Pin Enabled） | §2の動作モード、`RESET#`ピンの有効/無効 |

**Octal DTRでの注意点（MRAM DS Table 19 注2）**: 8D（DTRオクタル）モードで
Nonvolatile/Volatile Configuration Registerへ書き込む場合、**開始アドレスは偶数
でなければならず、1バイトだけの書き込みはできない（2バイト単位）**。1S-1S-1S
モードで設定する分には通常の1バイト書き込みでよい（[15](15_mram_octal_memory_mapped_write_init.md)
§6はこちらのケース）。

## 7. リセット・電源関連の挙動

MRAMのリセット手段は3種類あり、**いずれもVolatile Configuration RegisterとWELを
クリアする**（§4参照）：

1. **ソフトウェアリセット**: `RESET Enable (0x66)` → `RESET Memory (0x99)` の
   2コマンドを連続発行。誤動作防止のため2段階になっている。
2. **ハードウェアリセット**: `RESET#`ピンをLowにする（`CS#`がHighの間のみ有効）。
   Register 8 bit1（RPE）が`0`の場合、`RESET#`ピン自体が無視される設定にもできる。
3. **JESD252シグナルシーケンスリセット**: `CS#`/`CK#`/`IO0`を特定のパターンで
   駆動する、専用ピンを使わないリセット方式（JEDEC標準）。**コントローラと
   MRAMのプロトコル状態が同期しなくなった場合の救済手段**として用意されている。
   Non-Volatile/Volatile Registerの値自体は変更されない点が他の2方式と異なる。

> **RZ/N2H接続時の注意**: RZ/N2Hのxsチップ内蔵xSPIには通常`XSPIm_RESET0#`という
> 専用リセット出力ピンがあるが、これは**ユニット0（xSPI0）専用**で、
> MRAMを接続する**xSPI1には存在しない**（[14](14_mram_em064lx_xspi1_octal_connection.md)
> §6で確認済み）。したがって本構成では**ハードウェアリセットは外部プルアップ
> 任せ／汎用GPIO制御**にせざるを得ない。ソフトウェアから能動的にMRAMを
> リセットしたい場合は、上記(1)ソフトウェアリセットコマンド、または
> (3)JESD252シグナルシーケンス（RZ/N2Hのxsコマンド機構における
> 「パターン要求のフロー」`LPCTLn.PATREQ`が対応する可能性がある。マニュアル
> 37.4.7.7参照。本ドキュメント・[15](15_mram_octal_memory_mapped_write_init.md)では
> 未検証・未実装）を検討する。

**Deep Power Down**（`0xB9`で入る、`0xAB`または各種リセットで出る）は最低消費電力
モード（typ. 290µA）。Deep Power Down中は`Exit`/`Reset Enable`/`Reset`以外の
コマンドは無視される。バッテリ駆動や低消費電力要件がある場合の選択肢として
覚えておく（本プロジェクトの初期スコープでは未使用）。

## 8. 電気的基礎

- **電源電圧**: 1.65V～2.0V（**1.8V**運用が標準。3.3V動作の記載はない）
- **クロック**: 24-BGAパッケージには相補クロック（CK#）ボールが存在しない
  ＝**単相クロック（CK）のみ**。RZ/N2Hのxsチップ側もxSPI1はハードウェア的に
  単相クロック出力（`XSPI1_CKP`のみ、`CKN`はユニット0専用で存在しない）なので
  ちょうど噛み合う（[14](14_mram_em064lx_xspi1_octal_connection.md) §6）
- **データストローブ (DS)**: 読み出しデータと同時に出力される追加信号。
  高クロック時のサンプリング精度向上に必須（本製品では実質必須構成）
- **パッケージ**: 24-ball BGA（6mm×8mm、5×5配列）。8-pad DFNパッケージ品もあるが
  `IS2T`はBGA品番

## 9. ステータスレジスタの主要ビット（デバッグ・監視用）

`Read Status Register (0x05)` で読める主なビット：

| bit | 名称 | 意味 |
|---|---|---|
| 0 | WIP (Write In Progress) | `1`=書き込み／消去／CRC処理中でビジー。`0`=Ready |
| 1 | WEL (Write Enable Latch) | §3・§4参照 |
| 2-4, 6 | BP[3:0] | ソフトウェア書き込み保護のブロック範囲設定（不揮発） |
| 5 | Top/Bottom | BP範囲の起点（上端/下端） |
| 7 | Status Register Write Enable/Disable | `WP#`ピンと組み合わせたハードウェア書き込み保護の有効/無効 |

`Flag Status Register (0x70)` はエラー系ビット（Program/Erase失敗等）を持つ、
より詳細な状態確認用レジスタ。トラブルシュート時に合わせて確認するとよい
（本ドキュメントでは詳細割愛。MRAM DS §5.3参照）。

## 10. 実装時に参照すべきドキュメント

| 知りたいこと | 参照先 |
|---|---|
| そもそもRZ/N2HにこのMRAMを8D-8D-8Dで繋げるか、書き込みもメモリマッピングでできるか | [14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md) |
| 実際にどのレジスタにどの値を、どの順番で書けばよいか（レジスタ単位の初期化手順） | [15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md) |
| xSPIのプロトコルモード全般の仕組み（RZ/N2H共通知識） | [13_xspi_protocol_modes_and_quad_flash.md](13_xspi_protocol_modes_and_quad_flash.md) |
| メモリマッピング／マニュアルコマンドモードの基本、NORフラッシュとの一般的な違い | [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) §2 |
| xSPI1のアドレス空間 | [03_memory_map.md](03_memory_map.md) |

## 11. まとめ：一言で言うと

**「消去不要・書き込み回数無制限・SRAM並みの速度を持つ不揮発メモリ」**であり、
かつ**WRENを一度発行すればWELが持続する**という特有の仕様のおかげで、
通常のSPI NORフラッシュでは不可能な「メモリマッピングモードでの直接書き込み」が
そのまま使える。ソフトウェア設計上は、**摩耗管理・消去シーケンス・毎回のWREN
発行といったNORフラッシュ由来の複雑さを持ち込む必要がない**、という点が
最も重要な理解ポイントである。

---

[← README（目次）へ戻る](README.md)
