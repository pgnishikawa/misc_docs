# RZ/N2H 調査ドキュメント

ルネサス RZ/N2H（および同一マニュアルに含まれる RZ/T2H）に関する調査用のまとめです。
元資料は本リポジトリ直下の PDF 2 点です。

| 資料 | ファイル | 版数 | ページ数 |
|------|----------|------|----------|
| ユーザーズマニュアル ハードウェア編 | `r01uh1039jj0130-rzt2h-rzn2h.pdf` | R01UH1039JJ0130 Rev.1.30 (Jul 7, 2026) | 3817 |
| ハードウェアデザインガイド（アプリケーションノート） | `r01an7419jj0120-rzt2h-n2h-hardware-design-guide.pdf` | R01AN7419JJ0120 Rev.1.20 (2026.6.15) | 56 |

> マニュアルは MPU スーパーセットの仕様を記載しており、RZ/T2H と RZ/N2H 両方をカバーします。
> 製品によっては存在しない端子・レジスタ・機能があります（使用できないレジスタ領域は予約領域）。

Cortex-A55／Cortex-R52 の MMU/MPU・キャッシュ・ブート手順（Arm アーキテクチャの領域で RZ/N2H マニュアルには記載がない部分）は、`arm/` ディレクトリに取得した Arm 公式ドキュメント（Cortex-A55/R52 TRM、Cortex-R52 Programmer's Guide 等）で裏付けています。一覧・入手元は `arm/README.md` を参照。

## 現在の調査タスク

1. ~~R9A09G087M48GBG 搭載 試作初号機の JTAG デバッガ（PALMiCE4 Model-J / CSIDE）接続不可の切り分け。~~
   → **解決済み（2026-09-12）: ハードウェア不具合。0.8 V を入れるべき PLL 電源に 1.8 V を誤って入力していた。**
   （切り分け過程は [07_debug_interface.md](07_debug_interface.md) / [08_jtag_bringup_troubleshooting.md](08_jtag_bringup_troubleshooting.md) に記録済み）
2. **xSPI0 x1 ブート固定＋全コア NORTi の AMP マルチコア・ブリングアップ**（R52 CPU0 → LPDDR4 初期化 → R52 CPU1 + Cortex-A55 Core0〜3 を個別ロード・起動）のロードマップ整備。
   → [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md)（xSPI0 x1 ブート詳細＋ランタイム操作）
   → [10_amp_multicore_bringup_roadmap.md](10_amp_multicore_bringup_roadmap.md)（全体ロードマップ：DDR初期化、TZC-400、マルチコア起動レジスタ手順）
3. **xSPI1 に Everspin MRAM (EM064LXOAB320IS2T) を8D-8D-8D接続**できるかの検証。
   → [14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md)
   （結論: 可能。プロトコル互換性・レジスタ設定・書き込み特有の優位点を整理）
   → [15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md)
   （メモリマッピングモードでの8D-8D-8D書き込みを実現するための、レジスタ単位の詳細初期化手順）
   → [16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md)
   （ソフトウェア担当者向けMRAM基礎知識まとめ）
   → [17_mram_est3000_factory_initialization.md](17_mram_est3000_factory_initialization.md)
   （EST3000準拠：工場出荷後に1回だけ必要な初回初期化(DFIM)手順。ソフトウェア実装が必要）

## ドキュメント一覧

| ファイル | 内容 |
|----------|------|
| [01_overview.md](01_overview.md) | チップ概要、主要スペック、製品ラインナップ、RZ/N2H と RZ/T2H の違い、ブロック図（テキスト） |
| [02_manual_toc.md](02_manual_toc.md) | ユーザーズマニュアルの章・節インデックス（PDF ページ番号付き）。調査時にジャンプ先を探すための地図 |
| [03_memory_map.md](03_memory_map.md) | アドレス空間、統合メモリマップ、各周辺モジュールのベースアドレス |
| [04_boot_modes.md](04_boot_modes.md) | 動作モード（7 ブートモード）、MDn / MDD / MDV 端子、ブートフロー、ローダ用パラメータ、OTP 認証 |
| [05_clocks_reset.md](05_clocks_reset.md) | クロック発生回路（PLL0〜4、内部クロック一覧）、リセット要因、CLMA |
| [06_hardware_design_guide.md](06_hardware_design_guide.md) | ハードウェアデザインガイドの要点（電源シーケンス、発振回路、各 I/F の接続・レイアウト、未使用端子処理） |
| [07_debug_interface.md](07_debug_interface.md) | マニュアル 10 章詳細（JTAG/SWD/ETR、BSCANP、TRST#/RES# 接続シーケンス、OCD 認証レジスタ、CoreSight アドレスマップ） |
| [08_jtag_bringup_troubleshooting.md](08_jtag_bringup_troubleshooting.md) | 試作ボードで JTAG が繋がらないときの切り分け（仮説の優先順位、物理チェックリスト、推奨ブリングアップ手順）※解決済み事案の記録 |
| [09_xspi0_x1_boot_and_runtime.md](09_xspi0_x1_boot_and_runtime.md) | xSPI0 x1 ブートモードの詳細（ブート ROM 挙動、ローダ配置制約）とランタイムでの xSPI 操作（メモリマッピング／マニュアルコマンド／XiP 直接実行の可否／NOR フラッシュ書き込みの可否） |
| [10_amp_multicore_bringup_roadmap.md](10_amp_multicore_bringup_roadmap.md) | R52C0→R52C1+A55Core0-3 の AMP マルチコア・ブリングアップ ロードマップ（Master MPU、DDRSS 初期化、TZC-400、アドレス拡張、各コア起動レジスタ手順、ソフトウェア割り込みでのコア間通知）。冒頭にメモリマップ SVG 図＋ Mermaid シーケンス図あり |
| [11_memory_performance_comparison.md](11_memory_performance_comparison.md) | TCM / SYSRAM / LPDDR4 の速度比較。R52+TCM が最速・最も決定的である理由、A55 に TCM がない制約、混在配置の指針、A55/R52 のキャッシュ構成差、TCM/SYSRAM/DDR/xSPI 各メモリでのキャッシュ有無 |
| [12_ddr_noncacheable_region_setup.md](12_ddr_noncacheable_region_setup.md) | DDR 上のコア間共有 IPC 領域を「非キャッシュ」にする実装方法（R52 の MPU／A55 の MMU、MAIR 属性設定。※ Arm アーキテクチャ一般知識、RZ/N2H マニュアル非記載である旨を明記）|
| [13_xspi_protocol_modes_and_quad_flash.md](13_xspi_protocol_modes_and_quad_flash.md) | xSPI プロトコルモード（1S-1S-1S〜4S-4S-4S等）の記法とRZ/N2Hが対応する7種類の一覧（RZ/N2Lと同一IPブロックのため列挙値・制約は共通）、Quad SPI NORフラッシュ接続時の実践的な設計、RZ/N2Lとの仕様差分（スループット266MB/s、マルチスレーブ数、アドレス空間サイズ） |
| [14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md) | Everspin MRAM (EM064LXOAB320IS2T) を xSPI1 に 8D-8D-8D 接続する検証。プロトコル互換性（プロファイル1.0との一致）、レジスタ設定、MRAM特有の書き込み優位点（消去不要・WREN1回のみ）、電圧/クロック信号の確認、要検証事項 |
| [15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md) | MRAMへの8D-8D-8Dメモリマッピング書き込みを実現するための初期化手順をレジスタ単位で詳細化。端子設定(PMCm/PFCm)、xSPI1モジュール有効化(PRCRN/PRCRS, MRCTLA/MSTPCRA, SSTPCR6のREQ/ACK)、1S-1S-1SマニュアルコマンドでのWREN/VCR0書き込み、プロトコル切替の同期ポイント、メモリマッピング有効化、動作検証手順 |
| [16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md) | ソフトウェア担当者向けMRAM基礎知識まとめ。STT-MRAMの特性、Persistent Memory Mode、WELのスティッキー動作、対応プロトコルモード、主要設定レジスタ(NVCR/VCR)、リセット/Deep Power Down挙動、電気的基礎 |
| [17_mram_est3000_factory_initialization.md](17_mram_est3000_factory_initialization.md) | Everspin Application Note EST3000準拠。工場出荷後（リフロー半田付け後）に1回だけ必要な初回初期化(DFIM)手順。Status Register/NVCR0-12/OTP領域/メモリアレイ全体の初期化シーケンス、JESD252シグナルシーケンスリセットの詳細、電源投入タイミング要件(tPU=350µs)、リカバリフロー、RZ/N2Hマニュアルコマンドへの実装対応 |
| [appendix_full_toc.md](appendix_full_toc.md) | マニュアル目次の全階層（レジスタ名まで） |

## 元テキストの扱い（調査用）

PDF から抽出したテキストを `work/` 配下に置いています（Git 管理対象外を想定）。

- `work/manual_full.txt` … マニュアル全文（`pdftotext -layout`）
- `work/pages/pXXXX.txt` … マニュアルを 1 ページ 1 ファイルに分割（`pXXXX` = PDF ページ番号と一致）
- `work/hwguide_full.txt` … ハードウェアデザインガイド全文
- `work/toc_full.md` / `work/toc_lvl01.md` … 目次（ブックマーク）抽出

再生成する場合:

```bash
pdftotext -layout r01uh1039jj0130-rzt2h-rzn2h.pdf work/manual_full.txt
mkdir -p work/pages
python3 -c "d=open('work/manual_full.txt').read().split('\x0c'); [open(f'work/pages/p{i:04d}.txt','w').write(p) for i,p in enumerate(d,1)]"
```

特定トピックを調べるときは [02_manual_toc.md](02_manual_toc.md) でページを特定し、`work/pages/pXXXX.txt` を読むのが速いです。

## 既知の要点メモ（調査で判明）

- 対象製品 **R9A09G087M48GBG** = RZ/N2H、**576 ピン FCBGA**、**セキュリティ対応品**、Cortex-A55 クワッド ＋ Cortex-R52 ×2。`MD_MON.MDP` bit8 = 1（RZ/N2H）。
- CoreSight デバッグには **`BSCANP` = Low 必須**（High はバウンダリスキャン）。576pin: ボール AA6。
- JTAG 接続シーケンス: `RES#`Low & `TRST#`Low → `TRST#`High（SWJ-DP TAP）→ `RES#`High（AP=0 OCD）→ 認証 & ブートコード完了 → デバッグ許可。
- セキュリティ品固有: **`MDD` = High でハッシュ JTAG 認証モード**（ブリングアップ時は Low）。`MDD` は 576pin ボール AD7 で `XSPI0_RESET0#` と兼用、`VDD1833_4` ドメイン。
- OTP 未書き込みなら `AUTHMODEJ = 0x000` = 認証なし（デバッグ許可）。`AUTHMODEJ = 1xx` は JTAG 永久禁止。
- デバッグは段階的に解禁: **②SWJ-DP TAP と ③AP=0 OCD は RES# 保持中（ブート前）に到達可、フラッシュ内容に無関係**。**④の本格デバッグ（CoreSight ROM 列挙・コア halt）だけが「内蔵ブートコード実行の完了後」に解禁**（10.3.3 / 図 10.4・10.5）。
- xSPI ブートでフラッシュ空／CHECK_SUM 不一致時のブート ROM 終端状態は**マニュアルに記載なし**（SCI/USB ブートのみ「エラーコード返却・中断」と明記）。→ まず **"connect under reset"** で ② まで到達するかを確認。
- MD2:0 を振れるなら初回は **SCI ブート（101b）/ USB ブート（110b）**（失敗挙動が仕様で定義されている）。
- **xSPI1へのMRAM (Everspin EM064LXOAB320IS2T) 8D-8D-8D接続は可能**。MRAMのxSPIフレーム構造（コマンド反復送出方式）はRZ/N2Hの「8D-8D-8Dプロファイル1.0」（`CMCFG0CSn.FFMT=01`）と一致。さらに①消去(Erase)不要のPersistent Memory Mode、②Write EnableはWEL自動クリアなしで初回のみでよい、という2点により**書き込みもメモリマッピングモードでそのまま可能**（通常のNORフラッシュには無い優位性）。詳細は[14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md)、レジスタ単位の初期化手順は[15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md)。
- **RZ/N2Hのユニット別xSPI端子の非対称性に注意**：`XSPIm_CKN`（差動クロック負側）と`XSPIm_RESET0#/1#`等（表37.2 注1）は**ユニット0（xSPI0）専用**で、xSPI1には端子自体が存在しない。xSPI1にMRAM等を接続する場合、クロックは元々単相前提でよいが、**ソフトウェアからのハードウェアリセット制御はできない**（外部プルアップ、または`0x66`+`0x99`のソフトウェアリセットコマンドに頼る設計が必要）。
- **xSPI1の信号(`XSPI1_*`)はP01/P02ポートの汎用GPIOと共用ピン**（PFC機能コード`0x1C`）であり、専用ブート端子ではない。使用前に`PMCm`/`PFCm`の設定（`PRCRS.PRC2`のロック解除を伴う）が必要（[15](15_mram_octal_memory_mapped_write_init.md) §2）。
- **RZ/N2Hのモジュール有効化はRZ/N2Lより1段階多い**：①`MRCTLA`でモジュールリセット解除→②`MSTPCRA`でモジュールストップ解除→③`SSTPCR6`の`xxx_REQ`/`xxx_ACK`によるスレーブバスストップ解除、の3段階が必要（13.4.3）。RZ/N2Lの`MSTPCRA`のみで完結する単純な方式とは異なる。
- **MRAMはリフロー半田付け直後、内部状態が未定義**（Everspin Application Note EST3000）。運用開始前に**工場出荷後1回だけ**、Status Register・NVCR0-12・OTP領域・メモリアレイ全体を`0x00`→`0xFF`で強制的に埋めて検証する「DFIM（Device Factory Initialization Mode）」初期化が必須。125℃1時間以上の温度暴露やリワークでも再実施が必要。通常運用の起動シーケンス（[15](15_mram_octal_memory_mapped_write_init.md)）とは別物で、量産テスト工程等での実施を想定。詳細は[17_mram_est3000_factory_initialization.md](17_mram_est3000_factory_initialization.md)。
- 通常の電源投入・リセット時も、MRAMへの最初のコマンド発行前に**`tPU=350µs`（電源投入後）または`200ns`（リセット後）のウェイトが必要**（EST3000 Figure 2）。[15](15_mram_octal_memory_mapped_write_init.md)には未記載だった抜けているタイミング要件。
