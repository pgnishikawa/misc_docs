---
name: vault-convert
description: MCUデータシート（レジスタ仕様等の文章系PDF）やポート表（Excel/CSV）を、組み込み開発向け共有コンテキストストア（~/global_context/00_Vault/）の構造化Markdownに変換し、原本との突き合わせ検証を行った上でregistry.mdに索引登録する。Vaultへの変換・追加・更新を依頼された時、または`_index/registry.md`と原本PDFのどちらにも該当情報が見つからず新規変換が必要になった時に使う。回路図PDF（配線図としての読解が必要なもの）は`schematic-netmap`スキルに委譲すること。
---

# vault-convert: PDF/Excel → Vault Markdown 変換スキル

このスキルは、`~/global_context/` 共有コンテキストストアにおける「原本ドキュメント（PDF/Excel）→ 構造化Markdown」の変換・検証・索引登録を一貫して行うための手順書です。CLAUDE.mdからはこのスキルの呼び出しだけを指示し、詳細手順はこのファイルを唯一の正とします。

## 前提: 入出力の場所

| 種別 | 原本の場所 | 変換後Markdownの場所 |
|---|---|---|
| MCUデータシート | `01_Repositories/Schematics_PDFs/MCU_Datasheets/<型番>/` | `00_Vault/01_MCUs/<型番>/*.md` |
| 評価ボードマニュアル（文章・機能説明が中心のもの） | `01_Repositories/Schematics_PDFs/Eval_Boards/<ボード名>/` | `00_Vault/07_Eval_Boards/<ボード名>/*.md` |
| ポート表（Excel/CSV） | `01_Repositories/Port_Tables/<製品名>/` | `00_Vault/08_Product_Boards/<製品名>/pin_assignment.md` |

**回路図PDF（評価ボード・製品ボードとも、配線図としての読解が必要なもの）は対象外**。`schematic-netmap`スキルを使うこと。

対象ファイルが上記に見当たらない場合は、ユーザーに原本の配置を確認すること（勝手に他の場所を探索し続けない）。

## 手順

### ステップ1: ドキュメント種別を判定し、テンプレートを選ぶ
- レジスタ仕様・エラッタ・シーケンス手順などの文章系 → 「テンプレートA: レジスタ/仕様書」を使う
- ピン配置・信号対応表などの表系（ポート表Excel由来） → 「テンプレートB: ピン配置表」を使う
- **配線図（シンボル・配線）としての回路図PDF** → このスキルでは扱わない。`schematic-netmap`スキルに切り替える

### ステップ2: 原本を読み込む
- **PDF**: Readツールでページ範囲を指定して直接読み込む（該当章のみ。ドキュメント全体を一括で読まない）
- **Excel（.xlsx）**: Readツールはxlsxを構造化して読める保証がないため、先にテキスト化する
  - 手軽な方法: Excel側で対象シートを「CSVとして保存」してからCSVを読む
  - 自動化する場合: Bashで `python -c "import pandas as pd; pd.read_excel('<path>').to_csv('<path>.csv')"` を実行してからCSVを読む

### ステップ3: 変換ルールに従ってMarkdown化する
- ヘッダー・フッター・ドキュメントID・ページ番号などのノイズを除去する
- レジスタのビット割り当ては必ずテーブル形式（Bit, Field, Reset, Description）にする
- 初期化シーケンス・手順は番号付きリストにする
- Errata・特記事項は引用ブロック（`>`）で強調する
- ピン配置・ポート表は信号名を軸にした統一フォーマット（テンプレートB）に揃える

### ステップ4: 一旦 `verified: false` で保存する
下記テンプレートのfrontmatterを付けて、対応する`00_Vault/`配下のパスに保存する。この時点ではまだ`_index/registry.md`には登録しない。

### ステップ5: セルフレビュー（検証パス）を行う
1. ステップ4で保存したMarkdownをいったん脇に置き、元PDF（またはCSV/Excel）の該当ページ・該当シートをもう一度読み直す
2. 生成したMarkdownの表と原本を1項目ずつ突き合わせる。特にビット位置・オフセット・リセット値・ピン番号・信号名など数値情報を優先的に照合する
3. 不一致があれば、その場でMarkdownを修正する
4. 一致を確認したら、frontmatterを `verified: true` および `verified_date: <本日の日付>` に更新する
5. 明らかな不一致が解消できない場合は `verified: false` のまま残し、ユーザーに確認を求める（憶測で埋めない）

### ステップ6: `_index/registry.md` に索引登録する
- `verified: true` になったものだけを登録する
- 型番・ボード名・BSP名が既にregistry.mdにあれば何もしない。無ければ1行追記する（フォーマットは下記）
- `Aliases`列には略称・通称も入れる（例: "STM32F429ZIT6" に対し "STM32F429", "F429"）

### ステップ7: 結果を報告する
- 保存したMarkdownのパス、`verified`の状態、registry.mdへの追記有無を簡潔にユーザーへ報告する

## テンプレートA: レジスタ/仕様書

```markdown
---
title: USART Status Register (USART_SR)
mcu: STM32F4
source: RM0090 Rev19, p.450
converted: 2026-08-23
verified: true
verified_date: 2026-08-23
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

## テンプレートB: ピン配置表

```markdown
---
board: Model_Y
board_type: product          # eval / product
source: ポート表 v1.2
converted: 2026-08-23
verified: true
verified_date: 2026-08-23
keywords: [ピン配置, pinout, GPIO, ポート表]
---

| Signal | MCU Pin | Connector/Net | Function | 備考 |
|---|---|---|---|---|
| USART2_TX | PA2 | CN1-14 | デバッグUART送信 | 評価ボードと同一 |
| USART2_RX | PA3 | CN1-15 | デバッグUART受信 | 評価ボードと同一 |
| LED_STATUS | PB6 | LED1 | ステータスLED | 評価ボードはPA5、製品ボードはPB6に変更 |
```

## registry.md の行フォーマット

```markdown
| Name | Aliases | Type | Path |
|---|---|---|---|
| STM32F429ZIT6 | STM32F429, F429, STM32F4 | MCU | `00_Vault/01_MCUs/STM32F4/` |
```

`Type`は `MCU` / `EvalBoard` / `ProductBoard` / `BSP` / `RTOS` のいずれかを使う。

## ガードレール

- 対象章・対象シート以外の部分は読み込まない（ドキュメント全体の一括読み込み禁止）
- 既に`verified: true`のMarkdownが存在する場合、明示的に再変換・更新を求められない限り上書きしない
- 数値項目（ビット位置・オフセット・ピン番号等）で原本と食い違いが解消できない場合、`verified: false`のまま保存し、埋め合わせで値を創作しない
- 変換・検証・登録が全て終わるまで、途中経過だけをVaultに残したまま作業を終えない（`verified: false`のまま放置しない。次回続きから再開できるよう、必ずステップ5まで完了させるかユーザーに保留中であることを明示する）
