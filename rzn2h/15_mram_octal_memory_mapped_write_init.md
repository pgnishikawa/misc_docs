# RZ/N2H xSPI1 → MRAM 8D-8D-8D メモリマッピング書き込み 初期化手順（詳細）

[← README（目次）へ戻る](README.md)

出典: RZ/N2Hユーザーズマニュアル R01UH1039JJ0130
「6. リセット」「9. 低消費電力機能」「11. レジスタライトプロテクション機能」
「13. 内部バス」「17. I/O ポート」「37. 拡張シリアルペリフェラルインタフェース (xSPI)」。
MRAM側は `MRAM/EMxxxLX_B_HR Datasheet v3.7_0.pdf`（以下「MRAM DS」）。
前提として [14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md)
（互換性の結論・レジスタ値の根拠）と
[16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md)
（MRAMの基礎知識）を先に参照してください。本ドキュメントは「では実際にどのレジスタに
どの値をどの順番で書けばよいか」をビットレベルまで具体化したものです。

## 0. 対象・前提

- 接続先: RZ/N2H **xSPI1（ユニット m=1）の CS0** に MRAM EM064LXOAB320IS2T を1個接続
  （xSPI1ベースアドレス `0x801C_0000 + 0x1000×1 = 0x801D_0000`）
- 目標: **メモリマッピングモード**で 8D-8D-8D（Octal DTR）による**読み出しと書き込み**を
  行えるようにする
- MRAMは電源投入直後、既定では **SPI (1S-1S-1S) with DS** モードで起動する
  （Nonvolatile Configuration Register 0 既定値 `0xFF`）ことを前提とする
  （[14](14_mram_em064lx_xspi1_octal_connection.md) §5 方式(a)：起動のたびに
  Volatile Configuration Register でOctal DTRへ切り替える運用）。
  量産時に方式(b)（工場出荷時にNonvolatile Configuration Registerへ書き込み、
  MRAMが最初からOctal DTRで起動する）を採用する場合は、**§5・§6（1S-1S-1Sでの
  WREN／VCR0書き込み）が不要**になり、§1手順の④⑤を省略してリセット直後から
  §7以降だけを行えばよい（[14](14_mram_em064lx_xspi1_octal_connection.md) §5参照）。
- CS1（2個目のスレーブ）は使用しない前提。使う場合は本ドキュメントの`CS0`関連箇所を
  `CS1`に読み替え、`BMCTL0.CS1ACC`等の対応ビットを使うこと。

> **本ドキュメントの範囲外（別ドキュメント参照）**: 以下は
> [17_mram_est3000_factory_initialization.md](17_mram_est3000_factory_initialization.md)
> を参照すること。
> - MRAMは**工場出荷後（リフロー半田付け後）の初回は内部状態が未定義**であり、
>   本ドキュメントの手順を実行する前に**1回だけ**「DFIM（Device Factory
>   Initialization Mode）」による全域初期化が必要（量産テスト工程等での実施を想定）
> - 電源投入からMRAMへ最初のコマンドを発行するまでに**`tPU=350µs`**、
>   リセット発行からは**`200ns`**のウェイトが必要（本ドキュメントには含めていない）

## 1. 全体シーケンス概要

電源投入（またはRZ/N2Hのシステムリセット解除）後、1回だけ実行する手順：

1. **端子設定**（§2）: `XSPI1_*`信号を汎用GPIOから周辺機能へ切り替える
2. **xSPI1モジュールの有効化**（§3）: モジュールリセット解除 → モジュールストップ解除 →
   スレーブバスストップ解除（REQ/ACKハンドシェイク）
3. **xSPI1 CS0を1S-1S-1Sで初期設定**（§4）: `LIOCFGCS0.PRTMD = 0x000`
4. **MRAMへWrite Enable発行**（§5）: マニュアルコマンドで `0x06`
5. **MRAMのVolatile Configuration Register 0書き込み**（§6）: マニュアルコマンドで
   `0x81` + アドレス`0x000000` + データ`0xE7`（Octal DTR with DS へ切り替え指示）
6. **★同期ポイント★ 通信停止フロー確認 → RZ/N2H側をOctal DTRへ切替**（§7）:
   `LIOCFGCS0.PRTMD = 0x3FF`、`CMCFG0/1/2CS0` を設定
7. **メモリマッピングアクセスの有効化確認**（§8）: `BMCTL0.CS0ACC`
8. **（推奨）Octalモードでの状態再確認**（§9）: WELが立ったままであることの確認
9. **動作検証**（§10）: テストライト・リードバック

以降、AXI/AHBバスへの通常の読み書き（`*(volatile uint8_t*)addr = val;` 等）が、
そのままxSPI1経由でMRAMへの8D-8D-8Dトランザクションに変換される。

## 2. 端子設定（PMCm / PFCm）

### 2.1 端子とポートの対応

RZ/N2Hマニュアル表17.6・17.7（ポート01・02機能割り当て）で確認した対応：

| 信号 | ポート | PFC機能コード |
|---|---|---|
| `XSPI1_CKP` | P01_0 | `0x1C`（xSPI） |
| `XSPI1_CS0#` | P01_1 | `0x1C` |
| `XSPI1_CS1#` | P01_2 | `0x1C`（CS1未使用ならGPIOのままでも可） |
| `XSPI1_DS` | P01_3 | `0x1C` |
| `XSPI1_IO0` | P01_4 | `0x1C` |
| `XSPI1_IO1` | P01_5 | `0x1C` |
| `XSPI1_IO2` | P01_6 | `0x1C` |
| `XSPI1_IO3` | P01_7 | `0x1C` |
| `XSPI1_IO4` | P02_0 | `0x1C` |
| `XSPI1_IO5` | P02_1 | `0x1C` |
| `XSPI1_IO6` | P02_2 | `0x1C` |
| `XSPI1_IO7` | P02_3 | `0x1C` |

いずれも電圧ドメインは `VDD1833_5`（表17.6/17.7に明記）で、
[14](14_mram_em064lx_xspi1_octal_connection.md) §6 で確認した `VCC1833_5` = 1.8V設定と一致する。

> **注意（[14](14_mram_em064lx_xspi1_octal_connection.md) §6訂正済み）**: `XSPI1_CKN` /
> `XSPI1_RESET0#` はポート表に**存在しない**（マニュアル表37.2 注1「これらの端子は
> ユニット0でのみ使用可能」）。設定不要であり、そもそも設定先の端子がない。

### 2.2 レジスタ書き込み保護の解除（PRCRS.PRC2）

`PMCm`/`PFCm`（m=00～12、セーフティI/Oポート）は**セーフティ領域アドレス**
`PORT_SRS = 0x812C_0000` からアクセスする限り、`RSELPm`の既定値（`0`=セーフティ領域選択）
のままで読み書き可能（マニュアル17.4.7）。ノンセーフティ側アドレス
（`PORT_SRN = 0x802B_0000`）は既定では読み出し専用なので、**本手順では一貫して
`0x812C_0000`ベースを使う**（RSELPmの変更は不要）。

書き込み保護は `PRCRS.PRC2` で解除する（17.7.1）：

```c
#define PRCRS      (*(volatile uint32_t*)0x81296000u)

#define PORT_SRS_BASE   0x812C0000u
#define PMCm(m)    (*(volatile uint8_t *)(PORT_SRS_BASE + 0x400u + (m)))
#define PFCm(m)    (*(volatile uint64_t*)(PORT_SRS_BASE + 0x600u + 8u*(m)))

/* PRCRS.PRC2 を解除して PMCm/PFCm/RSELPm 等への書き込みを許可 */
static inline void prcrs_unlock_prc2(void)  { PRCRS = 0x0000A504u; }
static inline void prcrs_lock_prc2(void)    { PRCRS = 0x0000A500u; }
```

### 2.3 PFCm / PMCm 設定

`PFCm`は1ポート（8端子分）をまとめた64bitレジスタで、端子nの機能コードは
バイトn（bit `8n+5:8n`）に格納される（17.4.4）。P01・P02とも全対象ビットに
`0x1C`を書けばよい：

```c
void xspi1_pin_init(void)
{
    prcrs_unlock_prc2();

    /* P01_0～P01_7 = XSPI1_CKP, CS0#, CS1#, DS, IO0-3 → 全ビット PFC=0x1C */
    PFCm(1) = 0x1C1C1C1C1C1C1C1Cull;
    PMCm(1) = 0xFFu;                 /* 全端子を周辺機能として使用 */

    /* P02_0～P02_3 = XSPI1_IO4-7 → 下位4端子だけ PFC=0x1C、上位4端子は変更しない */
    uint64_t pfc02 = PFCm(2);
    pfc02 = (pfc02 & ~0x00000000FFFFFFFFull) | 0x1C1C1C1Cull;
    PFCm(2) = pfc02;
    PMCm(2) = (PMCm(2) & 0xF0u) | 0x0Fu;   /* P02_0~3 だけ周辺機能化 */

    prcrs_lock_prc2();
}
```

CS1を使わない場合、`P01_2`（`XSPI1_CS1#`）はGPIOのまま（`PMC`ビットを立てない）でも
xSPI1の動作に支障はない。単純化のため上記コードでは一括してP01全体を周辺機能化しているが、
他用途にP01_2を使う設計であれば個別にビットを制御すること。

## 3. xSPI1モジュールの有効化

RZ/N2Hのモジュール有効化は、RZ/N2Lより1段階多い。**(a) モジュールリセット解除 →
(b) モジュールストップ解除 → (c) スレーブバスストップ解除（REQ/ACKハンドシェイク）**
の3段階が必要（マニュアル13.4.3 スレーブストップ機能、表13.6）。

### 3.1 レジスタ書き込み保護の解除

- `MRCTLA`/`MSTPCRA` は `PRCRN.PRC1` で保護（ノンセーフティ領域）
- `SSTPCR6`（スレーブストップ制御）は `PRCRS.PRC3` で保護（セーフティ領域）

```c
#define PRCRN      (*(volatile uint32_t*)0x80294200u)
/* PRCRS は §2.2 で定義済み */

#define MRCTLA     (*(volatile uint32_t*)0x80280240u)
#define MSTPCRA    (*(volatile uint32_t*)0x80280300u)
#define SSTPCR6    (*(volatile uint32_t*)0x81290208u)

static inline void prcrn_unlock_prc1(void) { PRCRN = 0x0000A502u; }
static inline void prcrn_lock_prc1(void)   { PRCRN = 0x0000A500u; }
static inline void prcrs_unlock_prc3(void) { PRCRS = 0x0000A508u; }
static inline void prcrs_lock_prc3(void)   { PRCRS = 0x0000A500u; }
```

> `PRCRN`/`PRCRS`は`PRKEY[7:0]=0xA5`と対象`PRCi`ビットを**同時に**書き込む必要がある
> （11.3.1/11.3.2）。他のPRCiビットを既に1にしている設計であれば、単純代入ではなく
> 読み出した現在値にビットを追加するread-modify-writeにすること。

### 3.2 モジュールリセット解除・モジュールストップ解除

```c
void xspi1_module_reset_release(void)
{
    prcrn_unlock_prc1();
    MRCTLA  &= ~(1u << 5);   /* MRCTLA05 = 0: xSPIユニット1のリセット状態を解除 */
    MSTPCRA &= ~(1u << 5);   /* MSTPCRA05 = 0: xSPIユニット1のモジュールストップを解除 */
    prcrn_lock_prc1();
}
```

（リセット直後の既定値は両ビットとも`1`＝リセット状態／モジュールストップ状態。
6.3.11 / 9.3.1で確認済み）

### 3.3 スレーブバスストップ解除（REQ/ACKハンドシェイク）

マニュアル13.4.3「モジュールリセットまたはモジュールストップ状態からの解除の場合」の
手順に厳密に従う：

1. （§3.2で実施済み）モジュールリセット／モジュールストップを解除
2. `SSTPCR6.XSPI1_REQ`（bit4）を`0`に設定
3. `SSTPCR6.XSPI1_ACK`（bit5）が`0`になるまでポーリング
4. `ACK=0`確認後、xSPI1へのアクセスが可能になる

```c
void xspi1_slave_stop_release(void)
{
    prcrs_unlock_prc3();
    SSTPCR6 &= ~(1u << 4);              /* XSPI1_REQ = 0 */
    prcrs_lock_prc3();

    while (SSTPCR6 & (1u << 5)) {       /* XSPI1_ACK が 0 になるまで待つ */
        /* 必要であればタイムアウト処理を追加 */
    }
}
```

（リセット直後の既定値は`XSPI1_REQ`=`XSPI1_ACK`=`1`＝バス停止確定状態。13.3.11で確認済み）

> **参考（本ドキュメントでは踏み込まない）**: `SLVACCCTL8.XSPI1_SL[1:0]`
> （アドレス`0x8129_0318`、bit27:26、既定値`00b`）はxSPI1ユニットの
> TrustZoneセキュリティレベルを指定するレジスタ。単一コア・Non-secureのみで
> ブリングアップする構成であれば既定値のままで問題ないが、Cortex-A55の
> Secure/Non-secure分離やCortex-R52とのマスタ権限分離を行う設計では、
> このレベル設定とアクセス元コアのセキュリティ状態が一致している必要がある
> （13.4.4 TrustZoneアクセス制御を参照）。

### 3.4 まとめ関数

```c
void xspi1_enable(void)
{
    xspi1_module_reset_release();
    xspi1_slave_stop_release();
}
```

## 4. xSPI1 CS0 を 1S-1S-1S で初期設定

MRAMは起動直後1S-1S-1Sで待ち受けているため、まずRZ/N2H側もそれに合わせる。

```c
#define XSPI1_BASE   0x801D0000u
#define LIOCFGCS0    (*(volatile uint32_t*)(XSPI1_BASE + 0x050u))
#define CDCTL0       (*(volatile uint32_t*)(XSPI1_BASE + 0x070u))
#define CDTBUF0      (*(volatile uint32_t*)(XSPI1_BASE + 0x080u))
#define CDABUF0      (*(volatile uint32_t*)(XSPI1_BASE + 0x084u))
#define CDD0BUF0     (*(volatile uint32_t*)(XSPI1_BASE + 0x088u))

void xspi1_cs0_set_1s1s1s(void)
{
    LIOCFGCS0 = 0x000u;   /* PRTMD = 1S-1S-1S */
}
```

## 5. マニュアルコマンド: Write Enable (0x06)

マニュアルコマンド手順は「①TRREQ=0確認 → ②CDTBUFn/CDABUFn/CDD0BUFn設定 →
③TRREQ=1で発行 → ④完了待ち」（37.4.7.4）。

`CDTBUFn`のビット構成（37.3.2.7）: `CMD[31:16]`（1S-1S-1Sでは`CMD[15:8]`=オペコード、
`CMD[7:0]`は未使用）、`TRTYPE`(bit15)、`LATE[13:9]`、`DATASIZE[8:5]`、`ADDSIZE[4:2]`、
`CMDSIZE[1:0]`。WRENはコマンドのみ（アドレスなし・データなし）：

| フィールド | 値 | 理由 |
|---|---|---|
| `CMD[15:8]` | `0x06` | Write Enable オペコード（MRAM DS Table 21） |
| `CMDSIZE` | `01b`（1バイト） | 1S-1S-1Sなので1バイトコマンド |
| `ADDSIZE` | `000b`（0バイト） | WRENはアドレスフェーズなし |
| `DATASIZE` | `0x0`（0バイト） | WRENはデータフェーズなし |
| `LATE` | `0` | レイテンシなし |
| `TRTYPE` | `1`（読み出し以外） | 書き込み系コマンド |

→ `CDTBUF0 = 0x06008001`

```c
void mram_wren_1s1s1s(void)
{
    while (CDCTL0 & 0x1u) { /* 前トランザクションの完了待ち（念のため） */ }

    CDTBUF0 = 0x06008001u;   /* CMD=0x06, CMDSIZE=1byte, ADDSIZE=0, DATASIZE=0, TRTYPE=1 */
    CDCTL0  = 0x00000001u;   /* CSSEL=CS0(bit3=0), TRNUM=1コマンド(bit5:4=00), TRREQ=1 */

    while (CDCTL0 & 0x1u) { /* TRREQ が 0 に戻る = トランザクション完了まで待つ */ }
}
```

MRAM DSにより、**この WREN は電源投入後1回発行すればよく、以後の Write
（Program）コマンドではWELが自動クリアされない**（[14](14_mram_em064lx_xspi1_octal_connection.md)
§2・§4、MRAM DS §11）。

## 6. マニュアルコマンド: Volatile Configuration Register 0 書き込み (0x81)

MRAMを **Octal DTR with DS**（`0xE7`）に切り替える。MRAM DS Table 21で
`Write Volatile Configuration Register (81h)` のアドレスバイト数は `3/4`
（現在のアドレスモード次第）。MRAMは既定でNonvolatile Configuration Register 5
＝3バイトアドレスモードのままなので、**ここでは3バイトアドレス**を使う。

| フィールド | 値 | 理由 |
|---|---|---|
| `CMD[15:8]` | `0x81` | Write Volatile Configuration Register |
| `CMDSIZE` | `01b`（1バイト） | |
| `ADDSIZE` | `011b`（3バイト） | MRAM既定のアドレスモード（3バイト）に合わせる |
| `DATASIZE` | `0x1`（1バイト） | VCR0は1バイトレジスタ |
| `LATE` | `0` | Write Volatile Configuration Registerはダミーサイクルなし |
| `TRTYPE` | `1` | 書き込み |

→ `CDTBUF0 = 0x8100802D`、アドレス（VCR0のレジスタアドレス）`CDABUF0 = 0x00000000`、
データ `CDD0BUF0 = 0x000000E7`

```c
void mram_write_vcr0_octal_dtr(void)
{
    while (CDCTL0 & 0x1u) { }

    CDTBUF0  = 0x8100802Du;  /* CMD=0x81, ADDSIZE=3byte, DATASIZE=1byte, CMDSIZE=1byte, TRTYPE=1 */
    CDABUF0  = 0x00000000u;  /* VCR0 のレジスタアドレス = 0x000000 */
    CDD0BUF0 = 0x000000E7u;  /* 0xE7 = Octal DTR with DS */
    CDCTL0   = 0x00000001u;

    while (CDCTL0 & 0x1u) { }
}
```

> データバイトの格納位置（`CDD0BUFn`のどのバイトが実際に送出される最初のバイトか）は、
> レジスタ定義上は単に「書き込みデータ」とだけ記載され明記されていない
> （37.3.2.9）。本ドキュメントでは`ADD`/`CMD`と異なりバイトオーダーの特記がないことから
> 自然な低位バイト（`DATA[7:0]`）が使われる前提で記載している。量産前に実機での
> 動作確認（後述§10のリードバック検証）で必ず裏取りすること。

（任意）書き込み直後に `Read Volatile Configuration Register (0x85)` で読み戻し、
`CDD0BUF0 & 0xFF == 0xE7` を確認すると、この時点での設定ミスを早期に検出できる
（コマンド構成は本章と同様、`TRTYPE=0`にするだけ）。

## 7. ★同期ポイント★ 通信停止フロー確認 → RZ/N2H を Octal DTR へ切替

MRAM DSにより、Volatile Configuration Registerへの変更は
**「WRITE VOLATILE CONFIGURATION REGISTERコマンドの完了直後から即座に有効」**
になる（NVCRと異なり電源再投入は不要）。つまり、§6のコマンドが完了した瞬間から
MRAMは**Octal DTRでの応答しか受け付けない**。したがって、RZ/N2H側のプロトコル設定
切り替えは、**§6の完了直後・かつ他のどんなxSPI1トランザクションよりも前に**
行わなければならない。

### 7.1 通信停止の確認（37.4.7.2）

コンフィグレーションレジスタ（`LIOCFGCSn`/`CMCFGxCSn`等）の再設定前に、
以下2点を確認する：

```c
#define CCCTL0CS0  (*(volatile uint32_t*)(XSPI1_BASE + 0x130u))

/* ①自動キャリブレーション無効を確認（本手順では最初から使っていないので既定値0のはず） */
/* ②マニュアルコマンドの保留なしを確認（§5・§6で毎回ポーリング済みなのでTRREQ=0のはず） */
_Static_assert(1, "CCCTL0CS0.CAEN と CDCTL0.TRREQ が 0 であることを確認してから次へ進む");

if ((CCCTL0CS0 & 0x1u) != 0 || (CDCTL0 & 0x1u) != 0) {
    /* ここに来る場合は実装ミス。先に停止させること */
}
```

### 7.2 LIOCFGCS0 / CMCFG0-2CS0 の切替

[14](14_mram_em064lx_xspi1_octal_connection.md) §3.3 で確定済みの値をそのまま設定する：

```c
#define CMCFG0CS0    (*(volatile uint32_t*)(XSPI1_BASE + 0x010u))
#define CMCFG1CS0    (*(volatile uint32_t*)(XSPI1_BASE + 0x014u))
#define CMCFG2CS0    (*(volatile uint32_t*)(XSPI1_BASE + 0x018u))

void xspi1_cs0_switch_to_octal_dtr(void)
{
    LIOCFGCS0 = 0x3FFu;          /* PRTMD = 8D-8D-8D */

    /* FFMT=01b(プロファイル1.0), ADDSIZE=11b(4byte) ※ビット位置はCMCFG0CSnのFFMT/ADDSIZEフィールドに従う */
    CMCFG0CS0 = 0x0000000Du;     /* 例: FFMT[1:0]=01b, ADDSIZE[1:0]=11b の合成値（実装時に正式なビット定義で再確認） */

    /* RDCMD=0x0C0C（Read Fast、コマンド2バイト repeated）, RDLATE=16 */
    CMCFG1CS0 = (0x0C0Cu << 16) | 16u;

    /* WRCMD=0x1212（Write 4-byte address、コマンド2バイト repeated）, WRLATE=0 */
    CMCFG2CS0 = (0x1212u << 16) | 0u;
}
```

> `CMCFG0CS0`のビット合成値はプレースホルダ。実装時は37.3.1.4（CMCFG0CSn）の
> `FFMT[1:0]`・`ADDSIZE[1:0]`の正確なビット位置をマニュアルで再確認して埋めること
> （本ドキュメントでは既に[14](14_mram_em064lx_xspi1_octal_connection.md) §3.3で
> `FFMT=01b`・`ADDSIZE=11b`という設定値までは確定済みだが、レジスタ内の
> ビット位置はCMCFG1/2CS0ほど厳密に転記できていないため、コーディング時に
> 一次資料で最終確認するのを推奨する）。

## 8. メモリマッピングアクセスの有効化確認

`BMCTL0.CS0ACC[1:0]`（37.3.2.1）は **`00`=読み書き禁止、`01`=読み出しのみ、
`10`=書き込みのみ、`11`=読み書き許可**。リセット後の既定値は**`11b`
（読み書き許可）**なので、通常は何もしなくてもよいが、他のコードパスで
一時的に制限していた場合に備えて明示しておくと安全：

```c
#define BMCTL0   (*(volatile uint32_t*)(XSPI1_BASE + 0x060u))

void xspi1_cs0_enable_memmap_rw(void)
{
    uint32_t v = BMCTL0;
    v = (v & ~0x3u) | 0x3u;   /* CS0ACC[1:0] = 11b: 読み書き許可 */
    BMCTL0 = v;
}
```

## 9. （推奨）Octal DTR モードでの状態再確認

MRAM DSにより、**WELビットはPOR／ソフトウェアリセット／ハードウェアリセットでのみ
クリアされ**、レジスタ書き込み（VCRのI/Oモード切替）では影響を受けない
（MRAM DS §11「The WEL bit is volatile and returns to its default '0' state after
POR, software RESET, and hardware RESET」）。したがって §5 で立てたWELは
§6→§7のプロトコル切替をまたいでもセットされたままのはずである。

初回ブリングアップ時の保険として、Octal DTRへの切替直後に
`Read Status Register (0x05)` をOctalモードのマニュアルコマンドとして発行し、
WEL（bit1）が`1`のままであることを確認しておくと安心である：

| フィールド | 値 | 理由 |
|---|---|---|
| `CMD[15:0]` | `0x0505` | 8D-8D-8Dプロファイル1.0は**オペコード反復必須**（[14](14_mram_em064lx_xspi1_octal_connection.md) §3.1） |
| `CMDSIZE` | `10b`（2バイト、8D-8D-8Dでは固定） | |
| `ADDSIZE` | `000b` | Read Status Registerはアドレスなし |
| `DATASIZE` | `0x1` | 1バイト読み出し要求（物理的には2バイト転送され、2バイト目は無視） |
| `LATE` | `8` | MRAM DS Table 21: OSPI DTRでのRead Status Registerレイテンシ=8サイクル |
| `TRTYPE` | `0`（読み出し） | |

→ `CDTBUF0 = 0x05051022`

```c
int mram_check_wel_octal_dtr(void)
{
    while (CDCTL0 & 0x1u) { }

    CDTBUF0 = 0x05051022u;
    CDCTL0  = 0x00000001u;
    while (CDCTL0 & 0x1u) { }

    uint8_t status = (uint8_t)(CDD0BUF0 & 0xFFu);
    return (status & 0x02u) != 0;   /* bit1 = WEL */
}
```

`0`が返った場合は何らかの理由でWELが失われているので、Octalモードのまま
`0x06`（WREN、CMDSIZE=10b・CMD=0x0606で発行）を再送すればよい。

## 10. 動作検証

1. メモリマッピング領域（xSPI1 CS0、[03_memory_map.md](03_memory_map.md)参照）への
   ポインタ経由の書き込みで既知パターンを書く：
   ```c
   volatile uint32_t *mram = (volatile uint32_t*)0x0000000050000000ull; /* 例: 実アドレスは03のマップに従う */
   mram[0] = 0xDEADBEEFu;
   ```
2. 別のメモリマッピング読み出し、または`Read Fast Octal I/O (0xCC)`のマニュアル
   コマンドで読み戻し、`0xDEADBEEF`と一致することを確認する。
3. 必要であれば書き込み直後に `Read Status Register` のWIPビット（bit0）が
   `0`（ビジーでない）に戻っていることを確認する
   （[14](14_mram_em064lx_xspi1_octal_connection.md) §4・§7で述べた通り、
   正式なtPP値はデータシートに記載がないため、量産前にこの検証を必ず行うこと）。
4. 複数アドレス・複数回の書き込みを行い、**2回目以降のWriteの前にWRENを
   再送していなくても**正常に書き込めることを確認する
   （このMRAM特有のWEL維持動作の実機裏取り）。

## 11. レジスタ設定一覧（まとめ）

| レジスタ | アドレス | 設定値 | タイミング |
|---|---|---|---|
| `PMCm(1)` | `0x812C_0401` | `0xFF` | §2（端子設定） |
| `PFCm(1)` | `0x812C_0608` | `0x1C1C1C1C1C1C1C1C` | §2 |
| `PMCm(2)` | `0x812C_0402` | 下位4bit=`0xF`（他は維持） | §2 |
| `PFCm(2)` | `0x812C_0610` | 下位4バイト=`0x1C1C1C1C`（他は維持） | §2 |
| `MRCTLA` | `0x8028_0240` | bit5=0 | §3.2 |
| `MSTPCRA` | `0x8028_0300` | bit5=0 | §3.2 |
| `SSTPCR6` | `0x8129_0208` | bit4(REQ)=0 → bit5(ACK)=0待ち | §3.3 |
| `LIOCFGCS0` | `0x801D_0050` | `0x000`→(§7で)`0x3FF` | §4, §7 |
| `CDCTL0`/`CDTBUF0`/`CDABUF0`/`CDD0BUF0` | `0x801D_0070`/`0x080`/`0x084`/`0x088` | §5・§6・§9参照 | §5, §6, §9 |
| `CMCFG0CS0` | `0x801D_0010` | `FFMT=01b, ADDSIZE=11b` | §7 |
| `CMCFG1CS0` | `0x801D_0014` | `RDCMD=0x0C0C, RDLATE=16` | §7 |
| `CMCFG2CS0` | `0x801D_0018` | `WRCMD=0x1212, WRLATE=0` | §7 |
| `BMCTL0` | `0x801D_0060` | `CS0ACC=11b`（既定値のまま） | §8 |

## 12. まとめチェックリスト

- [ ] `PMCm`/`PFCm`（P01全体、P02下位4bit）を周辺機能（`0x1C`）に設定した（`PRCRS.PRC2`解除／再施錠を含む）
- [ ] `MRCTLA05`/`MSTPCRA05`を`0`にしてxSPIユニット1のリセット・モジュールストップを解除した（`PRCRN.PRC1`解除／再施錠を含む）
- [ ] `SSTPCR6.XSPI1_REQ=0`を書き込み、`XSPI1_ACK=0`になるまでポーリングした（`PRCRS.PRC3`解除／再施錠を含む）
- [ ] `LIOCFGCS0=0x000`（1S-1S-1S）でマニュアルコマンドモードから開始した
- [ ] マニュアルコマンドで`0x06`（WREN）を発行した
- [ ] マニュアルコマンドで`0x81`（Write VCR0）+アドレス`0x000000`+データ`0xE7`を発行した
- [ ] VCR0書き込み完了の**直後**に、他のトランザクションを挟まず`LIOCFGCS0=0x3FF`・`CMCFG0/1/2CS0`を設定した
- [ ] `BMCTL0.CS0ACC=11b`（読み書き許可）を確認した
- [ ] （推奨）Octal DTRモードで`0x0505`によりWELビットが立ったままであることを確認した
- [ ] メモリマッピング書き込み→読み戻しのテストで実機検証した
- [ ] WEL再送なしでの連続書き込みが実機で正常動作することを確認した

## 13. 参照

- [14_mram_em064lx_xspi1_octal_connection.md](14_mram_em064lx_xspi1_octal_connection.md) —
  互換性の結論、レジスタ設定値の根拠、信号・電圧、要検証事項
- [16_mram_fundamentals_for_software_engineers.md](16_mram_fundamentals_for_software_engineers.md) —
  MRAMの基礎知識まとめ
- [17_mram_est3000_factory_initialization.md](17_mram_est3000_factory_initialization.md) —
  工場出荷後に1回だけ必要な初回初期化(DFIM)手順、および本ドキュメントに含めていなかった
  電源投入タイミング要件（`tPU=350µs`等）
- [13_xspi_protocol_modes_and_quad_flash.md](13_xspi_protocol_modes_and_quad_flash.md) —
  プロトコルモード一覧、通信停止手順の位置づけ
- マニュアル 6章（リセット, MRCTLA）、9章（低消費電力機能, MSTPCRA）、
  11章（レジスタライトプロテクション, PRCRN/PRCRS）、13章（内部バス, スレーブストップ機能,
  SSTPCR6/SLVACCCTL8）、17章（I/Oポート, PMCm/PFCm/RSELPm）、37章（xSPI, 全レジスタ・動作フロー）
- MRAM DS: §5.5/5.6（Configuration Registers）、§8 Table 21（コマンド一覧・レイテンシ）、
  §11 Write Enable Latch（WELの挙動）

---

[← README（目次）へ戻る](README.md)
