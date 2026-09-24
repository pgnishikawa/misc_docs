# EST3000 準拠：MRAM 工場出荷後の初回初期化 (DFIM) 手順

[← README（目次）へ戻る](README.md)

出典: `MRAM/EST3000 Device Initialization Reset and Recovery EMxxLX_Rev2.1.pdf`
（Everspin Technologies、Application Note EST 3000 Rev.2.1、2024年11月、以下「EST3000」）。
RZ/N2H側のレジスタ・マニュアルコマンドの基礎は
[15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md)、
MRAM自体の基礎知識は
[16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md)
を前提とする。本ドキュメントはこれらとは**独立に、かつ先行して1回だけ**実行する必要のある
「工場出荷後の初回初期化（DFIM: Device Factory Initialization Mode）」を扱う。

## 0. これは何のためのドキュメントか（doc15/16との関係）

EST3000は3種類のフローを区別している：

| フロー | 実行タイミング | 本ドキュメントでの扱い |
|---|---|---|
| **Device Initialization Flow**（Figure 1） | **PCB実装後・初回電源投入時に1回だけ**（工場／量産ラインでのテスト工程、または後述§8のリカバリ発生時） | **本ドキュメントの主題**（§4以降） |
| **Device Power On/Reset Flow**（Figure 2） | **通常運用での毎回の電源投入・リセット時** | §1.2で触れる軽量な待ち時間要件のみ。実際のプロトコル設定手順は[15](15_mram_octal_memory_mapped_write_init.md)がカバー済み |
| **Device Recovery Flow**（Figure 3・4） | 通信不能・データ異常を検知した場合 | §8で要約 |

> **重要**: EST3000のDevice Initialization Flow（DFIM）は、**MRAMがリフロー半田付け直後で
> 内部状態が未定義**という前提に立った、**ステータスレジスタ・不揮発設定レジスタ・OTP領域・
> メモリアレイ全体を強制的に既知の状態へ揃える**手順である。[15](15_mram_octal_memory_mapped_write_init.md)
> で説明した「電源投入のたびにOctal DTRへ切り替える」手順とは**目的も実行頻度も異なる**。
> 本手順は基本的に**デバイスの寿命の中で1回**（量産のテスト工程で）実行すればよく、
> 毎回の起動シーケンスに含める必要はない（§9のリカバリ条件に該当しない限り）。

## 1. なぜ必要か

### 1.1 リフロー半田付け後の未定義状態

> The device state cannot be guaranteed after a solder reflow operation, hence a full
> device initialization and configuration is required.（EST3000 Introduction）

MRAMは磁気抵抗素子でビットを記憶するため、**製造・実装（リフロー半田付け）の熱工程を
経た後の各ビットの状態、および各種設定レジスタの内容は保証されない**。したがって、
実際の運用を始める前に「全ビット・全レジスタを既知のパターンで上書きし、読み戻して
検証する」という初期化を1回行う必要がある。これを怠った場合、Status Register・
Nonvolatile Configuration Register・OTP領域・メモリアレイの内容が不定のまま
運用を始めることになり、**予期しない書き込み保護（Block Protectビットが意図せず1）や
誤ったI/Oモード設定など、原因究明が困難な不具合につながる**。

### 1.2 通常起動時の電源投入タイミング要件（Figure 2、doc15への補足）

EST3000 Figure 2（Device Power On/Reset Flow）は、リフロー後の初回だけでなく
**通常運用の毎回の電源投入・リセットでも共通して守るべき最低限の待ち時間**を
定めている：

```
Device is powered on, or Device reset
        ↓
Wait tPU = 350µs（電源投入・電源サイクル後）
      または 200ns（デバイスリセット後）
        ↓
Device is Ready for operation
```

**この待ち時間は[15](15_mram_octal_memory_mapped_write_init.md)の手順には明記されていなかった**。
RZ/N2H側でxSPI1モジュールを有効化しMRAMへ最初のマニュアルコマンド（WREN等）を
発行する前に、**電源投入からtPU=350µs、またはMRAMへのハードウェア/JESD252リセット
発行から200ns**のウェイトを入れること。実装上は単純な busy-wait や、電源シーケンサ側の
タイミング設計に織り込む形でよい。

## 2. JESD252 シグナルシーケンスリセットの詳細

[16](16_mram_fundamentals_for_software_engineers.md) §7で触れた「専用リセットピンを
使わないリセット手段」について、EST3000は次のように具体的な指針を与えている：

> Not all MCU controllers natively support JESD252 Signal reset sequence... it is
> recommended customers investigate the GPIO remapping capabilities of the MCU
> controller... remap CS#, CK, IO 0:1 as GPIO signals. Using the logic control
> capabilities of the GPIO signals perform the Signal Reset sequence.（EST3000 §3）

**RZ/N2Hのxs1（MRAM接続先）には`XSPIm_RESET0#`のような専用リセット出力ピンが
存在しない**（[14](14_mram_em064lx_xspi1_octal_connection.md) §6・[16](16_mram_fundamentals_for_software_engineers.md)
§7で確認済み）ため、本プロジェクトでは**まさにこのGPIOリマップ方式が唯一の
実装可能なハードウェアリセット手段**になる。

### 2.1 タイミング仕様（EST3000 Figure 5）

| パラメータ | 記号 | 最小値 | 単位 | 意味 |
|---|---|---|---|---|
| CS# Low時間 | `tSL` | 500 | ns | リセットシーケンス中のCS# Low区間の最小幅 |
| CS# High時間 | `tSH` | 500 | ns | CS# High区間の最小幅 |
| データセットアップ時間 | `tDVSR` | 5 | ns | CS#の変化に対するIO0データのセットアップ時間 |
| データホールド時間 | `tSDVR` | 5 | ns | 同ホールド時間 |
| 内部リセット確定までの時間 | — | 200 | ns | シーケンス完了後、内部的にリセットが有効になるまでの時間（§1.2の「リセット後200ns」の根拠） |

CK（クロック）は**シーケンス中ずっとHigh（mode 3）またはLow（mode 0）に固定**し、
トグルさせてはならない（「コマンドと誤認させないため」とEST3000に明記）。
IO1は常にHigh-Zのままでよい。

**RZ/N2H実装上の注意**: 具体的なCS#/CK/IO0の駆動パターン（何クロック分・どのタイミングで
Low/Highにするか）はEST3000 Figure 5の波形図に依存し、本ドキュメントでは波形の詳細
までは転記していない。実装時は必ずEST3000原本のFigure 5、またはJESD252規格書
そのものを直接参照してビットパターンを確認すること。

RZ/N2H側では、[15](15_mram_octal_memory_mapped_write_init.md) §2で設定した
`XSPI1_CKP`(P01_0)・`XSPI1_CS0#`(P01_1)・`XSPI1_IO0`(P01_4)・`XSPI1_IO1`(P01_5)の
各ピンを、**このリセットシーケンスを送出する間だけ一時的にGPIO（`PMCm`のビットを
`0`に戻す）へ切り替え**、`Pm`レジスタで論理値を直接制御してビットバンギングする
実装になる。シーケンス完了後は`PMCm`を再度周辺機能（`0x1C`）へ戻し、
[15](15_mram_octal_memory_mapped_write_init.md) §4以降の通常のxSPI1動作へ復帰する。

## 3. DFIM (Device Factory Initialization Mode) とは

DFIMは、MRAM内部の「工場出荷時初期化専用モード」で、このモード中でなければ
実行できない特殊な初期化操作（NVCR全域の強制書き換え、OTP領域の初期化等）を
許可する状態。

- **エントリー**: `Write Volatile Configuration Register (0x81)`コマンドで、
  アドレス`0x1E`に**Manufacturer ID `0x6B`**を書き込む
- **イグジット**: 同じくアドレス`0x1E`に`0x00`を書き込む

`0x1E`はこれまでの[14](14_mram_em064lx_xspi1_octal_connection.md)/[15](15_mram_octal_memory_mapped_write_init.md)/[16](16_mram_fundamentals_for_software_engineers.md)
で扱ったVCR0/1/5/8等とは別枠の、DFIM専用の制御アドレスである点に注意
（EMxxxLX/LXBデータシートのDFIM節を参照。本ドキュメントではEST3000の記載のみに基づく）。

## 4. 初期化対象と手順の全体像（Figure 1、最新版のロジック）

Rev2.1で「NVCR・OTP領域の初期化は必須（以前はオプション）」に変更され、
さらに「Status Register・NVCR・OTP領域それぞれについて、初期化パスを**2回連続で
実行する**」という要件が追加されている（Revision History参照）。全体の流れ：

1. **JESD252リセット**を発行し、ホスト側インタフェースをSPI 1S-1S-1Sへ確定させる（§2）
2. **WREN**（`0x06`）でWELビットを`1`にする
3. （任意・高速化目的）**Volatile Configuration Register 0**でI/Oモードを希望の
   プロトコルへ変更し、RZ/N2H側の`LIOCFGCS0`等も同じプロトコルへ合わせる
   （[15](15_mram_octal_memory_mapped_write_init.md) §6・§7と同じ操作。
   後述の通りメモリアレイ全体を複数回走査するため、**Octal DTRへ切り替えてから
   DFIMを実行した方が大幅に高速**）
4. **DFIMへエントリー**（§3）
5. **Status Registerの初期化**（§5.1）を**2回**実行
6. **Nonvolatile Configuration Register 0～12（13個）の初期化**（§5.2）を**2回**実行
7. **OTP領域（256バイト）の初期化**（§5.3）を**2回**実行
8. **メモリアレイ全体の初期化**（§5.4）
9. **DFIMをイグジット**（§3）
10. デバイスは通常の設定・運用が可能な状態になる

## 5. 各対象の初期化パターン

いずれも「**まず`0x00`パターンで埋め、次に`0xFF`パターンで埋める**」（EST3000は
`0x00`→`0xFF`の順で統一）という書き込みを行い、必要に応じて読み戻して確認する
（読み戻し検証は「オプション」とされているが、本番前の初回ブリングアップでは
実施を強く推奨する）。

### 5.1 Status Register

```
a. Write Status Register (0x01), data = 0xFF
b. (推奨) Read Status Register (0x05) → 期待値 0xFE
   （書き込み対象はbit7:2のみ。bit1(WEL)は直前のWRENで1のまま、bit0(WIP)=0のため
     0xFF書き込み後の読み出し期待値は 1111_1110 = 0xFE）
c. Write Status Register (0x01), data = 0x00
d. (推奨) Read Status Register (0x05) → 期待値 0x02
   （bit7:2=0、bit1(WEL)=1、bit0(WIP)=0 → 0000_0010 = 0x02）
```

ステップcの`data=0x00`により**Block Protect(BP[3:0])とTop/Bottomビットがクリア**され、
「メモリアレイ全域が書き込み可能」な状態になる（EST3000 §6「For array initialization
and recovery, the BP bits [6:2] must be cleared」）。**この手順を怠ると、後続の
アレイ全体初期化が書き込み保護によって失敗する**。

このa~dを**2回連続で実行**する（Rev2.1で追加された要件）。

### 5.2 Nonvolatile Configuration Register 0～12

アドレス`0x0000`～`0x000C`の13レジスタそれぞれについて：

```
a. Write Nonvolatile Configuration Register (0xB1), addr, data = 0x00
b. (推奨) Read Nonvolatile Configuration Register (0xB5), addr → 期待値 0x00
c. Write Nonvolatile Configuration Register (0xB1), addr, data = 0xFF
d. (推奨) Read Nonvolatile Configuration Register (0xB5), addr → 期待値 0xFF
```

このa~dを13レジスタ全てに対して行い、さらに**その全体を2回連続で実行**する。

> **注意**: NVCR0（I/Oモード）・NVCR1（ダミーサイクル）等、既にプロトコル選択や
> タイミングに使っている設定値も、この初期化パスでは一旦`0x00`→`0xFF`で
> 上書きされる。DFIMを抜けた後、[15](15_mram_octal_memory_mapped_write_init.md)で
> 説明した本来の運用値（Octal DTR等）へ**あらためて設定し直す**運用にすること
> （DFIMはあくまで「既知の状態に揃える」ための手順であり、最終的な運用設定は
> DFIM後に別途行う）。

### 5.3 OTP領域（256バイト）

```
a. Write Volatile Configuration Register 8, data = 0xF9  （OTPロック無効化）
b. OTP Write (0x42), 全アドレス, data = 0xFF
c. (推奨) OTP Read (0x4B), 全アドレス → 期待値 0xFF
   （その後、b/cをdata=0x00で再実行）
```

これを**2回連続で実行**する。

> **副作用の注意**: `Register 8`は「OTPロック(bit2)」だけでなく「RPE=Reset Pin
> Enabled(bit1)」「Write mode=Persistent/NOR Emulation(bit0)」も同居する1バイト
> レジスタである。EST3000指定の`0xF9`（`1111_1001b`）は、**OTPロックを無効化
> すると同時にbit1(RPE)を`0`＝Reset Pin Disabledにする**（bit0は`1`のままPersistent
> Memory Modeを維持）。これがEST3000の意図した挙動か、単に「OTPロック解除に
> 必要な最小限の他ビット値」を埋めているだけかは本ドキュメント単体では判別できない。
> **`RESET#`ピンの有効/無効に依存する設計をしている場合は、このステップの前後で
> Register 8の値を読み戻し、意図しない設定変更が残っていないか確認すること**
> （DFIM完了後、§4手順の最後で運用値へ再設定する際にRPEも含めて明示的に
> 設定し直すのが安全）。

> **OTPアドレス範囲の注意**: EST3000は"n=257"というアドレス範囲を明記しているが、
> MRAM DSではOTP領域は256バイトと説明されている（[16](16_mram_fundamentals_for_software_engineers.md)は
> この前提で記載）。1バイトの差異が実装上意味を持つ可能性があるため、
> 実装前にEMxxxLXデータシートのOTP節（§14）でアドレス範囲の上限を再確認すること。

### 5.4 メモリアレイ全体

64Mbit以下の品番（**本プロジェクトのEM064LXOAB320IS2Tはこれに該当**）では、
バイト単位のWriteコマンドではなく**Erase/Bulk Chip (`0xC7`)**を使う（EST3000
Note2「Erase/Bulk Chip may be used in place of write commands」＋実際に
Command Sequence§13でも`0xC7`が使われている）：

```
9.  Write Volatile Configuration Register 8, data = 0xFF （Erase Bit Value → 全体を0で埋める設定）
    Erase/Bulk Chip (0xC7)
10. Write Volatile Configuration Register 8, data = 0x7F （Erase Bit Value → 全体を1で埋める設定）
    Erase/Bulk Chip (0xC7)
11. 手順9を再実行し、(推奨)読み戻し検証
12. 手順10を再実行し、(推奨)読み戻し検証
```

> **要確認（本ドキュメントでは解決していない矛盾）**: MRAM DSのNonvolatile/Volatile
> Configuration Register 8 bit7（Erase Bit Value）の定義は「`1`=Erase with '1'
> （既定値）、`0`=Erase with '0'」（[16](16_mram_fundamentals_for_software_engineers.md) §6）。
> ところがEST3000手順9（"Write entire Device to **0**"）では`data=0xFF`（bit7=**1**）を
> 使い、手順10（"Write entire Device to **1**"）では`data=0x7F`（bit7=**0**）を使っており、
> **データシートの定義と字面上、対応関係が逆に見える**。本ドキュメントはEST3000の
> 記載を字句通り転記したが、この対応関係の食い違いは解消できていない。
> **実装前に必ずEverspinへ確認するか、実機で少量のアドレス範囲に対して
> 実際にどちらのデータで埋まるかを検証すること**（[14](14_mram_em064lx_xspi1_octal_connection.md)
> §7の要検証事項に追記推奨）。

**128Mbit品番の場合**（本プロジェクトでは非該当、将来の型番変更に備えた参考情報）は、
`0xC7`の前に`Select lower/upper address (0xC4)`で対象半分（下位/上位）を選択し、
それぞれに対して`0xC7`を発行する必要がある（EST3000 §13「For Device Size =128Mbit」）。

## 6. RZ/N2H側マニュアルコマンドへの対応

[15](15_mram_octal_memory_mapped_write_init.md) §5-§6で定義した`CDCTL0`/`CDTBUF0`/
`CDABUF0`/`CDD0BUF0`のマクロ・関数をそのまま流用する。1S-1S-1Sモード
（`LIOCFGCS0.PRTMD=0x000`）・3バイトアドレスモード前提での`CDTBUF0`構成値：

| コマンド | オペコード | `CMDSIZE` | `ADDSIZE` | `DATASIZE` | `LATE` | `TRTYPE` | `CDTBUF0`値 |
|---|---|---|---|---|---|---|---|
| Write Status Register | `0x01` | 1byte | 0byte | 1byte | 0 | 1(write) | `0x01008021` |
| Read Status Register | `0x05` | 1byte | 0byte | 1byte | 0 | 0(read) | `0x05000021` |
| Write Nonvolatile Configuration Register | `0xB1` | 1byte | 3byte | 1byte | 0 | 1 | `0xB100802D` |
| Read Nonvolatile Configuration Register | `0xB5` | 1byte | 3byte | 1byte | 0 | 0 | `0xB500002D` |
| OTP Write | `0x42` | 1byte | 3byte | 1byte | 0 | 1 | `0x4200802D` |
| OTP Read | `0x4B` | 1byte | 3byte | 1byte | DCC（VCR1既定値16） | 0 | `0x4B00002D \| (DCC<<9)` |
| Erase/Bulk Chip | `0xC7` | 1byte | 0byte | 0byte | 0 | 1 | `0xC7008001` |
| Write Volatile Configuration Register（Register 8操作用、[15](15_mram_octal_memory_mapped_write_init.md) §6と同型） | `0x81` | 1byte | 3byte | 1byte | 0 | 1 | `0x8100802D`（アドレス`0x00000008`） |

```c
/* [15] §4-§6 のマクロ・CDCTL0/CDTBUF0/CDABUF0/CDD0BUF0 定義を流用 */

static void manual_cmd(uint32_t cdtbuf, uint32_t addr, uint32_t wdata, uint32_t *rdata)
{
    while (CDCTL0 & 0x1u) { }
    CDTBUF0  = cdtbuf;
    CDABUF0  = addr;
    CDD0BUF0 = wdata;
    CDCTL0   = 0x00000001u;
    while (CDCTL0 & 0x1u) { }
    if (rdata) *rdata = CDD0BUF0;
}

static inline void mram_write_status(uint8_t data) {
    manual_cmd(0x01008021u, 0, data, NULL);
}
static inline uint8_t mram_read_status(void) {
    uint32_t r; manual_cmd(0x05000021u, 0, 0, &r); return (uint8_t)r;
}
static inline void mram_write_nvcr(uint8_t addr, uint8_t data) {
    manual_cmd(0xB100802Du, addr, data, NULL);
}
static inline uint8_t mram_read_nvcr(uint8_t addr) {
    uint32_t r; manual_cmd(0xB500002Du, addr, 0, &r); return (uint8_t)r;
}
static inline void mram_otp_write(uint32_t addr, uint8_t data) {
    manual_cmd(0x4200802Du, addr, data, NULL);
}
static inline void mram_erase_bulk_chip(void) {
    manual_cmd(0xC7008001u, 0, 0, NULL);
}
static inline void mram_write_vcr8(uint8_t data) {
    manual_cmd(0x8100802Du, 0x00000008u, data, NULL);
}
static inline void mram_dfim_enter(void) { manual_cmd(0x8100802Du, 0x0000001Eu, 0x6Bu, NULL); }
static inline void mram_dfim_exit(void)  { manual_cmd(0x8100802Du, 0x0000001Eu, 0x00u, NULL); }

void mram_est3000_factory_init(void)
{
    /* §1.2: 電源投入後 tPU=350us、リセット後 200ns のウェイトを事前に確保しておくこと */
    /* §2: 必要ならここでJESD252シグナルシーケンスリセットを実施 */

    mram_wren_1s1s1s();               /* [15] §5 と同じ */
    /* (任意) [15] §6・§7でOctal DTRへ切替：大量データを扱うため強く推奨 */

    mram_dfim_enter();

    for (int pass = 0; pass < 2; pass++) {
        mram_write_status(0xFF);
        /* 推奨: assert(mram_read_status() == 0xFE); */
        mram_write_status(0x00);
        /* 推奨: assert(mram_read_status() == 0x02); */

        for (uint8_t addr = 0; addr <= 0x0C; addr++) {
            mram_write_nvcr(addr, 0x00);
            /* 推奨: assert(mram_read_nvcr(addr) == 0x00); */
            mram_write_nvcr(addr, 0xFF);
            /* 推奨: assert(mram_read_nvcr(addr) == 0xFF); */
        }

        mram_write_vcr8(0xF9);        /* OTPロック無効化（§5.3の副作用注意を参照） */
        for (uint32_t a = 0; a < 256u; a++) mram_otp_write(a, 0xFF);
        /* 推奨: 読み戻し検証 */
        mram_write_vcr8(0xF9);
        for (uint32_t a = 0; a < 256u; a++) mram_otp_write(a, 0x00);
        /* 推奨: 読み戻し検証 */
    }

    /* §5.4・§7の要確認事項に留意しつつ実施 */
    mram_write_vcr8(0xFF);            /* Erase Bit Value 設定（要確認、§5.4参照） */
    mram_erase_bulk_chip();
    mram_write_vcr8(0x7F);
    mram_erase_bulk_chip();
    mram_write_vcr8(0xFF);
    mram_erase_bulk_chip();
    /* 推奨: 読み戻し検証 */
    mram_write_vcr8(0x7F);
    mram_erase_bulk_chip();
    /* 推奨: 読み戻し検証 */

    mram_dfim_exit();

    /* この後、[15] の手順に従って本来の運用設定（Octal DTR、Register8の
       Persistent Memory Mode/RPE等）を明示的に設定し直すこと */
}
```

上記はEST3000のロジックをそのまま関数化したものであり、**64Mb品番（Erase/Bulk
Chipが単一ユニットで完結する）を前提**にしている。実行時間は、Status Register・
NVCR部分は数百コマンド程度で軽微だが、**OTP 256バイト×2パス×2回（Write+VerifyでOTP
Read込み）は最低でも1000コマンド超**になる点に留意（マニュアルコマンド1回あたりの
オーバーヘッドを考慮した実行時間見積りを推奨）。メモリアレイ全体自体は`0xC7`
（Erase/Bulk Chip）で一括処理されるため、こちらはコマンド発行回数自体は少ないが、
内部の消去・検証時間（MRAM DSに正式なタイミング値の記載がない、
[14](14_mram_em064lx_xspi1_octal_connection.md) §7参照）を考慮しWIPビットのポーリングを
挟むこと。

## 7. Flag Status Register・Interrupt Status Register（監視・エラー検知用）

初期化・通常運用いずれでも、以下のレジスタで異常を検知できる：

**Flag Status Register**（`Read Flag Status Register (0x70)`で読む。電源投入時に
全ビット0クリア、エラービットは`Clear Flag Status Register (0x50)`で明示的にクリアが必要）:

| bit | 名前 | 意味 |
|---|---|---|
| 0 | Addressing | 現在3byte/4byteアドレスモードのどちらか |
| 1 | Protection | Block Protectで保護された領域への書込み試行、またはロック済みOTPへの書込み試行 |
| 3 | CRC | CRC不一致 |
| 4 | Write (Program) | 書き込み失敗（**WRENが立っていない状態での書き込みは失敗する**） |
| 5 | Erase | 消去失敗（同上、WREN必須） |
| 7 | Write/Erase | `1`=Ready、`0`=Busy（Status RegisterのWIPと相補的な情報） |

**Interrupt Status Register**（`0x0010`、Read/Write Volatile Configuration
Registerコマンドでアクセス。`Interrupt Mask`は`0x000F`）:

| bit | 名前 | 意味 |
|---|---|---|
| 0 | Erase Done | Read, Write 1 to Clear |
| 1 | CRC Done | Read, Write 1 to Clear |
| 2 | **Power On Error** | Read, Write 1 to Clear。**ハードウェアリセットでもクリアされないため、明示的に1を書いてクリアする必要がある** |

`Power On Error`（bit2）が立っている場合は、後述§8のリカバリフローへ進む
（EST3000 §1「Detection of a Power-on error as shown in the Interrupt Status
Register, bit 2」）。

## 8. リカバリフロー（通信不能・データ異常検知時）

DFIM完了後の通常運用中に、次のいずれかを検知した場合はリカバリ（実質的に
本ドキュメントのDFIM手順の再実行）を検討する（EST3000 §1・Figure 3・4）：

- MRAMと通信できない（ウォッチドッグタイムアウト、または別メモリ資源から
  実行される診断コードでのデータ整合性検証の失敗）
- `Interrupt Status Register` bit2（Power On Error）が`1`
- NVCR・メモリアレイ・OTP領域のデータ異常（ホスト側のチェックサム/CRC等で検知）

Figure 3の切り分け手順（要約）：

1. 現在想定しているxSPIプロトコルでDevice IDを読み、期待値と一致するか確認
2. 一致しなければ：①XiP解除シーケンスを試す →②別のプロトコルモードを試す →
   ③ハードウェアリセットを試す → ④JESD252リセットを行いホスト側も
   工場出荷時プロトコル(SPI 1S-1S-1S)に戻す、の順にエスカレーション
3. 通信確立後、Volatile Configuration Registerを希望プロトコルへ再設定
4. `Interrupt Status`bit2を確認し、`1`ならエラーとして扱う
5. Status/Configuration Register・メモリアレイ・OTPの内容を読み戻して検証
6. 全て正常ならデバイス運用再開、異常があれば**本ドキュメントのDFIM手順
   （§4-§6）を再実行**（Figure 4「Device Recovery Flow Part 2」＝
   Device Initialization Flowの再実施そのもの）

## 9. 極端な温度暴露時の再実施要件

> The DFIM factory initialization procedure is intended to be a one-time event...
> If there are any subsequent solder reflow or extreme temperature exposures, the
> DFIM factory initialization procedure will need to be completed prior to device
> operation. In this case, extreme temperature exposure is 125°C for one hour or
> more.（EST3000 §9）

**基板の後工程でのリワーク（部品交換のための再リフロー等）や、125℃以上に
1時間以上さらされるイベントが発生した場合、本ドキュメントのDFIM手順を
再実行する必要がある**。量産後の修理・改造フローがある場合は、この条件を
作業手順書に明記しておくこと。

## 10. 設定保存・検証の推奨

> After the device is configured for desired operation, it is recommended to save
> all Volatile, Nonvolatile and Status Registers to a host side structure. This
> structure will be used for comparison during the Initialization and Recovery
> flow.（EST3000 §11）

DFIM完了後・本来の運用設定（[15](15_mram_octal_memory_mapped_write_init.md)の
Octal DTR設定等）を適用した直後に、**Status Register・NVCR0-12・VCR0-8の値を
RZ/N2H側の不揮発領域（例: xSPI0側のブート用フラッシュの一部）にスナップショットとして
保存**しておくと、§8のリカバリ発生時に「正しい設定値」との比較が容易になる。

## 11. 実装への統合方針（要検討）

DFIMの実行主体・タイミングについては、プロジェクトとして以下のいずれかを
選ぶ必要がある（[ET1100/docs/06_implementation_roadmap.md](../../ET1100/docs/06_implementation_roadmap.md)
で扱ったET1100 EEPROMの「初回書き込みの主体」検討と同種の論点）：

| 方式 | 概要 | 長所 | 短所 |
|---|---|---|---|
| (a) 量産テスト工程で1回実施 | 治具・テストプログラムでDFIMを実行し、以後は[15](15_mram_octal_memory_mapped_write_init.md)の通常フローのみをRZ/N2Hのファームウェアに実装 | 製品ファームウェアが単純。DFIMの実行時間（OTP等で数千コマンド規模）を量産ラインでのみ負担すればよい | テスト工程・治具の追加開発が必要 |
| (b) 初回起動時にファームウェアが自動実行 | RZ/N2Hのファームウェアが「未初期化」を検知（例: NVCR0が既定値`0xFF`のまま等）してDFIMを自動実行 | 専用治具が不要 | 初回起動時間が長くなる。誤検知で誤って再DFIMする、または低品質判定ロジックのリスク |

いずれの方式でも、§8のリカバリ判定ロジック自体はファームウェアに実装しておく
必要がある（フィールドでの異常発生に備えるため）。

## 12. まとめチェックリスト

- [ ] 電源投入後`tPU=350µs`、リセット後`200ns`のウェイトを実装した（§1.2）
- [ ] `XSPI1_CKP`/`CS0#`/`IO0`/`IO1`をGPIOへ一時切替してJESD252シグナルシーケンス
      リセットを送出する実装を用意した（専用リセットピンがないため。§2）
- [ ] DFIMエントリー（`0x81`, Addr`0x1E`, Data`0x6B`）／イグジット（同Addr, Data`0x00`）を実装した（§3）
- [ ] Status Register初期化（`0xFF`→`0x00`、BP解除を含む）を**2回**実行する実装にした（§5.1）
- [ ] NVCR 0～12（13レジスタ）の初期化（`0x00`→`0xFF`）を**2回**実行する実装にした（§5.2）
- [ ] OTP領域（256バイト、アドレス範囲は要データシート再確認）の初期化を**2回**実行する実装にした（§5.3）
- [ ] メモリアレイ全体の`0xC7`（Erase/Bulk Chip）による初期化を実装した（§5.4）
- [ ] Register8のErase Bit Value対応（データシートとEST3000の記載の食い違い）をEverspinへ確認、または実機検証した（§5.4）
- [ ] Register8=`0xF9`書き込みがRPE等の他ビットに与える副作用を認識し、DFIM後に運用値へ再設定する実装にした（§5.3）
- [ ] DFIM完了後、[15](15_mram_octal_memory_mapped_write_init.md)の運用設定（Octal DTR等）を再度明示的に適用する実装にした
- [ ] `Interrupt Status Register`bit2（Power On Error）等を監視し、§8のリカバリフローを実装した
- [ ] 125℃1時間以上の温度暴露やリワーク発生時にDFIM再実行が必要という運用ルールを文書化した（§9）
- [ ] DFIM実行の主体（量産テスト工程／初回起動時自動実行）を決定した（§11）

## 13. 参照

- [14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md) —
  MRAM接続可否の検証、信号・電圧
- [15_mram_octal_memory_mapped_write_init.md](15_mram_octal_memory_mapped_write_init.md) —
  通常運用時（毎回の起動）のOctal DTR切替・メモリマッピング書き込み初期化手順
- [16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md) —
  MRAM基礎知識（本ドキュメントで前提とするレジスタ知識）
- EST3000 (Application Note, Everspin Technologies, Rev.2.1, Nov 2024):
  Figure 1 Device Initialization Flow, Figure 2 Device Power On/Reset Flow,
  Figure 3-4 Device Recovery Flow, Figure 5 JESD252 Reset with Signal Sequence,
  §4-8 各種レジスタ, §9 極端温度暴露, §10 OTP, §11 設定保存, §12-13 初期化手順詳細

---

[← README（目次）へ戻る](README.md)
