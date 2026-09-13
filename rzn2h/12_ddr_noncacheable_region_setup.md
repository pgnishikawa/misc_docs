# DDR に「キャッシュ無効領域」を設定する方法（コア間共有 IPC 領域向け）

[← README（目次）へ戻る](README.md)

対象読者: [10_amp_multicore_bringup_roadmap.md](10_amp_multicore_bringup_roadmap.md) §6.2、[11_memory_performance_comparison.md](11_memory_performance_comparison.md) §5 で述べた「共有領域は非キャッシュにする」を実際にどう設定するか。

> **重要な前提**: この設定は **Cortex-A55 の MMU** および **Cortex-R52 の MPU** という **Arm アーキテクチャ標準の仕組み**で行うものであり、RZ/N2H 固有のレジスタではない。RZ/N2H マニュアルの Cortex-A55／Cortex-R52 の各章は「CPU に関する制限事項の詳細は Arm の Web サイトの情報を参照してください」（2.3節/3.3節）と明記しており、MPU/MMU のレジスタレベルの記載自体を持たない。
>
> **本ドキュメントの内容は、RZ/N2H が搭載する実際のコアリビジョンに対応する Arm 公式ドキュメントを取得・確認して記載している**（`arm/` ディレクトリに格納。出典は各節に明記）:
> - **Cortex-R52**: `arm/cortex_r52_trm_r1p3.pdf`（Technical Reference Manual）、`arm/cortex_r52_programmers_guide.pdf`（Programmer's Guide）。RZ/N2H マニュアルは revision **r1p4-00rel0** と記載しているが、入手できたのは r1p3。MPU/TCM/キャッシュのアーキテクチャ的挙動に版差の影響はない見込み（詳細は `arm/README.md`）。
> - **Cortex-A55**: `arm/cortex_a55_trm_r2p0.pdf`。RZ/N2H マニュアル記載の revision **r2p0** と**完全一致**。
>
> ### ★確定仕様: Cortex-A55 は AArch32 で使用する
> RZ/N2H マニュアル 表2.1（p.176）は A55 の「命令セット」を **「Armv8.2-A A64 命令セット (AArch64)」**と記載しているが、これは Arm® Cortex®-A55 Core TRM の実装オプション一覧（Table A1-1）に「AArch32 サポートの有無」という構成オプション自体が存在しないこと、および Table A1-2 に **「AArch64 and AArch32 execution states at all Exception levels」**（全 Exception Level で AArch64 と AArch32 の両方の実行状態をサポート）と明記されていることから、**AArch32 実行状態はこの A55 コアで常に利用可能**（Renesas の表記は「A64 命令セットにも対応する」という趣旨の要約であり、AArch32 が使用不可という意味ではない）。したがって **A55 を AArch32 で使用するという要件はこの実装で技術的に成立する**。
> ただし AArch32 では **MMU のレジスタ体系そのものが AArch64 とは別物**になる（§2 で詳述）。以前の版では A55 のページテーブル属性設定に AArch64 の `MAIR_EL1` 等の名称を使っていたが、**AArch32 使用を前提に全面的に書き直した**。

実務上は、**NORTi が MPU/MMU 設定用の API（リージョン登録、メモリ属性指定など）を提供しているはずなので、まずそちらを使うのが最も安全で簡単**。以下はその API が何をレジスタレベルで行っているかの背景知識、および API が無い場合の素の実装方法。

## 0. 設計方針（まず決めるべきこと）

- **推奨: DDR アドレスマップ上に、共有 IPC 専用の固定領域をあらかじめ 1 箇所確保し、その領域は最初から最後まで「非キャッシュ」としてのみマッピングする。** 動的にキャッシュ属性を切り替える設計は避ける（後述 §2.7 のとおり、属性変更は「同じ物理アドレスに異なるキャッシュ属性でマッピングしてはいけない」という Arm アーキテクチャ上の制約があり、正しく行うには break-before-make 手順とキャッシュのクリーン/インバリデートが必要で複雑・バグりやすい）。
- [10_amp_multicore_bringup_roadmap.md](10_amp_multicore_bringup_roadmap.md) §3.1 の DDR パーティション例に、たとえば `コア間 IPC 共有領域` 専用の数十 KB〜数 MB のレンジを固定で切っておき、**そのレンジだけは R52 CPU0 の MPU、R52 CPU1 の MPU、A55 各コアの MMU（このレンジを使うコアのみでよい）のすべてで、最初から Non-cacheable として登録する**。

## 1. Cortex-R52（Armv8-R, AArch32 のみ）: MPU でのキャッシュ属性設定

出典: `arm/cortex_r52_trm_r1p3.pdf` §3.3.85〜3.3.87（PRBAR/PRLAR/PRSELR）、§3.3.70（MAIR0/MAIR1）、Chapter 8（About the MPU / MPU regions）／`arm/cortex_r52_programmers_guide.pdf` Chapter 6・11。

### 1.1 重要な訂正: R52 は AArch64 の `_EL1` 系レジスタを持たない

Cortex-R52 は **Armv8-R **AArch32** 専用**コア（AArch64 実行状態を持たない）。したがって MMU/MPU 設定は **CP15 コプロセッサレジスタ**（`MRC`/`MCR p15, ...` 命令でアクセス）で行う。レジスタ名は `PRBAR_EL1` のような AArch64 風の名前ではなく、**`PRBAR`／`PRLAR`／`PRSELR`／`MAIR0`／`MAIR1`**（EL1 用）、**`HPRBAR`／`HPRLAR`／`HPRSELR`／`HMAIR0`／`HMAIR1`**（EL2 用、"H" = Hyp）という素の名前を使う。

### 1.2 レジスタのビット定義（TRM より正確に転記）

**PRBAR**（32 bit、間接アクセスは `MRC/MCR p15, 0, Rt, c6, c3, 0`）:

| ビット | フィールド | 内容 |
|--------|-----------|------|
| [31:6] | `BASE` | リージョンの下限アドレス（64 バイト境界にアライン、下位 6 bit は自動的に 0）|
| [5] | — | RES0（予約）|
| [4:3] | `SH[1:0]` | 共有属性: `00`=Non-shareable, `01`=UNPREDICTABLE, `10`=Outer Shareable, `11`=Inner Shareable |
| [2:1] | `AP[2:1]` | アクセス権限: `00`=EL1 RW/EL0 なし, `01`=EL1 RW/EL0 RW, `10`=EL1 RO/EL0 なし, `11`=EL1 RO/EL0 RO |
| [0] | `XN` | 実行不可（Execute-Never）|

**PRLAR**（32 bit、間接アクセスは `MRC/MCR p15, 0, Rt, c6, c3, 1`）:

| ビット | フィールド | 内容 |
|--------|-----------|------|
| [31:6] | `LIMIT` | リージョンの上限アドレス（下位 6 bit は `0x3F` を補完して上限アドレスを算出。つまり上限は 64 バイト境界の 1 つ手前にアライン）|
| [5:4] | — | RES0 |
| [3:1] | `AttrIndx[2:0]` | **`MAIR0`／`MAIR1`（EL2 なら `HMAIR0`／`HMAIR1`）内の属性エントリ番号を指す**（3 bit = 0〜7）|
| [0] | `EN` | リージョン有効化（`1`=有効。リセット後は `0`）|

**PRSELR**（リージョン選択、`MRC/MCR p15, 0, Rt, c6, c2, 1`）: RZ/N2H は **EL1/EL2 とも 24 リージョン実装**のため、`REGION[4:0]`（5 bit、0〜23 を指定。24 以上は UNPREDICTABLE）を使う変種（16 リージョン実装なら `REGION[3:0]`4 bit）。

**直接アクセス**: 上記の間接方式（PRSELR で選んでから PRBAR/PRLAR を読み書き）に加え、**PRBAR0〜PRBAR15／PRLAR0〜PRLAR15 への直接アクセス**も可能（`n` が 16〜23 の場合は間接方式のみ）:

```
MRC p15, 0, <Rt>, c6, c8+n[3:1], 4*n[0]     ; PRBARn を読む
MCR p15, 0, <Rt>, c6, c8+n[3:1], 4*n[0]     ; PRBARn へ書く
MRC p15, 0, <Rt>, c6, c8+n[3:1], 4*n[0]+1   ; PRLARn を読む
MCR p15, 0, <Rt>, c6, c8+n[3:1], 4*n[0]+1   ; PRLARn へ書く
```

### 1.3 MAIR0／MAIR1（メモリ属性エンコーディング、TRM Table 3-106/3-107 より正確に転記）

`MAIR0`（`MRC/MCR p15, 0, Rt, c10, c2, 0`）は `AttrIndx` 0〜3（Attr0〜Attr3）、`MAIR1`（`p15, 0, Rt, c10, c2, 1`）は `AttrIndx` 4〜7（Attr4〜Attr7）を保持する 32 bit レジスタ（各 8 bit × 4）。1 バイトの属性値 `Attr<n>` は上位 4 bit（Outer）と下位 4 bit（Inner、Device の場合は種別）で構成:

| `Attr<n>[7:4]`（Outer）| 意味 |
|---|---|
| `0000` | Device memory（Inner 側で種別を指定）|
| `0100` | Normal memory, Outer **Non-Cacheable** |
| `1111` | Normal memory, Outer Write-Back, Read/Write-Allocate（非transient）|
| （他）RW ビットで Write-Through/Write-Back・Allocate 有無の組み合わせが選べる | |

| `Attr<n>[3:0]`（`[7:4]=0000` の場合、Device 種別）| 意味 |
|---|---|
| `0000` | **Device-nGnRnE** |
| `0100` | Device-nGnRE |
| `1000` | Device-nGRE |
| `1100` | Device-GRE |

→ よく使う 3 値（**Arm TRM に明記された値そのもの**）:

| 値 | 意味 | 用途 |
|----|------|------|
| **`0xFF`** | Normal memory, Outer/Inner Write-Back, Read/Write-Allocate | 通常のキャッシャブル領域（プライベートコード/データ）|
| **`0x44`** | Normal memory, Outer/Inner **Non-Cacheable** | ★共有 IPC 領域用 |
| **`0x00`** | **Device-nGnRnE** | 周辺レジスタ等、順序も厳密に保証したい場合 |

### 1.4 設定手順の実例（EL1 MPU、`arm/cortex_r52_programmers_guide.pdf` の記法に準拠）

```asm
; --- 1. MAIR0 に属性テーブルを設定 ---
LDR  r0, =0x000044FF      ; Attr1=0x44(Non-cacheable), Attr0=0xFF(WB Cacheable)
MCR  p15, 0, r0, c10, c2, 0   ; Write MAIR0

; --- 2. リージョン n（例: n=5）を選択 ---
MOV  r0, #5
MCR  p15, 0, r0, c6, c2, 1    ; Write PRSELR

; --- 3. PRBAR（ベースアドレス + SH + AP + XN）---
LDR  r0, =(SHARED_IPC_BASE | (0b10 << 3) | (0b01 << 1) | 0)  ; SH=Outer Shareable, AP=RW/RW, XN=0
MCR  p15, 0, r0, c6, c3, 0    ; Write PRBAR

; --- 4. PRLAR（リミットアドレス + AttrIndx=1 + EN=1）---
LDR  r0, =((SHARED_IPC_LIMIT & 0xFFFFFFC0) | (1 << 1) | 1)   ; AttrIndx=1(Non-cacheable), EN=1
MCR  p15, 0, r0, c6, c3, 1    ; Write PRLAR

DSB                            ; それまでのメモリアクセス完了を保証
ISB                            ; パイプラインフラッシュ、設定を確実に反映
```

- **リージョンを再設定する際は、設定変更を行っているコード自体がそのリージョン内に無いことを確認し、変更前に `DSB`、変更後に `ISB` を実行すること**（Programmer's Guide 6章に明記）。
- **R52 CPU0 と CPU1 は互いに独立した MPU を持つ**（コアごとに別インスタンス）。共有領域を使うなら、**両方の CPU で同じアドレスレンジに同じ設定を入れる**必要がある。

### 1.5 リージョンの制約（TRM Chapter 8、Programmer's Guide 6章より）

- **最小リージョンサイズは 64 バイト**。ベースアドレスは 64 バイト境界にアライン、上限アドレスは 64 バイト境界の 1 つ手前にアライン。
- **リージョンは重複してはならない**（Armv7-R までと異なり、Armv8-R の PMSA は重複禁止。複数リージョンにヒットするアクセスは **Translation fault** になる）。
- RZ/N2H の R52 は EL1/EL2 とも 24 リージョン実装（[01_overview.md](01_overview.md)）。TCM・DDR ミラー窓・周辺機能・共有 IPC 領域などシステム全体で 24 本に収まるよう設計すること。

### 1.6 ★重要な発見: TCM はそもそも MPU のキャッシュ属性設定を無視する（Arm 公式に明記）

`arm/cortex_r52_programmers_guide.pdf` Chapter 5 に、以前「一般的な Arm アーキテクチャ知識」として説明した内容が **Arm 公式ドキュメントで文字通り明記**されていることを確認した:

> **"TCM is always accessed as Non-cacheable Non-shareable Normal memory, and ignores the memory attribute of the TCM memory region in the MPU."**
> （TCM は常に Non-cacheable・Non-shareable な Normal memory としてアクセスされ、MPU 上の TCM 領域に設定されたメモリ属性を無視する）

さらに:
- **TCM はオーバーラップしたメモリ領域より優先度が高い**（"TCM has a higher priority if the TCM region is overlapped with another memory region"）。
- したがって R52 の TCM 領域では、MPU で設定すべきは **アクセス権限（AP/XN）だけ**でよく、キャッシュ属性（AttrIndx）は実質無視される。

### 1.7 ★重要な発見: R52 のキャッシュ・コヒーレンシに関する Arm 公式の明記

同じく Programmer's Guide Chapter 5 より、これまで「一般的な知識」としていた内容が公式に確認できた:

> **"Cortex-R52/R52+ processors do not implement the cache coherency logic between the cores in a cluster."**（クラスタ内のコア間でキャッシュコヒーレンシロジックを実装していない）
> **"Cortex-R52/R52+ processors do not cache data that is marked as sharable."**（Shareable 属性が付いたデータはキャッシュしない）
> **"All cache maintenance instructions are performed locally... not broadcast to any other core."**（キャッシュメンテナンス命令は自コアのみに作用し、他コアには伝播しない）
> **"The write behavior for the data cache is always write-through whatever the region attribute is write-back or write-through."**（D キャッシュの書き込み動作は、リージョン属性が Write-Back でも Write-Through でも、常に Write-Through 動作）

これらから実務上言えること:
- **R52 CPU0/CPU1 間のキャッシュ不整合リスクは Arm 公式に確認済みの事実**であり、[10_amp_multicore_bringup_roadmap.md](10_amp_multicore_bringup_roadmap.md) §6.2 の対策（非キャッシュ属性化、または明示的なクリーン/インバリデート）は正しい。
- **R52 側だけの簡易策として、共有 IPC 領域を `SH=Shareable`（Outer/Inner いずれか）にマークするだけでも、R52 はそのデータをキャッシュしなくなる**（Non-cacheable な AttrIndx を明示的に選ばなくても、Shareable 指定だけで同等の効果が得られる）。ただし **これは R52 固有の実装挙動であり、Cortex-A55（Armv8-A）では Shareable な Normal memory は通常どおりキャッシュされる**（コヒーレンシはハードウェアスヌープで別途保証する設計のため）。したがって **R52⇔A55 間の共有領域では、A55 側は依然として明示的に Non-cacheable にするか、キャッシュメンテナンスを行う必要がある**（この trick は R52 側の保険にはなるが、A55 側の対策の代わりにはならない）。
- **R52 の D キャッシュは常に Write-Through 動作**なので、R52 が書いたデータが（キャッシュ属性に関わらず）比較的早く実メモリに反映される点は、他コアとの整合性確保にとって有利な特性ではある（ただし「読む側」がキャッシュ済みの古い値を返す可能性は残るため、読む側の対策は別途必要）。

## 2. Cortex-A55（★ AArch32 実行状態で使用）: MMU でのキャッシュ属性設定

出典: `arm/cortex_a55_trm_r2p0.pdf` Chapter A5（Memory Management Unit）、Chapter B1（AArch32 system registers）§B1.83-85（TTBCR/TTBCR2/TTBR0）。

### 2.1 ★重要な訂正: A55 を AArch32 で使う場合、AArch64 の `_EL1` 系レジスタは使わない

A55 は AArch64/AArch32 両方をサポートするコアだが、**ソフトウェアが AArch32 状態で動作している間は、MMU 設定も AArch32 の CP15 コプロセッサレジスタ体系**（`MRC`/`MCR p15, ...`）**で行う**。`TTBR0_EL1`・`MAIR_EL1`・`TCR_EL1` のような AArch64 名は使わない。AArch32 側の対応レジスタは **`TTBR0`／`TTBR1`／`TTBCR`／`TTBCR2`／`MAIR0`／`MAIR1`**（EL1 用）であり、これは **Cortex-R52 の MPU が使うレジスタ名（`MAIR0`／`MAIR1`）と文字通り同じ名前・同じアクセス命令**である（§2.4 で詳述）。

### 2.2 Short-descriptor と Long-descriptor（LPAE）の 2 方式（TRM §B1.83 より）

A55 の AArch32 MMU には **2 つの変換テーブル形式**があり、`TTBCR.EAE`（Extended Address Enable）ビットで選択する:

| `TTBCR.EAE` | 形式 | 物理アドレス | 属性指定方式 |
|---|---|---|---|
| `0` | **Short-descriptor**（Armv5/v6/v7 互換）| **最大 32 bit（4 GB）のみ** | `TEX`/`C`/`B` ビット直接指定（`MAIR` 不使用）、`DACR` によるドメイン管理 |
| `1` | **Long-descriptor（LPAE）** | **A55 の実装上限まで（40 bit）** | `AttrIndx` ＋ `MAIR0`／`MAIR1`（Cortex-R52 と同じ方式）|

**RZ/N2H の DDR は `0x2_0000_0000`〜（35 bit 物理アドレス空間）にマップされており、4 GB を超える**（[03_memory_map.md](03_memory_map.md)）。Short-descriptor 形式では 32 bit 物理アドレスしか表現できないため、**A55 が DDR に直接（32 bit ミラー窓を使わずに）アクセスするには `TTBCR.EAE=1`（Long-descriptor/LPAE）が事実上必須**。本書は以降 **LPAE を前提**に記載する（NORTi の A55 AArch32 ポートが実際にどちらを使うかは要確認だが、DDR 直接アクセスの要件からは LPAE一択と考えられる）。

> 32 bit 物理アドレスに収まる範囲（DDR ミラー窓経由、[03_memory_map.md](03_memory_map.md) の `0x0_C000_0000`〜等）だけで完結させるなら Short-descriptor でも動作しうるが、その場合は `MAIR`／`AttrIndx` の概念自体が無く、本書の Non-cacheable 設定方法は当てはまらない（`TEX`/`C`/`B` ビットで直接 Non-cacheable なタイプを選ぶ、別の設定方法になる）。

### 2.3 TTBCR（Long-descriptor 形式、TRM §B1.83.2 より正確に転記）

`TTBCR`（`MRC/MCR p15, 0, Rt, c2, c0, 2`）は 32 bit レジスタ。`EAE=1` 時のビット配置:

| ビット | フィールド | 内容 |
|---|---|---|
| [31] | `EAE` | `1` = Long-descriptor (LPAE) 使用 |
| [29:28] | `SH1` | `TTBR1` 用テーブルウォークの共有属性 |
| [27:26]/[25:24] | `ORGN1`/`IRGN1` | `TTBR1` 用テーブルウォークの Outer/Inner キャッシュ属性 |
| [23] | `EPD1` | `TTBR1` を使ったテーブルウォークの禁止 |
| [18:16] | `T1SZ` | `TTBR1` がカバーする領域サイズ（$2^{32-T1SZ}$ バイト）|
| [13:12]/[11:10]/[9:8] | `SH0`/`ORGN0`/`IRGN0` | `TTBR0` 用（`TTBR1` と同様の意味）|
| [7] | `EPD0` | `TTBR0` を使ったテーブルウォークの禁止 |
| [2:0] | `T0SZ` | `TTBR0` がカバーする領域サイズ |

`ORGN`/`IRGN`/`SH` は**テーブルウォーク自体（ページテーブルを読みに行くメモリアクセス）のキャッシュ属性**であり、共有 IPC 領域そのものの属性ではない点に注意（領域自体の属性は §2.5 の `AttrIndx` で指定）。

`TTBR0`／`TTBR1`（`MRRC/MCRR p15, 0, RtLow, RtHigh, c2` で 64 bit 値として読み書き。32 bit アクセスも可）は変換テーブルの物理ベースアドレス（`BADDR`）を保持。LPAE 形式でのビット配置の詳細は TRM 自身が「Arm® Architecture Reference Manual Armv8-A (DDI0487) を参照」と明記しており未取得——**一般に知られる配置は AArch64 の `TTBR0_EL1` とほぼ同型**（`BADDR` フィールド＋`ASID`）だが、最終確認は DDI0487 で行うこと。

### 2.4 MAIR0／MAIR1 は Cortex-R52 と同一（★実装の大きな単純化ポイント）

TRM の AArch32 レジスタ一覧に `MAIR0`／`MAIR1` が Cortex-R52 と全く同じ形で存在する（これは Armv8 アーキテクチャが AArch32 の PMSA（R プロファイル）と VMSA-LPAE（A プロファイル）で **同じ Memory Attribute Indirection Register 機構を共有**しているため）。

- アクセス命令: `MRC/MCR p15, 0, Rt, c10, c2, 0`（`MAIR0`）／`MRC/MCR p15, 0, Rt, c10, c2, 1`（`MAIR1`）— **§1.2 で示した Cortex-R52 の命令と完全に同一**。
- 属性エンコーディング（`0xFF`=Normal WB Cacheable、`0x44`=Normal Non-cacheable、`0x00`=Device-nGnRnE 等）も **§1.3 の表がそのまま適用できる**。

→ **実務上の利点**: R52（PMSA）と A55（AArch32 の VMSA-LPAE）を同じシステムで併用する場合、**`MAIR0`／`MAIR1` の設定コードをほぼ共通化できる**（アクセス命令もエンコーディング値も同じ）。両者で違うのは「属性をどこで参照するか」——R52 は `PRLAR.AttrIndx`（MPU リージョン）、A55 は変換テーブルの各記述子内の `AttrIndx` フィールド（次項）。

### 2.5 ページ/ブロック記述子の `AttrIndx`（LPAE、TRM に厳密なビット位置の記載なし）

Long-descriptor（LPAE）形式の変換テーブル記述子は 64 bit で、AArch64 の Block/Page 記述子と同系統の構造（Lower Attributes に `AttrIndx`/`AP`/`SH`/`AF`/`nG`、Upper Attributes に `XN`/`PXN` 等）を持つ。TRM はこの正確なビット位置を示しておらず「Arm® Architecture Reference Manual Armv8-A (DDI0487) を参照」を指示している。**一般に広く知られている配置**（AArch64 版と同じ、Linux カーネルや Arm Trusted Firmware 等の実装で一貫して使われている値）は次のとおりだが、**最終確認は DDI0487（AArch32 Long-descriptor 記述子の章）で行うこと**:

> - Lower attributes: `AttrIndx[2:0]` = bit[4:2]、`NS` = bit[5]、`AP[1:0]` = bit[7:6]、`SH[1:0]` = bit[9:8]、`AF` = bit[10]、`nG` = bit[11]
> - Upper attributes: `Contiguous` = bit[52]、`PXN` = bit[53]、`XN` = bit[54]

### 2.6 典型的な設定手順（AArch32／LPAE、疑似コード）

```asm
; --- 1. MAIR0 に属性テーブルを設定（R52 と同一の命令・値）---
LDR  r0, =0x000044FF          ; Attr1=0x44(Non-cacheable), Attr0=0xFF(WB Cacheable)
MCR  p15, 0, r0, c10, c2, 0   ; Write MAIR0

; --- 2. TTBCR で LPAE を有効化（初期化時に一度）---
;    EAE=1, T0SZ/T1SZ 等はメモリマップ設計に応じて設定

; --- 3. 共有 IPC 領域をカバーする変換テーブル記述子を作成/更新 ---
;    AttrIndx = 1 (Non-cacheable), SH = Outer Shareable, AP = 用途に応じて, AF = 1

; --- 4. 変更した仮想アドレス範囲に対して TLB invalidate ---
MCR  p15, 0, r0, c8, c7, 1    ; TLBIMVA 相当（対象アドレスを r0 に設定）

DSB
ISB
```

### 2.7 ★注意: 既にキャッシャブルとしてマップ済みの領域を後から Non-cacheable に変更する場合

Arm アーキテクチャ上、**同じ物理アドレスに対して異なるキャッシュ属性のマッピングが同時に存在する状態（mismatched memory attributes）は未定義動作を招く**とされている（AArch32/LPAE でも AArch64 と同じ制約）。もし共有 IPC 領域を後から Non-cacheable に切り替える設計にする場合は、少なくとも以下が必要:

1. 対象範囲への新規アクセスを一時停止する。
2. 既存のキャッシュ内容を **クリーン（write-back）＋インバリデート** する（該当コアの D キャッシュ）。
3. 変換テーブル記述子を Non-cacheable に書き換え、TLB invalidate。
4. DSB/ISB で同期。

→ **これが複雑でバグりやすいため、§0 で述べたとおり「最初から Non-cacheable 専用領域として設計する」ことを強く推奨する。**

### 2.8 A55 クラスタ内のコヒーレンシ（TRM 記載、AArch32/AArch64 で変わらない）

A55 クラスタ（Core0〜3）は DSU 経由の **SCU（Snoop Control Unit）／L3 スヌープフィルタ**でハードウェアコヒーレント（ACE）。この仕組みは実行状態（AArch32/AArch64）に依存しないハードウェア機構なので、**A55 を AArch32 で使っても、4 コア同士が共有する DDR 領域はキャッシャブルのままで問題ない**（明示的なキャッシュメンテナンスは不要）。問題になるのは、このコヒーレンシドメインの**外側**（R52、DMAC 等）とデータをやり取りする場合のみ（[10] §6.2、本書 §1.7）。

## 3. R52 の TCM と A55 の SYSRAM 利用時は別の考慮が必要

- **TCM は MPU のキャッシュ属性設定と無関係**（§1.6 のとおり、TCM はそもそもキャッシュを経由しない専用パスであり、Arm 公式ドキュメントで明記されている）。R52 の TCM 領域は MPU 上で単に「アクセス権限」だけ設定すればよい。
- **SYSRAM は通常のバス経由メモリなのでキャッシュ可能**（RZ/N2H マニュアル 44.5.4／13.5.5 に明記）。SYSRAM を複数コアの共有 IPC に使う場合も、DDR と同様に「共有部分だけ Non-cacheable にする」または「排他アクセス命令を使うためにキャッシャブル・非共有設定にする」という選択が必要（マニュアル 13.5.5 に、SYSRAM への排他アクセス命令を使う唯一の抜け道として「キャッシャブルかつ非共有設定にした場合」という条件が明記されている＝この場合は逆にキャッシャブルにする必要がある、という点に注意）。

## 4. 実装の入口（優先順位）

1. **NORTi のメモリ保護／MPU・MMU 設定 API を確認する。** RTOS がタスクごとのメモリ領域属性（キャッシュ／共有／実行可否）を設定する仕組みを標準で持っているはずなので、まずそのドキュメント・サンプルを確認する。
2. NORTi の設定項目が「Normal Cacheable」「Normal Non-cacheable」「Device」等の選択肢を提供していれば、共有 IPC 領域には Non-cacheable（または Device、順序保証も必要ならこちら）を選ぶだけで済む。
3. 生のレジスタ操作が必要な場合は、本ドキュメント §1／§2 の手順（`arm/` 配下の一次資料に基づく）を参照して実装する。A55 の変換テーブル記述子の正確なビット位置（§2.5）、および `TTBR0`/`TTBR1` の LPAE 形式でのビット位置（§2.3）だけは、DDI0487 での最終確認を推奨。

## 参照

- [10_amp_multicore_bringup_roadmap.md](10_amp_multicore_bringup_roadmap.md) §6.2 — なぜ共有領域に対策が必要か
- [11_memory_performance_comparison.md](11_memory_performance_comparison.md) §5 — キャッシュアーキテクチャ全般
- RZ/N2H マニュアル「2.3 CPUに関する制限事項」p.178、「3.3 CPUに関する制限事項」p.181（いずれも Arm 公式資料への参照のみ）
- `arm/README.md` — 本書で参照した Arm 公式資料の一覧・入手元
- `arm/cortex_r52_trm_r1p3.pdf` §3.3.70, §3.3.85-87, Chapter 8
- `arm/cortex_r52_programmers_guide.pdf` Chapter 5, 6, 11
- `arm/cortex_a55_trm_r2p0.pdf` Chapter A5（MMU概要）、Table A1-1/A1-2（AArch32サポートの確認）、§B1.83-85（TTBCR/TTBCR2/TTBR0、AArch32 LPAE）
- `arm/aarch64_memory_management_guide.pdf`（記述子の一般構造の参考。A55 は AArch32 で使用するため直接の一次資料ではない）
- Arm® Architecture Reference Manual Armv8-A (ARM DDI0487)（A55 の AArch32 Long-descriptor 記述子・TTBR0/1 の正確なビット位置の最終確認用、未取得）

---

[← README（目次）へ戻る](README.md)
