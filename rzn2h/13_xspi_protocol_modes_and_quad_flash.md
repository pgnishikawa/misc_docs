# xSPI プロトコルモード（1S-1S-1S / 4S-4S-4S 等）と Quad シリアルフラッシュ接続

[← README（目次）へ戻る](README.md)

出典: ユーザーズマニュアル R01UH1039JJ0130「37.3.1.7 LIOCFGCSn」p.2755、
「37.3.1.4-6 CMCFG0/1/2CSn」p.2752-2754、「37.3.2.7-8 CDTBUFn/CDABUFn」p.2760-2761、
「37.4.1.1 対応するプロトコルモード」p.2777、「37.4.3.2 書き込みアクセスの動作」p.2789、
「37.1 概要」p.2747。前提として [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md)
（メモリマッピング／マニュアルコマンドの基本、NORフラッシュ書き込み制約、XiP）を先に参照してください。

> **RZ/N2L 版との関係**: 本ドキュメントは [rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md](../../rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md)
> の RZ/N2H 版です。**xSPI の IP ブロックは RZ/N2L と同一**で、プロトコルモードの記法・
> 列挙値・構造上の制約はすべて共通です（後述の通り実際に完全一致することを確認済み）。
> 差分はベースアドレスと仕様値（スループット、マルチスレーブ数、アドレス空間サイズ）のみ。
> 記法の説明などRZ/N2L版と重複する内容は簡潔にとどめ、RZ/N2H固有の値を中心に記載します。

対象: Quad（4本のデータ線 IO0-IO3）対応のシリアル NOR フラッシュを xSPI0 または xSPI1 に
接続する場合の、プロトコルモード選定と実装上の注意点。

## 1. 記法の意味（nX-mX-oX）

xSPI のモード名は、SPI フレームの3フェーズ（コマンド－アドレス－データ）それぞれについて
「使用するデータ線の本数」と「SDR/DDR」を表す。**S**=SDR（片エッジ）、**D**=DDR（両エッジ）。
例: `1S-4S-4S` = コマンドは1本の線でSDR、アドレスとデータは4本の線（Quad）でSDR。
（詳細は [rzn2l/docs/05](../../rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md) §1 参照）

## 2. RZ/N2H の xSPI がサポートするプロトコルモード

`LIOCFGCSn.PRTMD[9:0]`（ベースアドレス `XSPIm = 0x801C_0000 + 0x1000 × m`、オフセット
`0x050 + 0x004 × n`）は RZ/N2L と**全く同じ7種類のコード値のみが有効**で、それ以外の値は
「設定禁止」と明記されている（37.3.1.7）。

| プロトコルモード | `PRTMD[9:0]` | 系統 | 典型的なSPI NORフラッシュのコマンド例 |
|---|---|---|---|
| `1S-1S-1S` | `0x000` | xSPI標準（シングル） | `0x03` Read／`0x06` WREN／`0x05` RDSR／`0x02` Page Program／`0x20` Sector Erase 等 |
| `1S-2S-2S` | `0x048` | QSPI互換（デュアル） | `0xBB` Fast Read Dual I/O 相当 |
| `2S-2S-2S` | `0x049` | QSPI互換（デュアル） | フラッシュが対応していれば全フェーズデュアルのコマンド |
| **`1S-4S-4S`** | **`0x090`** | **QSPI互換（クワッド）** | **`0xEB` Fast Read Quad I/O 相当＝Quadフラッシュの定番高速読み出しコマンド** |
| `4S-4S-4S` | `0x092` | QSPI互換（クワッド、QPI） | フラッシュを事前にQPIモードへ切り替えた場合の全フェーズクワッドコマンド |
| `4S-4D-4D` | `0x3B2` | xSPI標準（クワッドDDR） | フラッシュが対応していれば `0xED` Fast Read Quad I/O DTR 相当 |
| `8D-8D-8D` | `0x3FF` | xSPI標準（オクタルDDR） | HyperFlash / xSPI 8D-8D-8Dプロファイル2.0準拠デバイス用（本ドキュメントの対象外） |

**★RZ/N2Lと同一の注意点**: 「コマンド1本・アドレス1本・データ4本」（`1S-1S-4S`、SPI NOR
フラッシュの `0x6B` Fast Read **Quad Output** や `0x32` Quad Page Program に相当）は
このリストに**含まれない**。RZ/N2H の xSPI も「アドレスとデータを両方クワッドにする」
`1S-4S-4S`（`0xEB` Quad **I/O**）のみサポートし、データだけクワッドにする `0x6B`/`0x32`
系のコマンドには対応しない。Quad フラッシュ選定時は **`0xEB` 系対応**を確認すること。

## 3. 構造上の制約（RZ/N2Lと同一）

`LIOCFGCSn.PRTMD` は **CS単位の設定**であり、マニュアルコマンドモードとメモリマッピング
モードの両方がこの設定を共有する。マニュアルコマンドの `CDTBUFn`（37.3.2.7）には
コマンド/アドレス/データの**サイズ**設定はあるが、**ピン幅（プロトコル）を指定する項目は
ない**ことをレジスタ定義で確認済み。したがって：

- ある瞬間、そのCSで発行できるコマンドは全て同じプロトコルモード
- 「制御コマンドは1S-1S-1S、読み出しだけ1S-4S-4S」のように動的混在は不可、
  `LIOCFGCSn` の書き換え（＋通信停止手順）でモード切り替えが必要

詳細な運用パターン（QEビット設定、`0xEB`読み出しへの切替手順、Page Programの扱い、
QPIモードの是非）は [rzn2l/docs/05](../../rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md)
§4-5 がそのまま適用できる（レジスタ名・ビット定義が同一のため）。

## 4. RZ/N2L との差分（仕様値）

| 項目 | RZ/N2L | RZ/N2H |
|---|---|---|
| xSPIベースアドレス | `0x8022_0000 + 0x1000×m` | `0x801C_0000 + 0x1000×m` |
| RAWスループット | 200 MB/s（xSPI200） | **266 MB/s（xSPI266）** |
| マルチスレーブ | ユニット0: 最大2スレーブ／ユニット1: 1スレーブ | **両ユニットとも最大2スレーブ** |
| メモリマッピング可能空間 | CSあたり最大64MB | **CS0+CS1合計で最大256MB** |
| 割り込み要因 | 2 | 2（同数） |
| 書き込みバースト結合の判定バス | AHB（Cortex-R52のみ） | **AXI**（Cortex-A55/Cortex-R52が接続） |

プロトコルモードの列挙値・NORフラッシュ書き込み不可の構造・XiPの可否といった**動作面は
完全に同一**（IPブロック共通のため）。

## 5. ブートモードとの関係

RZ/N2H も RZ/N2L と同じく、**x1 ブートシリアルフラッシュ（`MD2:0=000b`）はフラッシュの
データ信号本数に関わらず `1S-1S-1S` 固定**（[04_boot_modes.md](04_boot_modes.md)、
[09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) §1.1）。ブートROMが
実際にドライブするのは `XSPI0_CKP`/`XSPI0_CS0#`/`XSPI0_IO0`/`XSPI0_IO1` の4本のみで、
Quad用の `IO2`/`IO3` は**ブート中は駆動されない**。Quad読み出しは一段目/二段目ローダが
`LIOCFGCSn` を再設定して初めて有効になる点、`IO2`/`IO3` のプルアップやブート後のPFC
切り替えが必要な点も含め、[rzn2l/docs/05](../../rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md)
§6 の指針がそのまま当てはまる。

xSPI0/xSPI1 ブート用アドレス（`LDR_ADDR_NML` の格納範囲）は RZ/N2L とは異なる
（[09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) 参照。xSPI0内部アドレス空間
`0x4000_0000`〜。RZ/N2Lは`0x6000_0000`〜であり、**両者を混同しないこと**、
[03_memory_map.md](03_memory_map.md)にも同様の注記あり）。

## 6. まとめチェックリスト

RZ/N2L版（[05](../../rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md) §7）と同一の
チェック項目がそのまま適用できる：

- [ ] 採用予定の Quad SPI フラッシュが `0xEB`（Fast Read Quad I/O）に対応していることを確認した
- [ ] QE (Quad Enable) ビットがある場合、`1S-1S-1S` のマニュアルコマンドで立てるシーケンスを実装した
- [ ] 制御コマンド（WREN/RDSR/Erase/Page Program）は `1S-1S-1S` で発行する設計にした
- [ ] 高速読み出し時のみ `LIOCFGCSn.PRTMD = 0x090`（1S-4S-4S）へ切り替え、`CMCFG1CSn.RDCMD=0xEB`／`ADDSIZE`／`RDLATE` をフラッシュの仕様に合わせて設定した
- [ ] プロトコルモード切り替え前に通信停止手順を踏む実装にした（37.4.7.2 通信停止のフロー）
- [ ] x1ブート中は `IO2`/`IO3` が未駆動であることを踏まえ、基板側のプルアップ／フラッシュの`WP#`/`HOLD#`仕様を確認した
- [ ] ブート後に `IO2`/`IO3` に対応する端子をxSPI0機能へ切り替えるPFC設定を実装した

## 7. 参照

- [rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md](../../rzn2l/docs/05_xspi_protocol_modes_and_quad_flash.md) — 記法の詳細説明とQuadフラッシュ運用の実践的設計（レジスタ名・値が共通のためRZ/N2Hにもそのまま適用可）
- [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) — RZ/N2H xSPI0 x1ブート詳細とランタイム操作
- [04_boot_modes.md](04_boot_modes.md) — 動作モード全体
- [03_memory_map.md](03_memory_map.md) — xSPI0/1アドレス空間
- マニュアル p.2747-2805（37章 xSPI）

---

[← README（目次）へ戻る](README.md)
