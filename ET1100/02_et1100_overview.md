# 2. ET1100 概要

[← 目次](README.md)

出典: `ethercat_et1100_datasheet_v2i1.pdf`（Section III – ET1100 hardware description、以下「HW」と略記）

## 2.1 ET1100 とは

Beckhoff 製の EtherCAT スレーブコントローラ（ESC）ASIC です。EtherCAT の
リアルタイムフレーム処理（FMMU/SyncManager によるデータ交換）をハードウェアで行い、
ホスト µController（本件では RZ/N2L）とは **PDI (Process Data Interface)** と呼ぶ
インタフェースで接続します。パッケージは BGA128、10×10mm（HW Table 4）。

## 2.2 アドレス空間とメモリマップ（全体像）

ESC は **64KB のアドレス空間**を持ちます（HW 2.2、Table 6/7）。

```
0x0000 ─┬─────────────────────────────────────┐
        │ レジスタ領域 (4 KB)                    │  ← 0x0000-0x0FFF
0x1000 ─┼─────────────────────────────────────┤
        │ プロセスデータ RAM (8 KB, ET1100)       │  ← 0x1000-0x2FFF
0x3000 ─┼─────────────────────────────────────┤
        │ 未使用領域                              │
0xFFFF ─┴─────────────────────────────────────┘
```

主なレジスタ領域（詳細アドレスは [05_register_reference.md](05_register_reference.md)）：

| 領域 | 用途 |
|---|---|
| `0x0000`-`0x0009` | チップ基本情報（Type/Revision、FMMU数・SM数・RAM サイズ等） |
| `0x0010`-`0x0013` | ステーションアドレス／エイリアス |
| `0x0100`-`0x0135` | データリンク層制御・状態、AL control/status（EtherCAT 状態機械） |
| `0x0140`-`0x0153` | **PDI 設定**（本ドキュメントの主題） |
| `0x0200`-`0x0223` | 割り込み（ECAT event / AL event） |
| `0x0300`-`0x0313` | エラーカウンタ |
| `0x0400`-`0x0443` | ウォッチドッグ |
| `0x0500`-`0x050F` | SII EEPROM インタフェース（I²C） |
| `0x0510`-`0x051F` | PHY 管理インタフェース（MII Management） |
| `0x0600`-`0x06FC` | FMMU 0〜7（各16バイト） |
| `0x0800`-`0x087F` | SyncManager 0〜7（各8バイト） |
| `0x0900`-`0x09FF` | Distributed Clocks |
| `0x0F80`-`0x0FFF` | User RAM（128バイト、汎用） |
| `0x1000`〜 | プロセスデータ RAM（8KB） |

## 2.3 主要スペック（HW 第2章 Table 4 より抜粋・ET1100 のみ）

| 項目 | 値 |
|---|---|
| EtherCAT ポート数 | 2〜4（実装依存、EEPROM で設定） |
| FMMU 数 | **8** |
| SyncManager 数 | **8** |
| プロセスデータ RAM | **8 KB** |
| User RAM | 128 バイト |
| SII EEPROM | I²C、1Kbit〜4Mbit 対応、I²C ベースアドレス = 0（実アドレス 0x50） |
| Distributed Clocks | あり、64bit 幅、Sync/Latch 信号 2本 |
| 対応 PDI | Digital I/O・SPI slave・**非同期 8/16bit µC**・同期 8/16bit µC・多重化async/sync・SPI master・オンチップバス 等 |
| コア電圧 | 2.5V（内蔵 LDO 1個） |
| I/O電圧 | 3.3V（5V 許容） |
| クロック | 25MHz 水晶（±25ppm 精度が必要、後述） |

## 2.4 PDI（Process Data Interface）の選択

ET1100 が対応する PDI 種別は PDI control レジスタ `0x0140[7:0]` の値で選ばれます
（HW 第6章 Table 58）。

| PDI番号 | 名称 | ET1100対応 |
|---|---|---|
| 0 | Interface deactivated | ○ |
| 4 | Digital I/O | ○ |
| 5 | SPI slave | ○ |
| **8** | **16bit 非同期 µController** | **○（本ドキュメントで採用）** |
| **9** | **8bit 非同期 µController** | **○（データ幅8bitで良ければこちらでも可）** |
| 10 | 16bit 同期 µController | ○ |
| 11 | 8bit 同期 µController | ○ |
| 16-20 | Digital I/O バリエーション | - |
| 128 | On-chip bus (Avalon/OPB、FPGA用) | - |

**重要**: この PDI 種別の選択は **ランタイムにレジスタで切り替えるものではなく、
SII EEPROM の Configuration Area A に書き込んでおく値**です（HW 6.4.2、後述 2.7）。
電源投入時に ET1100 が自動でこの設定を読み込み、対応する PDI 信号を有効化します。
RZ/N2L のファームウェアが起動する前に、この設定が確定している必要があります。

「非同期」と「同期」の違いは、後者がバスクロック（`BUSCLK`）を必要とする点です。
RZ/N2L の外部バス（BSC、後述）はクロックを外部出力しない非同期方式なので、
**非同期 8/16bit µController インタフェース（PDI = 8 または 9）を選びます**。
詳細な信号・タイミングは [03_rzn2l_bus_interface.md](03_rzn2l_bus_interface.md) を参照してください。

## 2.5 リセット（RESET ピン）

- `RESET` ピンはオープンコレクタの双方向信号（active low）。電源電圧不足時、
  または `0x0040`（ECAT reset）レジスタ経由でも自動的にアサートされます（HW 3.4）。
- **Beckhoff は PHY と µController（RZ/N2L）を同じ RESET ネットに接続することを推奨**しています
  （HW 3.4.1）。これにより ET1100 がリセット中は PHY も通信せず、EtherCAT 側から
  スレーブ全体（PHY 含む）をリセットすることも可能になります。
  → RZ/N2L 自身のリセット回路・リセットICもこのオープンコレクタ・ネットに
  ワイヤードOR接続する設計が推奨されます（RZ/N2L 側の RESET 端子の電気的仕様は
  RZ/N2L のハードウェアマニュアルで要確認）。

## 2.6 電源投入シーケンス（ソフトウェアが待つべきタイミング）

HW/Section I 17.2「Power-on sequence」の要点：

1. 電源電圧が確立
2. PLL ロック → クロック生成開始
3. `RESET` 解除 → ET1100 動作開始。**ただしこの時点ではプロセスメモリも PDI も未動作**
4. **SII EEPROM の読み込み開始**（`RESET` 解除から読み込み開始まで typ. **約168ms**、HW Table 84 `tDelay`）
5. EEPROM 読み込み成功後：
   - ESC 設定レジスタが初期化される（PDI 種別確定）
   - PDI が有効化される
   - `ESC DL status` レジスタ `0x0110[0]` が **1** になる（"PDI operational"）
   - 一部 PDI では `EEPROM_LOADED` 信号がアサートされる
   - ET1100 は **Init** 状態になる

**ソフトウェア設計への影響**：RZ/N2L 側は電源投入後すぐに ET1100 へアクセスしてはいけません。
`0x0110[0]`（またはハードウェアで `EEPROM_LOADED` 相当信号）を確認してから
最初のレジスタアクセスを行ってください。EEPROM サイズによって読み込み時間が変わるため
（HW Table 84：小容量 EEPROM で数百µs〜数ms程度、`tDelay`自体は約168ms固定）、
**数百ms のタイムアウトを持たせたポーリング**にするのが安全です。

## 2.7 SII EEPROM（設定と ESI）

> **TPS25751 のパッチバンドルロードとの違い**：ET1100 には CPU もファームウェアもなく、
> 毎回の電源投入時に RZ/N2L がデータを push する初期化は不要です。ET1100 は完全に
> ハードワイヤードされた ASIC で、必要な「設定データ」（下記）は**専用の物理 I²C EEPROM
> チップから ET1100 自身が I²C マスタとして自動で読み込みます**。加えて、TPS25751の
> 「EEPROM非搭載構成でホストがI2C経由でデータを都度供給する」方式に相当する
> **「EEPROM emulation by PDI」機能は ET1100 では非対応**です（Section III 機能一覧表。
> この機能があるのは FPGA 向けの EtherCAT IP core のみ）。したがって **ET1100 を使う限り
> 物理 EEPROM チップの搭載は必須**であり、RZ/N2L がそれを肩代わりする選択肢自体が
> ありません。RZ/N2L が関わるのは、後述 2.7.1 のとおり「EEPROM の中身を（工場出荷時などに）
> 一度だけ確定・書き込む」プロビジョニング作業だけで、以降の電源投入では一切関与しません。

- 電気インタフェースは I²C（`EEPROM_CLK`/`EEPROM_DATA`、ET1100 内蔵プルアップあり）。
  ET1100 と EEPROM は基本的に **1:1 の専用バス**として設計されており、他の I²C マスタを
  同じバスに繋ぐ場合は ET1100 をリセット状態に保つ必要があります（HW 第8章）。
- EEPROM word 0〜7（Configuration Area A）と、必要なら word 40〜47（Area B）は
  **電源投入直後に ET1100 自身が自動で読み込む**必須領域です。PDI 種別、DC 設定、
  Station Alias 等がここに入ります。
- word 0〜63（0x00-0x3F）＋ General Category が **ESI の絶対最小構成**です
  （最小 EEPROM サイズは 2Kbit）。実際には Vendor ID・Product Code・Revision・
  カテゴリ文字列等をマスタ用 ESI XML と一致させる必要があるため、
  複雑なデバイスでは **32Kbit 以上**の EEPROM が推奨されます（Section I 第11章）。
- **EEPROM への書き込み後、10秒以内に電源断・リセットしてはいけません**
  （内部ストア時間の制約、Section I 第11章）。

EEPROM の内容設計（PDI 種別・ピン設定・ESI 基本情報）は、`ET1100/et1100_configuration_and_pinout_v4.7.xls`
（Beckhoff 提供の設定/ピン配置計算ツール）を使うと、ピンごとの推奨接続と
Configuration Area A/B のビット値を機械的に導出できます。最終的な仕様は
このドキュメント（HW 本体）が優先されます（HW 第3章冒頭の注記）。

### 2.7.1 EEPROM を誰がどうやって書き込むか（実装で必ず必要になる手順）

EEPROM の中身を「確定させる」だけでは実装できません。実際に書き込む手段が要ります。
ET1100 は **PDI（＝RZ/N2L）自身が I²C マスタを持たなくても、ET1100 のレジスタ経由で
SII EEPROM の読み書き・再読込ができる**ようになっています（Section II 2.11「SII EEPROM
interface」、`0x0500`-`0x050F`）。

| レジスタ | 内容 |
|---|---|
| `0x0500` | EEPROM ECAT access state。bit0: 0=EtherCAT側が制御、1=PDI（RZ/N2L）側が制御 |
| `0x0501` | EEPROM PDI access state（同上のPDI側ビュー） |
| `0x0502:0x0503` | EEPROM control/status。`[10:8]` コマンド（`001`=Read, `010`=Write, `100`=**Reload**）、`[12]`=EEPROM未ロード、`[15]`=Busy |
| `0x0504:0x0507` | EEPROM word アドレス |
| `0x0508:0x050F` | EEPROM データ（4 または 8 バイト） |

典型的な手順（RZ/N2L 側から書く場合）：

1. `0x0501[0]=1` を書いて PDI 側にアクセス権を取得する（EtherCAT 側が使用中でないことを
   `0x0500[0]` 等で確認）
2. `0x0504` にワードアドレスを書く
3. 書き込みたい場合は `0x0508` 以降にデータを書いてから `0x0502[10:8]=010`（Write）を発行、
   読み出しなら `0x0502[10:8]=001`（Read）を発行
4. `0x0502[15]`（Busy）が0に戻るまで待つ
5. Configuration Area A/B（word 0-7, 40-47）を書き換えた場合は、**`100`（Reload）コマンドを
   発行**して ET1100 に再読込させる（電源再投入と同等の効果。PDI 種別変更などはこれで
   反映される）

**★ただし「工場出荷時、EEPROMが完全に未書き込み（ブランク）の状態」ではこの経路は使えない。**
Section I 11.2.1「SII EEPROM errors」に明記されている通り、EEPROM の読み込みに失敗
（チェックサム不一致・応答なし含む）すると、リトライ1回の後、**PDI Operational ビット
（`0x0110[0]`）は立たず、`EEPROM_LOADED` 信号も非活性のまま**になる。さらに、
**Configuration Area で初期化されるレジスタ（`0x0140`/`0x0141` の PDI 種別選択を含む）は
「読み込み失敗時、直前の値を保持する」**ため、初回電源投入時（＝「直前の値」が存在しない
ハードウェアリセット直後）は PDI 種別が有効化されない可能性が高い。**つまりブランクな
EEPROMのままでは、RZ/N2L から見た PDI（今回の非同期 8/16bit µC バス）自体が反応しない
可能性があり、"PDIレジスタ経由でEEPROMを書く" というこの手順自体が実行できない**
（鶏と卵の関係）。

**用途の例（前提: EEPROMに何らかの有効な内容が既に入っている場合のみ）**：
- フィールドでの ESI/設定更新（Vendor情報の変更や不具合修正）
- 開発中に PDI 種別や DC 設定を試行錯誤で調整する（一度有効な内容が書き込まれ、
  PDI が生きている状態からの再設定）

**工場出荷時の初回書き込みは、`04_software_bringup.md` 4.6節で述べる通り、
ET1100 を `RESET` 状態に保持したまま外部 I²C ライタで `EEPROM_CLK`/`EEPROM_DATA` に
直接書き込む方法（ET1100 は PDI を介さない）を基本とすること。** RZ/N2L 経由の
レジスタ書き込みは、その後の更新・調整用と位置づけるのが安全。
実装ロードマップ全体の中での位置づけは [06_implementation_roadmap.md](06_implementation_roadmap.md)
Phase 3 を参照。

## 2.8 割り込み（IRQ）

- `IRQ` ピン（非同期 µC PDI の場合）は **レベルトリガ、複数の内部要因の論理和**です
  （Section I 12.1、Figure 46）。`AL event request (0x0220:0x0223)` と
  `AL event mask (0x0204:0x0207)` の AND を全ビット OR したものが1本の IRQ 信号になります。
- **エッジトリガ割り込み入力の µController に接続する場合は要注意**（Section I 18.13.1）：
  ISR 内で `AL event request` を読み切って要因がすべてクリアされたことを確認してから
  ISR を抜けないと、「新しい割り込み要因がISR処理中に発生 → エッジが来ないので二度と
  ISR が呼ばれなくなる」という事故が起きます。RZ/N2L 側でこの ISR 設計は
  [04_software_bringup.md](04_software_bringup.md) で具体的に扱います。
- ポーリングと割り込みは**混在させない**（同じ要因を両方の方法で消費すると
  取りこぼしが起きる、Section I 18.13.2）。
- RZ/N2L 側の受け口：本リポジトリの FSP には外部割り込み用ドライバ `r_icu`
  （`external_irq_api_t` を実装）があり、トリガ種別として
  `EXTERNAL_IRQ_TRIG_LEVEL_LOW` / `EXTERNAL_IRQ_TRIG_LEVEL_HIGH`（レベルトリガ）と
  `EXTERNAL_IRQ_TRIG_FALLING` / `RISING` / `BOTH_EDGE`（エッジトリガ）が選べます
  （`r_external_irq_api.h`）。ET1100 の `IRQ`（既定 active low）を接続する場合は
  **`EXTERNAL_IRQ_TRIG_LEVEL_LOW` を選び**、レベルトリガのままISR側で
  `AL event request` を読み切る設計にするのが最も安全です（エッジトリガしか
  選べない古い MCU 向けの回避策は不要）。

## 2.9 LED・ポート・PHY（参考、ハードウェア設計者向け情報の要約）

- `RUN` LED は AL status レジスタで自動制御されます（緑）。
- 各ポートに Link/Activity LED、PERR（受信エラー）LED があります。
- ET1100 のポートは **EBUS（LVDS、Beckhoff独自）** または **MII（外付け Ethernet PHY 経由）**
  のどちらかで構成します。RZ/N2L 接続の観点では影響しませんが、外付け PHY を使う場合は
  `an_phy_selection_guidev3.2.pdf` に記載の要件（リンク断検出時間、MDC非依存の起動、
  Enhanced link detection 要否など）を満たす PHY を選定する必要があります。
- **未使用の物理ポート 0 を作らないこと**（Section I 18.2）。ポート0は特殊な役割を持ち、
  複数のスレーブでポート0を未使用にすると通信不能になるケースがあります。

次章では、RZ/N2L 側の外部バス（SRAM 空間）と ET1100 の非同期 8/16bit µC PDI の
具体的な接続方法を扱います。
