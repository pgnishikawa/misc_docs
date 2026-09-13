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
