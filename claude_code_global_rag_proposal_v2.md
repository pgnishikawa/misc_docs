# Claude Code 向けグローバル知識ベース構成案（完全版 / 組み込み開発最適化）

元案（`claude_code_global_rag_proposal.md`）の設計思想を引き継ぎつつ、レビューで指摘した「CLAUDE.md自動読み込み範囲」「権限摩擦」「PDF変換の手間」「鮮度管理」「grepの限界」を解消した実装版です。

---

## 0. 目的（再確認）
- MCUデータシート・RTOS/BSP仕様・過去機種資産を**複数プロジェクトで使い回す**
- 毎セッションPDF全量やソースツリー全体を読ませず、**必要な箇所だけ検索**させてトークンを節約する
- レジスタ値・ピン配置の**誤認識（ハルシネーション）を防ぐ**
- 資料作成（レジスタ設定表、ピン配置表など）を高速化する

---

## 1. 全体方針（3つの原則）

### 原則1: 事前に全部変換しない（Just-in-Time変換）
1,000ページのデータシートを最初に全部Markdown化するのは工数対効果が悪いです。**実際にプロジェクトで必要になったセクションだけ、その場でMarkdown化してVaultに積み上げる**運用にします。2機種目以降は自動的に資産が溜まっていくため、変換コストは徐々にゼロへ収束します。

### 原則2: CLAUDE.mdは「薄い索引」を階層配置し、`@import`で参照する
Claude Codeは**プロジェクトルートのCLAUDE.mdをセッション開始時に自動読み込み**し、**サブディレクトリのCLAUDE.mdはそのディレクトリ配下のファイルに実際にアクセスした時に追加読み込み**します。この性質を利用し、詳細な索引はサブディレクトリ側に置き、ルートは要約のみにします。各プロジェクト側のCLAUDE.mdは中身をコピペせず `@~/global_context/00_Vault/CLAUDE.md` の1行インポートで参照し、実体は一箇所で管理します。

### 原則3: grep前提で運用し、意味検索は後回しにする
これは厳密なベクトルRAGではなく**構造化Markdown＋grep検索**です。組み込みのレジスタ名・信号名は表記ゆれが少ないため実用上問題ありませんが、frontmatterに検索キーワード（エイリアス）を持たせて表記ゆれを吸収します。将来「言い換え検索」が本当に必要になったら、その時点でベクトル検索MCPの追加を検討すれば十分です（8章参照）。

---

## 2. 全体ディレクトリ構成

```text
~/global_context/
├── 00_Vault/                             # [Knowledge Hub] Markdown索引・ナレッジ
│   ├── CLAUDE.md                         # ★薄い索引のみ（下記3章参照）
│   ├── 01_MCUs/
│   │   ├── CLAUDE.md                     # MCUカテゴリ全体の案内
│   │   └── STM32F4/
│   │       ├── CLAUDE.md                 # STM32F4固有の目次・参照ルール
│   │       ├── usart_registers.md
│   │       ├── timer_registers.md
│   │       └── errata.md
│   ├── 02_RTOS/
│   │   └── FreeRTOS/
│   │       ├── CLAUDE.md
│   │       └── task_design.md
│   ├── 03_BSPs/
│   │   └── Vendor_BSP_v2/
│   │       ├── CLAUDE.md
│   │       └── peripheral_usage.md
│   ├── 04_Legacy_Assets/
│   │   └── Model_X/
│   │       └── architecture.md
│   └── 05_Schematics/
│       └── Model_X_Pinout.md
│
└── 01_Repositories/                      # [Storage Real Assets] 実体
    ├── RTOS/
    ├── BSPs/
    ├── Legacy_Projects/
    └── Schematics_PDFs/                  # データシート原本PDF（変換元として保管）
```

元案からの変更点は「各カテゴリ配下にCLAUDE.mdを追加した」ことだけです。ディレクトリの意味自体は変えていません。

---

## 3. CLAUDE.mdの配置と書き方

### 3-1. グローバル索引（`~/global_context/00_Vault/CLAUDE.md`）
**数行の要約のみ**。詳細を書かない。

```markdown
# グローバル組み込みナレッジベース

Markdownナレッジ: `~/global_context/00_Vault/`
実体ソース/PDF: `~/global_context/01_Repositories/`

## 行動ルール
- レジスタ/ピン配置/RTOS/BSP情報が必要な時は、まず該当カテゴリのサブディレクトリ
  （例: `00_Vault/01_MCUs/<型番>/`）まで絞ってgrep検索すること。
  Vault全体やRepositories全体を再帰的に読み込まないこと。
- 各サブディレクトリのCLAUDE.mdに詳細な参照ルールがあるので、そちらに従うこと。
- 該当Markdownが存在しない場合のみ、5章の手順でPDFから該当セクションを
  変換し、このVaultに追加すること（作りっぱなしにせず必ず保存する）。
```

### 3-2. カテゴリ別索引（例: `00_Vault/01_MCUs/STM32F4/CLAUDE.md`）
実際に使うディレクトリに入った時だけ読み込まれるので、ここに詳細を書いてよい。

```markdown
# STM32F4 ナレッジ

- レジスタ仕様: `usart_registers.md`, `timer_registers.md` など、ペリフェラル名 + `_registers.md`
- 既知の不具合: `errata.md`
- 変換元データシート実体: `../../../01_Repositories/Schematics_PDFs/STM32F4/RM0090.pdf`
  （このディレクトリに答えがない場合のみ、該当章を明示ページ範囲で読み、
  Markdown化してこのディレクトリに追加すること）
```

### 3-3. 各プロジェクト側のCLAUDE.md
コピペせず、インポートで参照する。

```markdown
# プロジェクト固有の指示
...(プロジェクト固有の内容)...

@~/global_context/00_Vault/CLAUDE.md
```

これで索引の実体は一箇所（グローバルVault側）だけで管理でき、プロジェクトが増えても更新は1箇所で済みます。

---

## 4. 権限設定（アクセス摩擦の解消）

プロジェクト外の絶対パスへの読み書きは、都度確認プロンプトが出て運用の妨げになります。`~/global_context/` へのRead/Grep/Glob許可をあらかじめグローバル設定に登録しておきます。

- Claude Codeの `update-config` スキル（`/`コマンドから呼び出せる設定用スキル）で、`~/global_context/**` に対する読み取り系ツールの許可をグローバル設定（`~/.claude/settings.json`）に追加する
- 書き込み（Markdown追加・更新）はVault配下のみ許可し、`01_Repositories/`（実体資産）は誤って上書きしないよう書き込み許可を与えない

正確な許可ルールの書式はバージョンによって変わり得るため、手で `settings.json` を編集するより `/permissions` コマンドまたは `update-config` スキル経由で設定するのが確実です。

---

## 5. PDF→Markdown変換フロー（Just-in-Time版）

元案は「PDFをコピーしてWeb版Claudeに貼り付け」という完全手動フローでしたが、**Claude CodeのReadツールはPDFを直接ページ範囲指定で読み込めます**。Web版へのコピペは不要です。

### 手順
1. データシートPDFを `01_Repositories/Schematics_PDFs/<型番>/` に置く（原本として保管）
2. プロジェクト作業中に該当ペリフェラルの情報が必要になったら、Claude Codeに次のように依頼する:
   > 「`01_Repositories/Schematics_PDFs/STM32F4/RM0090.pdf` の p.450-470（USARTレジスタ章）を読み、6章のテンプレートに従ってMarkdown化し、`00_Vault/01_MCUs/STM32F4/usart_registers.md` に保存して」
3. Claude Codeが変換・保存まで一括で行う（Web版との往復が不要になる）
4. 大きな章をまとめて変換したい場合は、章ごとにサブエージェントへ並列で投げることもできる

これにより「1,000ページを最初に全部変換する」という重い前提がなくなり、**実際に使った分だけ資産が育つ**運用になります。2機種目・3機種目の開発では、既存Vaultのヒット率が上がり変換の手間はどんどん減ります。

---

## 6. Markdownファイルのテンプレート（frontmatter付き）

grepの表記ゆれ吸収と、鮮度検証のため、変換時は必ず以下の情報を付与します。

```markdown
---
title: USART Status Register (USART_SR)
mcu: STM32F4
source: RM0090 Rev19, p.450
converted: 2026-08-23
keywords: [USART, UART, シリアル通信, ステータスレジスタ, TXE, TC, RXNE]
---

### USART_SR (Status Register)
- **Offset:** 0x00 | **Reset Value:** 0x00C0

| Bit | Field | Reset | Description |
|---|---|---|---|
| 7 | TXE | 0 | **Transmit data register empty**<br>0: Data not transferred<br>1: Data transferred to shift register |
| 6 | TC | 1 | **Transmission complete** |
| 5 | RXNE | 0 | **Read data register not empty** |
```

- `source` があることで、後で元データシートの版数と食い違っていないか人間が検証できる
- `keywords` があることでgrepの表記ゆれ耐性が上がる（"UART"で検索しても"USART"のファイルがヒットする）

変換プロンプト自体（表構造化・ノイズ除去・番号付きリスト化・Erratumは引用ブロック化）は元案4章のルールをそのまま流用してよく、これはClaude Codeへの依頼文にそのまま含めます。

---

## 7. 日々の運用ワークフロー

### 新規プロジェクト立ち上げ時
1. プロジェクトルートのCLAUDE.mdに `@~/global_context/00_Vault/CLAUDE.md` を1行追加
2. 使用するMCU/RTOS/BSPのVaultサブディレクトリが無ければ空で作成（`CLAUDE.md`の雛形だけ先に置いてもよい）

### 開発中（都度）
1. レジスタ/ピン配置を確認したい → Claude Codeが自動的に `00_Vault/` 配下を該当カテゴリまで絞ってgrep
2. 該当Markdownがあれば、そのままそれを根拠に回答・コード生成
3. なければ、5章のフローでPDFから該当箇所だけ変換してVaultに追加（次回以降は他プロジェクトでも再利用できる）

### 過去機種コード・回路図の参照
- 元案通り、実体はそのまま `01_Repositories/` に保管し、パスをピンポイントで指定して読ませる（ディレクトリ全体は読ませない）

---

## 8. 将来の拡張（意味検索が本当に必要になったら）

grepベースでは、言い換えや曖昧な自然文検索（例:「低消費電力モードでのUART推奨クロックは？」のような複合条件）に弱い場面が出てくる可能性があります。その場合は:

- MCPサーバー経由でベクトルDB（Chroma等）を接続し、`00_Vault/` のMarkdownをチャンク化して埋め込み検索を追加する
- ただし現状のgrep運用で困っていないうちは導入しない（複雑さとメンテコストが増えるため）

---

## 9. まとめ（Before / After）

| 項目 | 元案 | 完全版での変更点 |
|---|---|---|
| Markdown変換 | 事前に全量手動変換（Web版コピペ） | 使った分だけJust-in-Timeで変換、Claude CodeのPDF直接読み込みを使用 |
| CLAUDE.md | ルートに全索引を記述 | ルートは要約のみ、詳細はカテゴリ別サブディレクトリに分散、`@import`で参照 |
| 権限 | 未考慮 | `update-config`スキル/`/permissions`でグローバルパスの読み取りを事前許可 |
| grep精度 | 素のgrep | frontmatterでキーワード・エイリアスを付与 |
| 鮮度管理 | なし | `source`（版数・ページ）と`converted`日付を各Markdownに記録 |
| 意味検索 | 「RAG」と呼称するが実体はgrep | 現状はgrepで十分と割り切り、将来必要ならベクトル検索を追加 |

この完全版に沿えば、初期投資（全量変換）の重さを避けつつ、狙い通りトークン消費を抑えた運用が可能になります。
