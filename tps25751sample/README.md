# TPS25751 パッチバンドル・ロード サンプル (RZ/N2H + FSP)

RZ/N2H の I2C1 と Renesas FSP の I2C マスタドライバ (`r_iic_master`) を使って、
TI の USB-C PD コントローラ **TPS25751** を初期化（パッチバンドルをロードして
`PTCH` モード → `APP` モードへ遷移）するサンプルコードです。

- I2C: RZ/N2H **I2C1** (`r_iic_master`, channel 1) — `r_iic_master.h` の `R_IIC_MASTER_*` API を直接呼び出し
- TPS25751 レジスタアクセス用アドレス: **0x22** (7-bit)
- PBM バーストデータ書き込みアドレス: **0x30** (`DATA1` で PD に通知)
- 完了検出: **レジスタポーリング方式**（`I2Ct_IRQ` GPIO は不使用）
- 実行環境: **ベアメタル**（`R_BSP_SoftwareDelay` でウェイト、I2C 完了はコールバックフラグをポーリング）

## なぜこのロードが必要か（EEPROM 非搭載時）

TPS25751 は起動時に「構成（Application Customization ＋ パッチ）」を読み込んで初めて
`APP` モードに入り、USB PD / Type-C のポリシーエンジンが動作します。構成のロード経路は
2 つで、**どちらか一方が必須**です（データシート §8.4.1）。

| 経路 | 説明 |
|------|------|
| 外部 EEPROM（I2Cc、7bit アドレス 0x50） | コールドブート時にデバイスが自動ロード（フルフラッシュイメージ） |
| ホスト（EC）が I2Ct 経由で push | **EEPROM 非搭載構成。本サンプルの `tps25751_load_patch_bundle()` がこれ** |

> データシート §8.4.1: "The device then attempts to load a configuration from an external
> EEPROM on the I2Cc bus. **If no EEPROM is detected, then the device waits for an external
> host to load a configuration.**"

**ロード前（`PTCH` モード）の挙動:**

- `MODE(0x03)` / `CMD1(0x08)` / `DATA1(0x09)` / `INT_*(0x14/0x16/0x18)` / `BOOT_FLAGS(0x2D)`
  以外のレジスタにアクセスできず、PD ステートマシンは実質停止（TRM Table 5-1、JAJA940A §2）。
- **PDO 設定が無いため、ソース（給電側）としては動作しません。**
  USB-C を挿してもコントラクト交渉を行わず、**VBUS へ給電しません**。
- 例外的に動くのは ADCIN2 ストラップで決まる**デッドバッテリ／シンク経路**のみ
  （基板自身が外部ソースから電力を受けるための経路。給電＝ソース動作とは別物）:
  - `AlwaysEnableSink` … Type-C 接続時に常にシンク経路 ON（PD 交渉なし）
  - `SafeMode` … シンク経路も ON にしない（source-only mode 設定も可）。構成ロードまで PD 無効
  - `NegotiateHighVoltage` … 例外的にデフォルト設定で `APP` に入りシンクとして最大 20V を交渉。EEPROM パッチと併用不可・非推奨

つまり **EEPROM を載せない設計では、電源投入のたびにこの初期化（パッチバンドルのロード）を
必ず実行する必要があります。** ロードして `APP` に入れば、バンドル内の設定どおりに
ソース給電・PD 交渉が有効になります。

## 根拠にした資料

| 資料 | 使用箇所 |
|------|----------|
| `TPS25751/slvucr8b.pdf` (Technical Reference Manual, SLVUCR8B) | §1.3 Unique Address Interface Protocol、§3.2 レジスタ定義、§4.5 `PBMs`/`PBMc`/`PBMe`、§5.2 Loading a Patch Bundle (Figure 5-1) |
| `TPS25751/jaja940a.pdf` (Application Note JAJA940A / EN: SLVAFV8) | §4.1 PTCH→APP の 17 ステップ、§5 サンプルコード |

## ファイル構成

| ファイル | 内容 |
|----------|------|
| `tps25751.h` | レジスタ番号 / 4CC / 結果コード / API 宣言 |
| `tps25751.c` | パッチバンドル・ロードのシーケンス実装（本体） |
| `tps25751_patch_bundle.h` / `.c` | パッチバンドル配列（**プレースホルダ。実データに要差し替え**） |
| `tps25751_i2c1_cfg.c` | I2C1 構成の参考テンプレート（通常は Smart Configurator を使用） |
| `tps25751_sample_main.c` | 呼び出し例（`hal_entry()` から `tps25751_sample_run()` を呼ぶ） |
| [`FLOW.md`](FLOW.md) | **処理フロー図解**（レイヤ構成・モード遷移・シーケンス・フローチャート・I2C バイト構成） |

## セットアップ手順

### 1. FSP プロジェクトに I2C1 を追加（RZ Smart Configurator）

1. Stacks に **`r_iic_master`** を追加
2. プロパティを設定
   - Channel: **1**
   - Rate: **Fast-mode (400kHz)** … RZ/N2H は Fast-mode Plus 非対応（`BSP_FEATURE_IIC_FAST_MODE_PLUS = 0`）
   - Slave Address: `0x22` / 7-Bit（実行時に `0x22`⇔`0x30` を切り替えるので初期値は任意）
   - Timeout Mode: Short
   - DMAC: 不要
   - Callback: 空欄で可（`tps25751_open()` が `callbackSet()` で登録します）
3. Pins で I2C1 の **SCL / SDA** を基板の配線に合わせて割り当て（外部プルアップ必須）
4. 割り込み（IIC1 RXI/TXI/TEI/EEI）を有効化 → コード生成

生成される制御ブロック／構成の名前を **`g_i2c1_ctrl`** / **`g_i2c1_cfg`**
（`r_iic_master` の既定名。インスタンス名を `g_i2c1` にすれば自動でこの名前）
にしておくと、`tps25751_sample_main.c` をそのまま使えます
（別名にした場合は `&g_i2c1_ctrl` / `&g_i2c1_cfg` の箇所を読み替え）。

本サンプルは `R_IIC_MASTER_Open` / `R_IIC_MASTER_Write` / `R_IIC_MASTER_Read` /
`R_IIC_MASTER_SlaveAddressSet` / `R_IIC_MASTER_CallbackSet` / `R_IIC_MASTER_Close`
を直接呼び出します（汎用 `i2c_master_instance_t` / `p_api` 経由ではありません）。

> Smart Configurator を使わない場合は `tps25751_i2c1_cfg.c` を参照。
> ただし ICU イベントリンク / NVIC ベクタ登録（`vector_data.c` 相当）は別途必要です。

### 2. パッチバンドルを用意

TI **USB-C PD アプリケーションカスタマイズツール**（GUI）で構成を作成し、
Export から **Low Region Binary** を **C ファイル形式**で出力（JAJA940A §4.2）。
出力された配列を `tps25751_patch_bundle.c` の `g_tps25751_patch_bundle[]` に貼り付け、
`tps25751_patch_bundle.h` の `TPS25751_PATCH_BUNDLE_SIZE` を実サイズに更新します。

### 3. ソースを追加してビルド

`codes/` の `.c` をプロジェクトに追加。`hal_entry()` などから呼び出します。

```c
extern void tps25751_sample_run(void);

void hal_entry(void)
{
    /* ... クロック / ピン初期化は FSP が実施 ... */
    tps25751_sample_run();
    while (1) { /* ... */ }
}
```

## 処理シーケンス（`tps25751_load_patch_bundle()`）

> シーケンス図・フローチャート・レイヤ構成などの図解は **[FLOW.md](FLOW.md)** を参照。

`I2Ct` のレジスタアクセスは SLVUCR8B Figure 1-2 / 1-3 のプロトコル：

```
書き込み: S [0x22|W] [RegNum] [ByteCount=N] [D0..DN-1]                       P
読み出し: S [0x22|W] [RegNum]  Sr  [0x22|R] [ByteCount=N] [D0..DN-1(先頭=バイト数)] P
バースト: S [0x30|W] [生データ ...]                                          P
```

| # | 動作 | レジスタ | 備考 |
|---|------|----------|------|
| 1 | `MODE` が `'PTCH'` か確認 | `0x03` | `'APP '` なら二重ロード防止で中断 |
| 2 | 滞留割り込みクリア | `0x18` | 11 バイト `0xFF` |
| 3 | `PBMs` パラメータ書き込み | `0x09` | `[size LE 4B][0x30][0x32]`（timeout=5s） |
| 4 | `DATA1` 読み戻し確認 | `0x09` | 500us 待ち後、6 バイト一致を確認 |
| 5 | `CMD1 = "PBMs"` → 完了待ち | `0x08` | `CMD1[0]==0x00` 完了 / `0x21('!')` 拒否 |
| 6 | `PatchStartStatus` 確認 | `0x09` | `0x00` 成功 / `0x04,0x05,0x06` 各種不正 |
| 7 | バーストデータ書き込み | → `0x30` | `TPS25751_BURST_CHUNK_SIZE` 単位、各回 500us 待ち |
| 8 | 滞留割り込み再クリア | `0x18` | |
| 9 | `CMD1 = "PBMc"` → 完了待ち | `0x08` | CRC チェック＆パッチ適用 |
| 10 | 20ms 待機 | — | SLVUCR8B 規定 |
| 11 | 完了ステータス確認 | `0x09` | `DevicePatchCompleteStatus`(Byte3) / `AppConfigPatchCompleteStatus`(Byte4) |
| 12 | `MODE` が `'APP '` になるまでポーリング | `0x03` | |

## 調整パラメータ（`tps25751.h`）

| マクロ | 既定 | 説明 |
|--------|------|------|
| `TPS25751_ADDR_REGISTER` | `0x22` | レジスタアクセス用アドレス |
| `TPS25751_ADDR_PBM_BURST` | `0x30` | バーストデータ書き込み先（`0x00` および ADCINx で選択済みのアドレスは不可） |
| `TPS25751_PBM_TIMEOUT_100MS` | `0x32` | PBM タイムアウト（5 秒） |
| `TPS25751_BURST_CHUNK_SIZE` | `1024` | バースト 1 トランザクションの最大バイト数。PD 側ポインタは自動インクリメントされ START/STOP でリセットされないため分割可 |
| `TPS25751_MODE_POLL_MAX` | `200` | `MODE` ポーリング上限（×10ms） |
| `TPS25751_CMD_POLL_MAX` | `600` | `CMD1` 完了ポーリング上限（×10ms） |
| `TPS25751_I2C_WAIT_LOOP_MAX` | `5000000` | 1 転送の完了ビジーウェイト上限 |

## 注意 / 制限

- **プレースホルダのパッチバンドル（全 0）のままでは `PBMc` の CRC チェックで失敗します。** 必ず実データに差し替えてください。
- クロック設定（`iic_master_clock_settings_t`）は P0CLK 実値に依存します。Smart Configurator に計算させた値を使ってください（`tps25751_i2c1_cfg.c` のテンプレート値は仮）。
- 本サンプルはベアメタル前提です。FreeRTOS 等で使う場合は `R_BSP_SoftwareDelay` を `vTaskDelay` に、I2C 完了待ちのビジーループをセマフォ/タスク通知に置き換えてください（`tps25751_i2c_wait()` と `tps25751_delay_*()`）。
- `I2Ct_IRQ` 線は使用していません。使う場合は各 4CC 完了待ち（`tps25751_run_4cc()` のポーリング）を GPIO 立ち下がり監視に置き換え、手順 2/8 で `INT_MASK1(0x16)` に CMD1 Complete / Ready-for-Patch を設定してください（JAJA940A ステップ 3/11）。
- デッドバッテリ（ADCIN2）や ADCIN1 によるアドレス選択はハード設定の範囲であり、本コードの対象外です。
