# Claude Code 向けグローバル RAG 構成案（組み込み開発最適化）

## 1. 概要・設計思想
組み込み開発において、マイコンのデータシート（1,000ページ超）やRTOS/BSP仕様書、過去機種のソースコードを毎セッション全投入すると、コンテキストウィンドウの枯渇や無駄なトークン消費が発生します。

本構成案では、**「全量読み込み（Full Context）」から「必要時の局所検索（RAG）」への移行**を目的とします。データ特性に応じて**「軽量構造化テキスト（Markdown）」**と**「実体コード・バイナリ（C/C++ / PDF / CAD）」**を分離し、PC全体の共有領域（グローバルストレージ）から効率よくコンテキストへ接続する仕組みを確立します。

---

## 2. 全体ディレクトリ構成
PC内の絶対パスでアクセス可能な共有領域（例: `~/global_context/`）を定義し、ナレッジハブと実体ソースコードを明確に分けて配置します。

```text
~/global_context/
├── 00_Vault/                         # [Knowledge Hub] Markdown形式のインデックス・ナレッジ
│   ├── CLAUDE.md                     # Claude Code向けのグローバル指示書・全体の目次
│   ├── 01_MCUs/                      # マイコン固有ナレッジ（Markdown化済み）
│   │   └── STM32F4/
│   │       ├── usart_registers.md
│   │       ├── timer_registers.md
│   │       └── errata.md
│   ├── 02_RTOS/                      # RTOSの設計ガイド・パラメータ指定
│   │   └── FreeRTOS/
│   │       └── task_design.md
│   ├── 03_BSPs/                      # BSPの利用方法・ドライバの注意点
│   │   └── Vendor_BSP_v2/
│   │       └── peripheral_usage.md
│   ├── 04_Legacy_Assets/             # 過去機種のアーキテクチャ・設計思想
│   │   └── Model_X/
│   │       └── architecture.md
│   └── 05_Schematics/                # 回路図のピンマッピング・GPIO対照表
│       └── Model_X_Pinout.md
│
└── 01_Repositories/                  # [Storage Real Assets] ソースコード・データシート実体
    ├── RTOS/                         # RTOSのソースコード（FreeRTOS, ThreadX等）
    ├── BSPs/                         # マイコン/基板のBSPソースコード
    ├── Legacy_Projects/              # 過去機種の全Gitリポジトリ
    └── Schematics_PDFs/              # 回路図PDF・CADデータ・元資料PDF
```

---

## 3. コンテキスト層分離の設計原則

| データ種別 | 配置場所 | 管理形式 | AIへの見せ方・アクセス方法 |
| :--- | :--- | :--- | :--- |
| **マイコン仕様 / 回路図ピン配置** | `00_Vault/` | 構造化Markdown | Claude Codeの検索ツール（`grep` / `find`）で直接検索・読み込みさせる。 |
| **RTOS / BSPソースコード** | `01_Repositories/` | C/C++ 実体ソースコード | **コードは変換せずそのまま配置。** `00_Vault/` 側からパス（例: `../01_Repositories/BSPs/...`）を示し、必要時のみファイル直接読み込み。 |
| **過去機種コード / 回路図PDF** | `01_Repositories/` | Gitリポジトリ / PDF | PDFは実体を保管。過去コードもリポジトリのまま保持し、ピンポイントで参照させる。 |

---

## 4. PDFマニュアルのMarkdown化手順（詳細）
データシート等のPDF生のテキスト抽出をそのまま持たせると、「表構造の崩れ（レジスタ位置誤認）」「ヘッダー・フッターのノイズ」「巨大トークン消費」が発生します。事前に1度だけAIを用いてMarkdownに整形・構造化します。

### Step 1: 構造化変換プロンプト
PDFからコピーした該当章のテキストを、以下のプロンプトでAI（Web版Claude等）に渡してMarkdown化します。

```text
【指示】
以下のテキストはマイコンのPDFデータシートから抽出した生のテキストです。
Claude Codeが高速かつ最小トークンで検索・理解できるように、以下のルールに従ってMarkdownに変換してください。

1. ヘッダー、フッター、ドキュメントID、ページ番号などの不要ノイズはすべて除去する。
2. レジスタのビット割り当ては必ず「Markdownのテーブル形式（Bit, Field Name, Reset, Description）」にする。
3. 初期化処理やシーケンスは番号付きリストにする。
4. 特記事項や注意点（Errata関連）は引用ブロック（>）で強調する。
```

### Step 2: 変換前後の比較例
* **変換前（生テキスト抽出）：**
  ```text
  DocID022152 Rev 8 450/1422
  26.6.1 USART Status register (USART_SR) Address offset: 0x00 Reset value: 0x00C0
  31 30 29 ... 7 6 5 Reserved TXE TC RXNE
  Bit 7 TXE: Transmit data register empty
  0: Data is not transferred to the shift register
  1: Data is transferred to the shift register
  ```

* **変換後（構造化Markdown）：**
  ```markdown
  ### USART_SR (Status Register)
  - **Offset:** 0x00 | **Reset Value:** 0x00C0

  | Bit | Field | Reset | Description |
  |---|---|---|---|
  | 7 | TXE | 0 | **Transmit data register empty**<br>0: Data not transferred<br>1: Data transferred to shift register |
  | 6 | TC | 1 | **Transmission complete** |
  | 5 | RXNE | 0 | **Read data register not empty** |
  ```

---

## 5. Claude Code との接続設定（CLAUDE.md）
各開発プロジェクトのルート、および `~/global_context/00_Vault/CLAUDE.md` に以下のように参照パスとルールを日本語で記述します。

```markdown
# グローバル組み込みナレッジ インデックス

## ディレクトリマップ
- Markdownナレッジ保管庫: `~/global_context/00_Vault/`
- ソースコード・実体リポジトリ: `~/global_context/01_Repositories/`

## リソースの参照先一覧
1. **マイコンレジスタ・仕様書**: `~/global_context/00_Vault/01_MCUs/` を参照
2. **ピン配置・回路図対照表**: `~/global_context/00_Vault/05_Schematics/` を参照
3. **RTOSソースコード**:
   - 利用ガイド: `~/global_context/00_Vault/02_RTOS/`
   - ソースコード実体: `~/global_context/01_Repositories/RTOS/`
4. **BSPソースコード**:
   - 利用ガイド: `~/global_context/00_Vault/03_BSPs/`
   - ソースコード実体: `~/global_context/01_Repositories/BSPs/`

## AIエージェント行動ルール
- ソースコードツリー全体や大容量PDFファイルを一括でコンテキストに読み込まないこと。
- ピン配置、レジスタオフセット、モジュール仕様を確認する際は、まず `~/global_context/00_Vault/` 配下を `grep` や `file_search` でピンポイント検索すること。
- `~/global_context/01_Repositories/` 配下へのアクセスは、特定のソースコードファイルの直接解析が明示的に求められた場合のみとすること。
```

---

## 6. まとめ・運用のメリット
1. **トークン消費の極小化：** 数千ページのPDFを流し込まず、数行〜数百行の構造化Markdownと必要最低限のソースファイルだけを読むため、トークンを大幅に節約できます。
2. **検索精度の向上：** レジスタのビットマップやピンアサインが綺麗なテーブルになっているため、AIの誤認やハルシネーション（誤った設定値の生成）を防げます。
3. **無駄のないソースコード管理：** RTOSやBSP、過去コードは実体のまま保管し、パスを示すインデックスのみ管理するため、二重管理の手間がありません。
