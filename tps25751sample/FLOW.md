# TPS25751 パッチバンドル・ロード — 処理フロー図解

[README.md](README.md) の補足資料。`codes/` のサンプルコードが、どの層で何を呼び、
どんな I2C トランザクションを発行して `PTCH` → `APP` へ遷移させるかを図で示す。

対応コード:
[tps25751.c](tps25751.c) / [tps25751.h](tps25751.h) / [tps25751_sample_main.c](tps25751_sample_main.c)

根拠資料: TPS25751 TRM (SLVUCR8B) §1.3 / §4.5 / §5.2、Application Note JAJA940A (SLVAFV8) §4.1。

---

## 1. レイヤ構成と役割

```mermaid
flowchart TD
  A["tps25751_sample_run()<br/>tps25751_sample_main.c"] --> B["tps25751_open()"]
  A --> C["tps25751_load_patch_bundle()"]

  B --> D["R_IIC_MASTER_Open()<br/>R_IIC_MASTER_CallbackSet()"]

  C --> E["中位ヘルパ (tps25751.c)<br/>tps25751_reg_write / tps25751_reg_read<br/>tps25751_burst_write / tps25751_set_addr<br/>tps25751_run_4cc / tps25751_clear_interrupts"]
  E --> F["FSP r_iic_master API<br/>R_IIC_MASTER_Write / R_IIC_MASTER_Read<br/>R_IIC_MASTER_SlaveAddressSet"]
  F --> G["FSP r_iic_master ドライバ<br/>(RZ/N2H I2C1 ペリフェラル)"]
  G -->|"I2Ct バス"| H["TPS25751<br/>0x22=レジスタ / 0x30=バースト"]
  G -.->|"TXI/RXI/TEI/ERI 割り込み"| I["tps25751_i2c_callback()<br/>i2c_event 保存 / i2c_done = true"]
  I -.-> E
```

- **サンプル層** … 呼び出し例。結果を `g_tps25751_last_*` に保持。
- **ドライバ層 (`tps25751.c`)** … TPS25751 の手順・レジスタプロトコルを実装。
- **FSP `r_iic_master`** … `R_IIC_MASTER_*` を直接呼び出し（汎用 `p_api` 経由ではない）。
- 転送は**非ブロッキング**。完了はコールバックが `i2c_done` を立て、`tps25751_i2c_wait()` がポーリングで拾う（ベアメタル）。

---

## 2. TPS25751 のモード遷移

```mermaid
stateDiagram-v2
  [*] --> BOOT: パワーオン / GAID
  BOOT --> PTCH: EEPROM 未検出<br/>(I2Cc の SDA 切断構成)
  PTCH --> PTCH: PBMs 成功<br/>+ バーストデータ受信
  PTCH --> APP: PBMc 成功<br/>(CRC OK → パッチ適用)
  APP --> [*]: 通常動作 (全レジスタ利用可)

  note right of PTCH
    アクセス可能なレジスタは
    MODE(0x03) / CMD1(0x08) / DATA1(0x09)
    INT_EVENT(0x14) / INT_MASK(0x16)
    INT_CLEAR(0x18) / BOOT_FLAGS(0x2D) のみ
  end note
```

本サンプルは「コールドブート後 `PTCH` で待機している」状態から開始する前提。

**`APP` に入る前は PD ポリシーエンジンが実質停止**しており、PDO 設定が無いため
**ソース（給電側）としては動作しない**（USB-C を挿しても VBUS へ給電しない）。
`PTCH` で生きているのは ADCIN2 ストラップで決まるデッドバッテリ／シンク経路のみ。
EEPROM（I2Cc, 0x50）非搭載の設計では、電源投入のたびにこのロードが必須。
詳細は [README.md](README.md) 「なぜこのロードが必要か（EEPROM 非搭載時）」。

---

## 3. 全体シーケンス（`tps25751_load_patch_bundle()`）

```mermaid
sequenceDiagram
  autonumber
  participant H as Host (tps25751.c)
  participant T as TPS25751

  Note over H,T: ターゲットアドレス 0x22 = レジスタアクセス

  H->>T: [0x22] MODE(0x03) 読み出し
  T-->>H: "PTCH"
  H->>T: [0x22] INT_CLEAR1(0x18) <- 0xFF x11
  H->>T: [0x22] DATA1(0x09) <- size(LE4) + 0x30 + 0x32
  H->>H: 500us 待ち
  H->>T: [0x22] DATA1(0x09) 読み戻し
  T-->>H: 6 バイト一致を確認

  H->>T: [0x22] CMD1(0x08) <- "PBMs"
  loop CMD1[0] が 0x00 になるまで (10ms 間隔)
    H->>T: [0x22] CMD1(0x08) 読み出し
    T-->>H: "PBMs"(実行中) / 0x00(完了) / '!'(拒否)
  end
  H->>T: [0x22] DATA1(0x09) 読み出し
  T-->>H: PatchStartStatus == 0x00

  Note over H,T: SlaveAddressSet で 0x30 に切替
  loop バンドル全体を CHUNK 単位で送信
    H->>T: [0x30] パッチバンドル生データ (最大 TPS25751_BURST_CHUNK_SIZE)
    H->>H: 500us 待ち
  end
  Note over H,T: SlaveAddressSet で 0x22 に戻す

  H->>T: [0x22] INT_CLEAR1(0x18) <- 0xFF x11
  H->>T: [0x22] CMD1(0x08) <- "PBMc"
  loop CMD1[0] が 0x00 になるまで (10ms 間隔)
    H->>T: [0x22] CMD1(0x08) 読み出し
    T-->>H: 0x00(完了) / '!'(拒否)
  end
  H->>H: 20ms 待ち
  H->>T: [0x22] DATA1(0x09) 40 バイト読み出し
  T-->>H: Device / AppConfig 完了ステータス

  loop MODE が "APP " になるまで (10ms 間隔)
    H->>T: [0x22] MODE(0x03) 読み出し
    T-->>H: "APP "
  end
```

---

## 4. フローチャート（分岐とエラー終了）

```mermaid
flowchart TD
  S([tps25751_load_patch_bundle]) --> A["set_addr(0x22)"]
  A --> B{"MODE == 'PTCH' ?<br/>最大 200 x 10ms"}
  B -- "'APP ' / タイムアウト" --> E1[["return ERR_NOT_PTCH"]]
  B -- Yes --> C["INT_CLEAR1(0x18) クリア"]
  C --> D["DATA1(0x09) <- PBMs パラメータ<br/>size(LE4) + 0x30 + 0x32"]
  D --> F["500us 待ち → DATA1 読み戻し"]
  F --> G{"6 バイト一致 ?"}
  G -- No --> E2[["return ERR_DATA1_VERIFY"]]
  G -- Yes --> H["CMD1(0x08) <- 'PBMs'<br/>→ CMD1 完了ポーリング"]
  H --> I{"結果"}
  I -- "'!CMD'" --> E3[["return ERR_PBMS_REJECTED"]]
  I -- "タイムアウト" --> E4[["return ERR_PBMS_TIMEOUT"]]
  I -- "CMD1[0]=0x00" --> J["DATA1(0x09) 読み出し"]
  J --> K{"PatchStartStatus == 0 ?"}
  K -- "0x04/0x05/0x06" --> E5[["return ERR_PATCH_START"]]
  K -- Yes --> L["set_addr(0x30)"]
  L --> M["バンドルを CHUNK 単位でバースト書き込み<br/>各回 500us 待ち"]
  M --> N{"全チャンク成功 ?"}
  N -- No --> E6[["set_addr(0x22)<br/>return ERR_BURST"]]
  N -- Yes --> O["set_addr(0x22)<br/>INT_CLEAR1(0x18) クリア"]
  O --> P["CMD1(0x08) <- 'PBMc'<br/>→ CMD1 完了ポーリング"]
  P --> Q{"結果"}
  Q -- "'!CMD'" --> E7[["return ERR_PBMC_REJECTED"]]
  Q -- "タイムアウト" --> E8[["return ERR_PBMC_TIMEOUT"]]
  Q -- "CMD1[0]=0x00" --> R["20ms 待ち<br/>DATA1(0x09) 40 バイト読み出し"]
  R --> U{"Device == 0x00 かつ<br/>AppConfig != 0x80 ?"}
  U -- No --> E9[["return ERR_PATCH_COMPLETE"]]
  U -- Yes --> V{"MODE == 'APP ' ?<br/>最大 200 x 10ms"}
  V -- No --> E10[["return ERR_NOT_APP"]]
  V -- Yes --> OK[["return TPS25751_OK"]]
```

結果コードの詳細は [tps25751.h](tps25751.h) の `tps25751_result_t` を参照。

---

## 5. 低レベル I2C ヘルパのバイト構成

SLVUCR8B Figure 1-2 / 1-3 のプロトコルを `R_IIC_MASTER_Write` / `R_IIC_MASTER_Read`
の `restart` 引数で組み立てている。

```
tps25751_reg_write(reg, data[len])
  R_IIC_MASTER_Write(buf, len+2, restart = false)   // 末尾 STOP
  ┌── S ─┬ 0x22+W ┬ reg ┬ len ┬ data[0] ┬ … ┬ data[len-1] ┬ P ──┐

tps25751_reg_read(reg, out[len])
  R_IIC_MASTER_Write([reg], 1, restart = true)       // STOP を出さない
  R_IIC_MASTER_Read (buf, len+1, restart = false)    // 末尾 STOP
  ┌ S ┬ 0x22+W ┬ reg ┬ Sr ┬ 0x22+R ┬ cnt ┬ d[0] ┬ … ┬ d[len-1] ┬ P ┐
      out[0..len-1] = buf[1..len]      // buf[0] = デバイスが返すバイト数

tps25751_burst_write(data[len])       // 事前に set_addr(0x30) 済み
  R_IIC_MASTER_Write(data, len, restart = false)     // 末尾 STOP
  ┌── S ─┬ 0x30+W ┬ raw[0] ┬ raw[1] ┬ … ┬ raw[len-1] ┬ P ──┐
```

---

## 6. 1 トランザクションの完了待ち（非同期 → ポーリング）

```mermaid
sequenceDiagram
  participant D as tps25751_reg_* / _burst_write
  participant F as R_IIC_MASTER_Write/Read
  participant ISR as r_iic_master 割り込み
  participant CB as tps25751_i2c_callback

  D->>D: i2c_done = false
  D->>F: 転送開始（非ブロッキング）
  F-->>D: FSP_SUCCESS
  D->>D: tps25751_i2c_wait()<br/>while(!i2c_done) でスピン
  ISR->>CB: 転送完了 (TX_COMPLETE / RX_COMPLETE / ABORTED)
  CB->>CB: i2c_event = event<br/>i2c_done = true
  D->>D: i2c_done==true で復帰<br/>ABORTED なら return ERR_I2C
```

`TPS25751_I2C_WAIT_LOOP_MAX` 回スピンしても完了しなければ `ERR_I2C` で抜ける
（配線・クロック設定・プルアップ不良などの検出用フェイルセーフ）。

> FreeRTOS 等で使う場合は、このスピンをセマフォ / タスク通知待ちに、
> `R_BSP_SoftwareDelay` を `vTaskDelay` に置き換える（[README.md](README.md) 「注意 / 制限」参照）。
