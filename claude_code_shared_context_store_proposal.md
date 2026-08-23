# Claude Code 向け共有コンテキストストア構成案（完全版 / 組み込み開発最適化）

異なるディレクトリで立ち上げたClaude Codeの各セッションは、それぞれ独立していて記憶を共有しません。本構成案は、**複数のAIエージェントセッションが横断的に参照できる共有コンテキストストア**（人間向けの「メモリ（RAM）」と紛らわしいため、意図的にこの呼称を採用しています）を組み込み開発向けに構築するものです。元案（`claude_code_global_rag_proposal.md`）の設計思想を引き継ぎつつ、レビューで指摘した「CLAUDE.md自動読み込み範囲」「権限摩擦」「PDF変換の手間」「鮮度管理」「検索精度の限界」を解消し、さらに**新規プロジェクト向けのコード生成**を用途に加えた実装版です。

---

## 0. 目的（再確認）
- MCUデータシート・RTOS/BSP仕様・過去機種資産を**複数プロジェクトで使い回す**
- 毎セッションPDF全量やソースツリー全体を読ませず、**必要な箇所だけ検索**させてトークンを節約する
- レジスタ値・ピン配置の**誤認識（ハルシネーション）を防ぐ**
- 資料作成（レジスタ設定表、ピン配置表、**BSP導入手順書**、**サンプルビルド手順書**、**旧製品ソースコードの解析資料**など）を高速化する
- **新規プロジェクトの立ち上げ時、過去機種のコード資産・BSP・RTOS設定・コーディング規約を踏まえた初期コード（スケルトン・周辺機能初期化コードなど）を生成させる**
- **評価ボード（Evaluation Board）向けのBSP/RTOSサンプルコードをベースに、製品ボードの回路図・ポート表に合わせて配線・ピン配置を移植した、製品ボード向け最小構成のサンプルコードを生成させる**

---

## 0-1. この文書の位置づけ（設計資料 ≠ CLAUDE.md）

**本ドキュメント自体を、そのままCLAUDE.mdとして配置してはいけません。** これは人間が設計を検討・共有するための資料であり、AIエージェントへの行動ルールだけを渡すCLAUDE.mdとは役割が異なります。丸ごとCLAUDE.mdにすると、実際に必要な行動ルールの何倍もある背景説明・比較表・依頼例を毎セッション自動的に読み込むことになり、本構成が目指す**トークン消費の最小化**と正面から矛盾します。

| | 本ドキュメント（設計資料） | 実際に配置するCLAUDE.md |
|---|---|---|
| 目的 | なぜこの構成にしたか、原則・比較・具体例を人間に説明する | AIエージェントへ行動ルールを与える |
| 分量 | 長文（背景説明・Before/After・依頼例を含む） | 数行〜数十行（3章参照） |
| 読み手 | 人間（エンジニア） | Claude Codeエージェント（毎セッション自動読み込み） |
| 配置場所 | `~/global_context/00_Vault/_design/claude_code_shared_context_store_proposal.md`（参照用として保管。自動読み込みはさせない） | `~/global_context/00_Vault/CLAUDE.md`（3-1章の内容そのもの） |
| 更新頻度 | 構成そのものを見直す時だけ | 運用しながら随時 |

設計の経緯を後から思い出したくなった場合は、Claude Codeに「`_design/claude_code_shared_context_store_proposal.md` を読んで」と明示的に指示すれば参照できます。`_design/`配下は自動読み込み対象に含めないため、普段のトークン消費には影響しません。CLAUDE.mdへ書くべき内容は、3-1章・3-2章に示した最小限のテンプレートだけです。

---

## 1. 全体方針（3つの原則）

### 原則1: 事前に全部変換しない（Just-in-Time変換）
1,000ページのデータシートを最初に全部Markdown化するのは工数対効果が悪いです。**実際にプロジェクトで必要になったセクションだけ、その場でMarkdown化してVaultに積み上げる**運用にします。2機種目以降は自動的に資産が溜まっていくため、変換コストは徐々にゼロへ収束します。

### 原則2: CLAUDE.mdは「薄い索引」を階層配置し、`@import`で参照する
Claude Codeは**プロジェクトルートのCLAUDE.mdをセッション開始時に自動読み込み**し、**サブディレクトリのCLAUDE.mdはそのディレクトリ配下のファイルに実際にアクセスした時に追加読み込み**します。この性質を利用し、詳細な索引はサブディレクトリ側に置き、ルートは要約のみにします。各プロジェクト側のCLAUDE.mdは中身をコピペせず `@~/global_context/00_Vault/CLAUDE.md` の1行インポートで参照し、実体は一箇所で管理します。

### 原則3: grep前提で運用し、意味検索は後回しにする
この共有コンテキストストアの検索は、埋め込みベクトルによる意味検索ではなく**構造化Markdown＋grepによるキーワード検索**です。組み込みのレジスタ名・信号名は表記ゆれが少ないため実用上問題ありませんが、frontmatterに検索キーワード（エイリアス）を持たせて表記ゆれを吸収します。将来「言い換え検索」が本当に必要になったら、その時点でベクトル検索MCPの追加を検討すれば十分です（10章参照）。

### 原則4: `00_Vault/`と`01_Repositories/`の関係は資産の種類で異なる（代替 vs 併存）
`00_Vault/`は「使うほど育ち、次第にそれだけで用が足りるようになる」場所ですが、この効果は`01_Repositories/`の中身によって効き方が違います。

- **ドキュメント系（データシートPDF・評価ボードマニュアル・製品ボード回路図・ポート表Excel、`Schematics_PDFs/`・`Port_Tables/`）**: Vaultへの変換が進むほど、原本PDFへ直接アクセスする頻度は下がっていきます。Markdownが実質的に**原本の代替**になるためです。
- **実装資産系（RTOS/BSP/過去機種ソース/プロジェクト雛形、`RTOS/`・`BSPs/`・`Legacy_Projects/`・`Templates/`）**: Vaultが育っても参照頻度は下がりません。コード生成（9章）では変数名・関数シグネチャまで含めて過去資産と一致させる必要があり、Markdown要約では精度が足りないためです。`00_Vault/03_BSPs/`や`04_Legacy_Assets/`にあるのはあくまで手順書・解析結果であり、実際のコード生成時には実体ソースを毎回名指しで直接読みに行きます。この場合Vaultは原本の**代替ではなく補助的な索引**という位置づけです。

つまり「Vaultが育つほどRepositoriesを見なくなる」のはドキュメント系だけで、実装資産系のRepositoriesはVaultが成熟しても恒常的に参照され続ける一次情報源です。

### 原則5: `00_Vault/`内も「知識本体」と「制御レイヤー」で読み手が違う
`00_Vault/`は全体が「AI専用で人間には読みにくいもの」というわけではありません。中身によって想定読者が分かれます。

- **知識本体（レジスタ表・ピン配置表・BSP導入手順書・旧製品解析資料など）**: PDFのノイズ除去・表構造化を経ているため、**人間にとってもむしろ読みやすい**設計です。frontmatter付きMarkdownという形式もObsidianのノートと同じ形式なので、AIエージェント用の索引としてだけでなく、人間がそのままObsidian Vaultとして閲覧・編集する使い方とも相性が良いです。
- **制御レイヤー（各`CLAUDE.md`、`_index/registry.md`）**: こちらはAIエージェント向けの行動指示・機械的な索引データであり、人間が読むと指示書然としていて説明文書としては読みにくいものです。

「Vault=AI専用で人間に不向き」なのはこの制御レイヤーだけで、知識本体は人間にも有用な資料になる、という整理です。

---

## 2. 全体ディレクトリ構成

```text
~/global_context/
├── 00_Vault/                             # [Knowledge Hub] Markdown索引・ナレッジ
│   ├── CLAUDE.md                         # ★薄い索引のみ（下記3章参照）
│   ├── _design/                          # ★新規: 設計資料置き場（自動読み込み対象外）
│   │   └── claude_code_shared_context_store_proposal.md  # 本ドキュメント自体（0-1章参照）
│   ├── _index/                           # ★新規: 固有名詞→パス解決インデックス
│   │   └── registry.md                   # SoC名/ボード名/BSP名 → Vaultパスの対応表（3章参照）
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
│   │       ├── peripheral_usage.md
│   │       ├── setup_guide.md            # ★新規: BSP導入手順書
│   │       └── build_guide.md            # ★新規: サンプルビルド手順書
│   ├── 04_Legacy_Assets/
│   │   └── Model_X/
│   │       ├── architecture.md
│   │       ├── module_analysis.md        # ★新規: 旧製品ソースコードの解析資料
│   │       └── known_issues.md           # ★新規: 解析で判明した既知の問題点
│   ├── 05_Schematics/
│   │   └── Model_X_Pinout.md
│   ├── 06_Coding_Standards/              # ★新規: コーディング規約・命名規則
│   │   ├── CLAUDE.md
│   │   ├── naming_convention.md
│   │   └── project_structure.md
│   ├── 07_Eval_Boards/                   # ★新規: 評価ボードのピン配置・周辺回路
│   │   └── NUCLEO-F429ZI/
│   │       ├── CLAUDE.md
│   │       ├── pin_assignment.md         # 評価ボードのMCUピン⇔コネクタ/機能 対応表
│   │       └── peripheral_connections.md # LED/ボタン/センサ等オンボード周辺の接続
│   └── 08_Product_Boards/                # ★新規: 製品ボードのピン配置・差分
│       └── Model_Y/
│           ├── CLAUDE.md
│           ├── pin_assignment.md         # 製品ボードのピン配置（回路図+ポート表を統合）
│           └── pin_diff_vs_NUCLEO-F429ZI.md  # 評価ボードとの差分（コード移植の根拠）
│
└── 01_Repositories/                      # [Storage Real Assets] 実体（原則4参照: ドキュメント系はVaultへ代替、実装資産系はVaultと併存）
    ├── RTOS/
    ├── BSPs/
    ├── Legacy_Projects/
    ├── Schematics_PDFs/                  # PDF原本（変換元として保管）
    │   ├── MCU_Datasheets/<型番>/
    │   ├── Eval_Boards/<評価ボード名>/    # ★新規: 評価ボードのユーザーマニュアルPDF
    │   └── Product_Boards/<製品名>/       # ★新規: 製品ボードの回路図PDF
    ├── Port_Tables/                       # ★新規: ポート表原本（Excel/CSV）
    │   └── Model_Y/port_table.xlsx
    └── Templates/                         # 新規プロジェクト用スケルトン（ビルド可能な最小構成）
        └── STM32F4_Base/
```

元案からの変更点は「各カテゴリ配下にCLAUDE.mdを追加した」ことに加え、コード生成のために **`06_Coding_Standards/`（規約）**・**`07_Eval_Boards/`（評価ボードのピン配置）**・**`08_Product_Boards/`（製品ボードのピン配置・差分）**・**`01_Repositories/Templates/`（プロジェクト雛形）**・**`01_Repositories/Port_Tables/`（ポート表原本）** を新設し、さらに資料生成のために **`03_BSPs/*/setup_guide.md`・`build_guide.md`（BSP導入・ビルド手順書）** と **`04_Legacy_Assets/*/module_analysis.md`（旧製品解析資料）**、本ドキュメント自体の置き場所として **`_design/`（自動読み込み対象外）**、そして固有名詞をトークン消費少なく解決するための **`_index/registry.md`** を追加したことです。既存ディレクトリの意味は変えていません。

---

## 3. CLAUDE.mdの配置と書き方

### 3-1. グローバル索引（`~/global_context/00_Vault/CLAUDE.md`）
**数行の要約のみ**。詳細を書かない。

```markdown
# 組み込み開発向け共有コンテキストストア

Markdownナレッジ: `~/global_context/00_Vault/`
実体ソース/PDF: `~/global_context/01_Repositories/`

## カテゴリ一覧（00_Vault/）
- `01_MCUs/`: MCUレジスタ・エラッタ
- `02_RTOS/`: RTOS設計ガイド
- `03_BSPs/`: BSP利用ガイド・導入/ビルド手順書
- `04_Legacy_Assets/`: 過去機種の設計思想・ソースコード解析資料
- `05_Schematics/`: 回路図由来のピン配置表
- `06_Coding_Standards/`: コーディング規約・命名規則
- `07_Eval_Boards/`: 評価ボードのピン配置・オンボード周辺回路
- `08_Product_Boards/`: 製品ボードのピン配置・評価ボードとの差分

## 固有名詞の解決ルール（SoC名・ボード名・BSP名などを指示された時）
ユーザーの指示に「STM32F429のマニュアルを調べて」のような固有名詞が含まれる場合、
フルパスを聞き返さず、以下の順で自分で解決すること（この順に安い）。
1. `00_Vault/_index/registry.md` を対象語で `grep -i` する（1回のgrepで完結）
2. ヒットしたPathのMarkdownを読み、内容があればそれで回答する
3. registry.mdにヒットしない場合、`01_Repositories/Schematics_PDFs/` 配下を対象語で
   `glob`/`find` し、原本PDFの有無を確認する（ファイル名検索のみ、中身は読まない）
4. 原本PDFが見つかったら5章の手順（検証パス含む）でMarkdown化・検証してVaultに保存し、
   **`registry.md` にも1行追記する**（検証未了のものは追記しない。次回以降は手順1で即座にヒットする）
5. registry.mdにも原本にも見つからない場合のみ、ユーザーに確認する

## 行動ルール
- レジスタ/ピン配置/RTOS/BSP情報が必要な時は、まず該当カテゴリのサブディレクトリ
  まで絞ってgrep検索すること。Vault全体やRepositories全体を再帰的に読み込まないこと。
- 各サブディレクトリのCLAUDE.mdに詳細な参照ルールがあるので、そちらに従うこと。
- 新しい固有名詞（SoC名・ボード名・BSP名など）のMarkdownをVaultに追加した時は、
  検証済みであれば `00_Vault/_index/registry.md` にも1行追記すること
  （5章・7章・9章のどの生成フローでも共通のルール）。追記を忘れても致命的ではないが、
  次回以降の検索コストが上がる。

## 資料生成ルール（BSP導入手順書・ビルド手順書・旧製品解析資料）
- BSP導入手順書・ビルド手順書は `00_Vault/03_BSPs/<BSP名>/` に生成・保存すること（7章参照）。
- 旧製品ソースコードの解析は、対象モジュールを1つずつ指定して行い、
  `01_Repositories/Legacy_Projects/` 配下を一括で読み込まないこと。
  結果は `00_Vault/04_Legacy_Assets/<機種名>/module_analysis.md` に追記保存すること。

## コード生成ルール（新規プロジェクト向け）
- 新規プロジェクトのコードを生成する際は、まず `00_Vault/06_Coding_Standards/` の
  規約（命名規則・ディレクトリ構成）に従うこと。
- プロジェクトの雛形が必要な場合は `01_Repositories/Templates/` からコピーして開始すること。
- 過去実装を参考にする場合は `01_Repositories/Legacy_Projects/` や `01_Repositories/BSPs/`
  内の対象ファイルを名指しで読み込み、ディレクトリ全体をスキャンしないこと。
- レジスタ設定値・ピン番号は必ず `00_Vault/01_MCUs/` 配下のMarkdownを根拠にすること。
- **評価ボードのBSP/RTOSサンプルを製品ボード向けに移植する場合**は、
  `00_Vault/07_Eval_Boards/<評価ボード名>/pin_assignment.md` と
  `00_Vault/08_Product_Boards/<製品名>/pin_assignment.md` を突き合わせ、
  `pin_diff_vs_<評価ボード名>.md` があればそれを差分の根拠とすること。無ければ先に作成すること。
  詳細は9章参照。
```

### 3-1補足. `_index/registry.md`（固有名詞→パス解決インデックス）

SoC名・評価ボード名・製品ボード名・BSP名などの固有名詞を、Vault内のパスへ即座に解決するための**単一の索引ファイル**です。カテゴリ一覧が「大分類」の地図だとすれば、こちらは「型番・製品名」の電話帳にあたります。1エントリ1行なので、登録数が増えても数百エントリ程度まではgrep一発で軽量に検索できます。

```markdown
| Name | Aliases | Type | Path |
|---|---|---|---|
| STM32F429ZIT6 | STM32F429, F429, STM32F4 | MCU | `00_Vault/01_MCUs/STM32F4/` |
| NUCLEO-F429ZI | Nucleo F429, 評価ボードF429 | EvalBoard | `00_Vault/07_Eval_Boards/NUCLEO-F429ZI/` |
| Model_Y | 製品ボードY | ProductBoard | `00_Vault/08_Product_Boards/Model_Y/` |
| Vendor_BSP_v2 | - | BSP | `00_Vault/03_BSPs/Vendor_BSP_v2/` |
| FreeRTOS | - | RTOS | `00_Vault/02_RTOS/FreeRTOS/` |
```

- `Aliases`列には型番の略称・通称を入れておく（例: "STM32F429" だけでなく "F429" も）。ユーザーが略称で聞いても `grep -i` でヒットする
- 新しいMCU/ボード/BSPをVaultに追加するたびに、必ず1行追記する（5章・7章・9章のいずれの生成フローでも共通のルール）
- 追記漏れがあっても致命的ではない（その場合は§固有名詞の解決ルールの手順3=PDF原本のfind探索にフォールバックする）が、追記しておくほど2回目以降の検索が速く・安くなる

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

## 5. PDF/Excel → Markdown変換フロー（Just-in-Time版）

元案は「PDFをコピーしてWeb版Claudeに貼り付け」という完全手動フローでしたが、**Claude CodeのReadツールはPDFを直接ページ範囲指定で読み込めます**。Web版へのコピペは不要です。

### 5-1. PDF（データシート・評価ボードマニュアル・製品ボード回路図）
1. PDFを該当する `01_Repositories/Schematics_PDFs/` 配下（`MCU_Datasheets/` / `Eval_Boards/` / `Product_Boards/`）に置く（原本として保管）
2. 該当情報が必要になったら、Claude Codeに次のように依頼する:
   > 「`01_Repositories/Schematics_PDFs/STM32F4/RM0090.pdf` の p.450-470（USARTレジスタ章）を読み、6章のテンプレートに従ってMarkdown化し、`00_Vault/01_MCUs/STM32F4/usart_registers.md` に保存して」
3. Claude Codeが変換・保存まで一括で行う（Web版との往復が不要になる）
4. **セルフレビュー（検証パス）を行う**: 変換を担当したセッション内で、保存したMarkdownと元PDFの該当ページをもう一度読み比べさせ、特にレジスタのビット位置・オフセット・リセット値・ピン番号など数値情報を優先的に照合させる。不一致があれば修正してから確定する（詳細は5-3参照）
5. 大きな章をまとめて変換したい場合は、章ごとにサブエージェントへ並列で投げることもできる
6. **その型番/ボードがまだ `00_Vault/_index/registry.md` に無ければ、1行追記する**（3-1補足参照）。検証済みのものだけを登録することで、registry.md経由でヒットする情報は常に検証済みという状態を保てる

### 5-2. ポート表（Excel）
ポート表は多くの場合Excel（`.xlsx`）で管理されており、Claude CodeのReadツールはPDF/画像/テキストには対応していますが、xlsxを直接構造化して読める保証がありません。以下いずれかの方法で一度テキスト化してから読み込ませます。

- **手軽な方法**: Excel側で対象シートを「CSVとして保存」し、CSV（プレーンテキスト）をClaude Codeに読ませる
- **自動化する場合**: Bashツール経由で `python -c "import pandas as pd; pd.read_excel(...).to_csv(...)"` のような変換コマンドを一度実行し、CSV化してから読ませる（都度Excelを手動操作しなくて済む）

変換後は6章のテンプレートに従い、`00_Vault/08_Product_Boards/<製品名>/pin_assignment.md` のような構造化Markdownに落とし込みます。Excel/CSV由来の場合も5-3の検証パスは同様に行います（元CSVと生成Markdownの表を突き合わせる）。

### 5-3. 変換の検証パス（人間レビューの代わりにClaude自身で二重チェックする）
PDF→Markdown変換はLLMが行うため、レジスタのビット割り当てやピン番号を取り違えるリスクがあります。一度Vaultに保存されると以降のセッション全てがそれを無条件に信じてしまうため、**人間が目視確認する代わりに、Claude Code自身に独立した検証パスを踏ませます**。

1. 変換直後、生成したMarkdownをいったん脇に置き、元PDF（または元CSV/Excel）の該当ページ・該当シートをあらためて読ませる
2. 生成したMarkdownの表と、読み直した原本を1項目ずつ突き合わせさせる（特に数値: ビット位置・オフセット・リセット値・ピン番号・信号名）
3. 不一致があれば、その場でMarkdownを修正してから保存を確定する
4. 検証が終わったらfrontmatterに `verified: true` と検証日を記録する（6章のテンプレート参照）

同一セッション内の「生成→検証」の2パスは、変換対象のページ数に対してもう一度分のトークンを使いますが、**変換自体が一度きり（Just-in-Time）である以上、その場で正確性を担保しておく方が、誤ったレジスタ値が将来のセッションで繰り返し使われ続けるリスクよりずっと安いコスト**です。

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
verified: true            # 5-3章のセルフレビュー（原本との突き合わせ）を通過したか
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

- `source` があることで、後で元データシートの版数と食い違っていないか検証できる
- `keywords` があることでgrepの表記ゆれ耐性が上がる（"UART"で検索しても"USART"のファイルがヒットする）
- `verified` があることで、5-3章のセルフレビューを経た情報かどうかをフィルタできる（`verified: false`のまま残っているファイルは要注意、という運用ができる）

変換プロンプト自体（表構造化・ノイズ除去・番号付きリスト化・Erratumは引用ブロック化）は元案4章のルールをそのまま流用してよく、これはClaude Codeへの依頼文にそのまま含めます。

### 6-1. ピン配置表のテンプレート（評価ボード／製品ボード共通）

回路図PDFやポート表Excelから変換する際は、信号名を軸に統一フォーマットへ揃えます。こうしておくと評価ボードと製品ボードのMarkdownを機械的に突き合わせやすくなります。

```markdown
---
board: Model_Y
board_type: product          # eval / product
source: 製品ボード回路図 Rev.C, ポート表 v1.2
converted: 2026-08-23
verified: true                # 5-3章のセルフレビューを通過したか
keywords: [ピン配置, pinout, GPIO, ポート表]
---

| Signal | MCU Pin | Connector/Net | Function | 備考 |
|---|---|---|---|---|
| USART2_TX | PA2 | CN1-14 | デバッグUART送信 | 評価ボードと同一 |
| USART2_RX | PA3 | CN1-15 | デバッグUART受信 | 評価ボードと同一 |
| LED_STATUS | PB6 | LED1 | ステータスLED | 評価ボードはPA5、製品ボードはPB6に変更 |
```

評価ボード側も同じ表形式（`board_type: eval`）でMarkdown化しておくことで、後述の差分抽出がやりやすくなります。

---

## 7. 資料生成ワークフロー（BSP導入手順書・ビルド手順書・旧製品解析資料）

資料作成はレジスタ表・ピン配置表だけでなく、**BSP導入手順書**・**サンプルビルド手順書**・**旧製品ソースコードの解析資料**も対象にします。いずれも実体資産（PDF/README/ビルド設定/過去ソース）から一度生成すれば使い回せるため、生成後はVaultに保存し、以降は差分だけ更新する運用にします。

### 7-1. BSP導入手順書
- 生成対象: `00_Vault/03_BSPs/<BSP名>/setup_guide.md`
- 元ネタ: `01_Repositories/BSPs/<BSP名>/` 内のREADME・付属インストールガイドPDF
- 依頼例:
  > 「`01_Repositories/BSPs/Vendor_BSP_v2/README.md` と付属のインストール手順PDFを読み、開発環境構築からBSP組み込みまでの手順を `00_Vault/03_BSPs/Vendor_BSP_v2/setup_guide.md` にMarkdownでまとめて」

### 7-2. サンプルビルド手順書
- 生成対象: `00_Vault/03_BSPs/<BSP名>/build_guide.md`（ボード固有のフラッシュ/デバッグ手順がある場合は `00_Vault/07_Eval_Boards/<評価ボード名>/build_guide.md` にも追加）
- 元ネタ: `01_Repositories/Templates/` や `01_Repositories/BSPs/` 内のビルド設定（Makefile/CMakeLists.txt/IDEプロジェクトファイル）
- 依頼例:
  > 「`01_Repositories/Templates/STM32F4_Base/CMakeLists.txt` を読み、ビルド手順と書き込み（フラッシュ）手順を `00_Vault/03_BSPs/Vendor_BSP_v2/build_guide.md` にまとめて」

### 7-3. 旧製品ソースコードの解析資料
過去機種のソースコード全体を一度に読み込ませると、実体コードを直接参照する方針でも1回のセッションでトークンを大量消費します。**モジュール単位で段階的に解析し、その都度Markdownに追記保存する**Just-in-Time方式にします。

- 生成対象: `00_Vault/04_Legacy_Assets/<機種名>/module_analysis.md`, `known_issues.md`
- 元ネタ: `01_Repositories/Legacy_Projects/<機種名>/`
- 手順:
  1. 対象モジュール・ディレクトリを1つだけ指定して解析させる（例: 通信モジュールのみ）
  2. 解析結果を `module_analysis.md` に追記形式で保存する
  3. 規模が大きい場合は、モジュールごとにサブエージェントへ並列で投げ、結果だけをメインセッションに集約する（5章のPDF変換と同様の考え方）
- 依頼例:
  > 「`01_Repositories/Legacy_Projects/Model_X/src/comm/` 配下の通信モジュールを解析し、責務・主要関数・他モジュールとの依存関係を `00_Vault/04_Legacy_Assets/Model_X/module_analysis.md` に追記して」

こうして作られた解析資料は、9章（コード生成フロー）で過去実装を参考にする際の一次情報としても再利用でき、実体コードを毎回読み直す必要が減ります。

---

## 8. 日々の運用ワークフロー

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

## 9. 新規プロジェクトのコード生成フロー

新規機種の開発着手時、過去機種のコード資産・BSP・RTOS設定・MCUレジスタ情報を組み合わせて、初期スケルトンや周辺機能の初期化コードをClaude Codeに生成させる際のフローです。ドキュメント作成と異なり、**コード生成では変数名・関数シグネチャ・ディレクトリ構成まで含めて過去資産と一貫性を保つ必要がある**ため、Markdown要約だけでなく実体コードを直接参照させます。

中心的なユースケースは、**評価ボード（Nucleo等）向けに動作確認済みのBSP/RTOSサンプルを、製品ボードのピン配置に合わせて移植し、最小構成のサンプルコードを作る**ことです。

### 9-1. 追加で用意するもの
- `00_Vault/06_Coding_Standards/`: 命名規則、ディレクトリ構成、ヘッダーコメント規約などをMarkdown化したコーディング規約
- `00_Vault/07_Eval_Boards/<評価ボード名>/`: 評価ボードのピン配置・オンボード周辺回路（マニュアルPDFから変換）
- `00_Vault/08_Product_Boards/<製品名>/`: 製品ボードのピン配置（回路図PDF＋ポート表Excelから統合変換）
- `01_Repositories/Templates/`: 実際にビルド可能な最小プロジェクトスケルトン（Git管理）。新規プロジェクトはここからコピーして始める
- `01_Repositories/BSPs/`, `01_Repositories/RTOS/`: 評価ボード向けに動作実績のあるBSP/RTOSサンプルの実体（ベンダー提供コード等）

コーディング規約・雛形・評価ボードのピン配置は一度作れば使い回せる資産なので、5章のPDF変換と違ってJust-in-Timeではなく、**最初にまとめて整備しておく**方が効率的です。一方、製品ボード側のピン配置・差分は製品ごとに毎回新しく発生するので、都度Just-in-Timeで作成します。

### 9-2. 評価ボード→製品ボード移植の具体フロー
1. `00_Vault/07_Eval_Boards/<評価ボード名>/pin_assignment.md` を確認する（無ければ評価ボードマニュアルPDFから6-1テンプレートで変換）
2. `00_Vault/08_Product_Boards/<製品名>/pin_assignment.md` を確認する（無ければ製品ボード回路図PDF＋ポート表Excelから変換・統合。5-2章の手順でExcelをCSV化してから読む）
3. 両者を突き合わせて `00_Vault/08_Product_Boards/<製品名>/pin_diff_vs_<評価ボード名>.md` を作成する（信号ごとに「評価ボードのピン → 製品ボードのピン」の対応表。既にあれば再利用する）
4. 評価ボード向けに動作実績のあるBSP/RTOSコード（`01_Repositories/BSPs/` または `Legacy_Projects/`）を**ファイル名を明示して**参照し、差分表に従ってピン定義・初期化パラメータのみを製品ボード向けに書き換える
5. `01_Repositories/Templates/` の最小構成雛形をベースに、必要なペリフェラル初期化コードだけを組み込んだ製品ボード向けサンプルを生成する
6. 生成したコードは規約 `00_Vault/06_Coding_Standards/` に沿っているか確認する

この手順により、評価ボードのBSP全体やRTOSソースツリーをまるごと読み込ませることなく、**差分表に載っている変更点だけをピンポイントで書き換える**形でコードを生成させられます。

### 9-3. 具体的な依頼例
> 「`00_Vault/07_Eval_Boards/NUCLEO-F429ZI/pin_assignment.md` と `00_Vault/08_Product_Boards/Model_Y/pin_assignment.md` を突き合わせて差分表を作成し、`01_Repositories/BSPs/NUCLEO-F429ZI_BSP/usart_init.c` のUART初期化コードを製品ボードのピン配置に合わせて書き換えた最小構成のサンプルを `01_Repositories/Templates/STM32F4_Base/` ベースで作成して。命名規則は `00_Vault/06_Coding_Standards/naming_convention.md` に従って」

このように依頼先のファイルを具体的に絞ることで、BSPやRTOSのソースツリー全体をスキャンさせずに済みます。

---

## 10. 将来の拡張

### 10-1. 意味検索が本当に必要になったら
grepベースでは、言い換えや曖昧な自然文検索（例:「低消費電力モードでのUART推奨クロックは？」のような複合条件）に弱い場面が出てくる可能性があります。その場合は:

- MCPサーバー経由でベクトルDB（Chroma等）を接続し、`00_Vault/` のMarkdownをチャンク化して埋め込み検索を追加する
- ただし現状のgrep運用で困っていないうちは導入しない（複雑さとメンテコストが増えるため）

### 10-2. 複数人・複数マシンで共有するようになったら（Git管理）
現状は一人での運用を前提としているため`~/global_context/`自体のバージョン管理は導入していませんが、将来的に複数人・複数マシンでこの共有コンテキストストアを使うようになった場合は、Gitリポジトリ化を前提とします。

- `~/global_context/`全体（`00_Vault/`と`01_Repositories/`）をGit管理下に置き、変更履歴・ロールバック・コンフリクト解消の手段を持つ
- 特に`01_Repositories/`はPDF/バイナリを含むため、必要に応じてGit LFS等の大容量ファイル対応を検討する
- 複数人が同時にMarkdownを追記する場合、`_index/registry.md`のようなテキストファイルはコンフリクトが起きやすいため、通常のGitのマージ運用に乗せれば十分（専用の排他制御は不要）
- 導入タイミングは「一人運用からチーム運用に切り替わるとき」でよく、現時点で先回りして整備する必要はない

---

## 11. まとめ（Before / After）

| 項目 | 元案 | 完全版での変更点 |
|---|---|---|
| 用途 | 資料作成のみ（レジスタ表・ピン配置表） | 資料作成（+ BSP導入手順書・ビルド手順書・旧製品解析資料） + 新規プロジェクトのコード生成 |
| Markdown変換 | 事前に全量手動変換（Web版コピペ） | 使った分だけJust-in-Timeで変換、Claude CodeのPDF直接読み込みを使用 |
| Vault成長とRepositoriesの関係 | 未整理（一律「使うほど育つ」というイメージ） | 原則4で明確化: ドキュメント系はVaultが原本の代替になり参照頻度が下がる、実装資産系はVaultが育っても実体コードを恒常的に参照し続ける |
| Vaultの読み手 | 未整理（Vault=AI専用というイメージ） | 原則5で明確化: 知識本体は人間にも読みやすい（Obsidian互換）、CLAUDE.md/registry.mdなど制御レイヤーのみAI専用 |
| CLAUDE.md | ルートに全索引を記述 | ルートは要約のみ、詳細はカテゴリ別サブディレクトリに分散、`@import`で参照 |
| 固有名詞→パス解決 | 未考慮（毎回フルパスを指示する前提） | `_index/registry.md`を新設。「<SoC名>を調べて」だけでgrep一発→未登録ならfind→変換後に自動追記、で解決 |
| 設計資料とCLAUDE.mdの区別 | 未考慮（提案書自体をCLAUDE.md化する誤用の恐れ） | 本ドキュメントは`_design/`に保管し自動読み込みさせない、CLAUDE.mdは3-1章の最小限テンプレートのみ（0-1章参照） |
| 権限 | 未考慮 | `update-config`スキル/`/permissions`でグローバルパスの読み取りを事前許可 |
| grep精度 | 素のgrep | frontmatterでキーワード・エイリアスを付与 |
| 鮮度管理 | なし | `source`（版数・ページ）と`converted`日付を各Markdownに記録 |
| 変換精度の検証 | なし（変換結果を無条件に信頼） | 5-3章のセルフレビュー工程を追加。Claude自身が原本と再照合し、`verified`フラグを記録してから確定・registry登録 |
| 共有領域のバージョン管理 | 未検討 | 現状は一人運用のため未導入。10-2章に将来のGit管理方針を明記（複数人・複数マシン運用に切り替わる時点で導入） |
| 呼称 | 「RAG」（実体はgrep検索でありRAGは不正確、「メモリ」もRAMと紛らわしく不適切） | 「共有コンテキストストア」に統一。現状はgrepで十分と割り切り、将来必要ならベクトル検索を追加 |
| コード生成 | 想定なし | コーディング規約（`06_Coding_Standards/`）とプロジェクト雛形（`Templates/`）を新設し、過去資産を参照優先順位付きで再利用 |
| 評価ボード→製品ボード移植 | 想定なし | 評価ボード（`07_Eval_Boards/`）と製品ボード（`08_Product_Boards/`）のピン配置Markdownを新設し、差分表を根拠にBSP/RTOSサンプルを最小構成に移植 |
| 入力ドキュメント | データシートPDFのみ | 評価ボードマニュアルPDF・製品ボード回路図PDF・ポート表Excel（CSV化して変換）を追加 |
| BSP導入・ビルド手順書 | 想定なし | `03_BSPs/*/setup_guide.md`・`build_guide.md` を新設し、README/ビルド設定から生成 |
| 旧製品ソースコード解析 | `architecture.md`のみ | `04_Legacy_Assets/*/module_analysis.md` を新設し、モジュール単位でJust-in-Time解析・追記 |

この完全版に沿えば、初期投資（全量変換）の重さを避けつつ、狙い通りトークン消費を抑えた運用が可能になります。厳密な効果測定（セッションごとのコスト計測など）はまだ行っていませんが、**「毎セッションPDF原本から読み込ませる場合と比べれば、トークン消費はかなり抑えられるはず」**という定性的な見立てとしては妥当と言えます。
