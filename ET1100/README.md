# ET1100 制御ドキュメント（ソフトウェアエンジニア向け）

RZ/N2L のマイコンから Beckhoff の EtherCAT スレーブコントローラ **ET1100** を、
外部バス（BSC＝Bus State Controller の SRAM 空間）経由で制御するためのドキュメント一式です。
EtherCAT の知識を前提とせず、ET1100 のハードウェアデータシート／レジスタリファレンスの内容から
ソフトウェア実装に必要な情報を整理しています。

## まず読むもの

**[06_implementation_roadmap.md](06_implementation_roadmap.md)** — RZ/N2L 上で ET1100 を
EtherCAT スレーブとして動かすまでの実装ロードマップ（フェーズ分け・完了条件・詰まりやすい点）。
以下の01〜05は、このロードマップの各フェーズで参照する詳細資料という位置づけです。

## 読む順番（詳細資料）

| # | ドキュメント | 内容 |
|---|---|---|
| 1 | [01_ethercat_basics.md](01_ethercat_basics.md) | EtherCAT とは何か（フレーム処理、FMMU/SyncManager、状態機械、メールボックス、Distributed Clocks）の基礎知識 |
| 2 | [02_et1100_overview.md](02_et1100_overview.md) | ET1100 チップ概要（機能一覧、アドレス空間、EEPROM/ESI、リセット、電源、LED、ポート） |
| 3 | [03_rzn2l_bus_interface.md](03_rzn2l_bus_interface.md) | RZ/N2L の外部バス（SRAM 空間）と ET1100 の非同期 8/16bit µC インタフェースの接続方法・信号対応・タイミング設計 |
| 4 | [04_software_bringup.md](04_software_bringup.md) | ソフトウェア設計：初期化シーケンス、状態遷移、割り込み設計、EEPROM/ESI 作成、推奨スレーブスタック、ブリングアップ手順 |
| 5 | [05_register_reference.md](05_register_reference.md) | よく使うレジスタの早見表（アドレス・サイズ・用途）と疎通確認サンプルコード |
| 6 | [06_implementation_roadmap.md](06_implementation_roadmap.md) | 全体を統合した実装ロードマップ（上記） |

## 元資料（`ET1100/` 配下）

これらのドキュメントは以下の Beckhoff / TI 提供資料を根拠にしています。本ドキュメントは要点の抜粋・整理であり、
実装の最終確認は必ず原本を参照してください。

| ファイル | 通称 | 内容 |
|---|---|---|
| `ethercat_esc_datasheet_sec1_technology_v2.5.pdf` | Section I – Technology | 全 ESC 共通のEtherCAT技術解説（プロトコル、FMMU、SyncManager、DC、状態機械、EEPROM、割り込み等） |
| `ethercat_esc_datasheet_sec2_registers_v3.3.pdf` | Section II – Register description | 全 ESC 共通のレジスタ詳細仕様 |
| `ethercat_et1100_datasheet_v2i1.pdf` | Section III – ET1100 hardware description | ET1100 固有のピン配置・PDI・電気特性 |
| `an_phy_selection_guidev3.2.pdf` | AN – PHY selection guide | EtherCAT 用 Ethernet PHY の要件・選定ガイド |
| `et1100_configuration_and_pinout_v4.7.xls` | Configuration and pinout tool | ピン用途と EEPROM 設定値（Config Area A/B）を計算する Excel ツール（Beckhoff 提供） |

本ドキュメント中では上記を「Section I §x.x」のように章番号付きで引用します。

## 本ドキュメントに含まれない情報（本リポジトリ外の依存）

- RZ/N2L のハードウェアユーザーズマニュアルに基づく詳細（BSC/CS0のレジスタ設定、
  I/Oポートのピン設定、割り込みコントローラ等）は [../../rzn2l/docs/](../../rzn2l/docs/)
  にまとめてあります。`03_rzn2l_bus_interface.md` 執筆時点では同マニュアルが手元になく
  FSP ソースコード（`rz-fsp/`）のみを根拠にしていた箇所も、その後のマニュアル入手により
  `rzn2l/docs/07_bsc_cs0_et1100_register_setup.md` 等で裏付け・詳細化されています。
- **CoE/メールボックス/ESI の正式プロトコル仕様**（ETG.1000.6、ETG.1020、ETG.2000）は
  EtherCAT Technology Group (ETG) が発行するものであり、本リポジトリには含まれていません。
  `04_software_bringup.md` の CoE/PDO マッピングの説明は一般的な実装の考え方の概要に
  留めています。詳細実装時は ETG 発行資料（ETG会員向け）または採用するスレーブスタックの
  ドキュメントを参照してください。
- EtherCAT マスタ（TwinCAT、IgH EtherCAT Master、SOEM 等）や Wireshark の
  EtherCAT dissector など、開発・検証用ツールも本リポジトリの範囲外です
  （`04_software_bringup.md` 4.10節に選択肢を紹介）。

## この文書の対象読者・前提

- 対象：RZ/N2L 上でファームウェアを書くソフトウェアエンジニア（EtherCAT 未経験を想定）
- 接続構成：RZ/N2L の外部バス（BSC、FSP 上は `r_bsc` / External Bus Interface、メモリタイプ = SRAM）に
  ET1100 を PDI（非同期 8/16bit µController インタフェース）として接続
- ハードウェア（PHY 選定、EBUS/MII、基板設計）は範囲外としつつ、必要な範囲で補足しています

## 用語ミニ辞書

| 用語 | 意味 |
|---|---|
| ESC | EtherCAT Slave Controller。ET1100 はこの ASIC 実装のひとつ |
| PDI | Process Data Interface。ESC とホスト µController を繋ぐインタフェース（本件では非同期 8/16bit µC バス） |
| FMMU | Fieldbus Memory Management Unit。EtherCAT の論理アドレスを ESC 内の物理アドレスに変換する機構 |
| SyncManager (SM) | ESC 内のメモリを EtherCAT マスタとローカルアプリケーションで安全に受け渡すための機構 |
| DC | Distributed Clocks。複数スレーブ間で時刻を同期する機構 |
| SII EEPROM | ESC に外付けする I²C EEPROM。ESC の起動設定と ESI（スレーブ識別情報）を格納する |
| ESI | EtherCAT Slave Information。スレーブを識別・設定するための情報（EEPROM とマスタ用 XML の両方に存在） |
| AL 状態 | Application Layer 状態。Init / Pre-Operational / Safe-Operational / Operational の4状態 |
