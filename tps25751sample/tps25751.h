/*
 * tps25751.h
 *
 * RZ/N2H + Renesas FSP を用いた TI TPS25751 (USB-C PD コントローラ) の
 * パッチバンドル・ロード用サンプルドライバ.
 *
 * I2C は FSP の IIC マスタドライバ (r_iic_master) を
 *   rz-fsp/rzn/rz/fsp/inc/instances/r_iic_master.h
 * の API (R_IIC_MASTER_Open / R_IIC_MASTER_Write / R_IIC_MASTER_Read /
 *         R_IIC_MASTER_SlaveAddressSet / R_IIC_MASTER_CallbackSet /
 *         R_IIC_MASTER_Close) を直接呼び出して使用する.
 *
 * 参考資料:
 *   - TPS25751 Technical Reference Manual (SLVUCR8B)  ... 4.5 節 / 5.2 節
 *   - Application Note JAJA940A (English: SLVAFV8)
 *       "組み込みコントローラ(EC)を使用して TPS25751/TPS26750 にパッチバンドルを直接読み込む"
 *
 * 動作概要 (I2Ct 経由, レジスタ・ポーリング方式, ベアメタル):
 *   1. MODE レジスタ(0x03) が 'PTCH' であることを確認
 *   2. DATA1(0x09) に PBMs パラメータ(バンドルサイズ / バーストアドレス / タイムアウト)を書き込み
 *   3. CMD1(0x08) に 4CC "PBMs" を書き込み, CMD1 が 0 に戻るまでポーリング
 *   4. DATA1(0x09) の PatchStartStatus == 0 を確認
 *   5. バーストターゲットアドレス(既定 0x30) にパッチバンドル本体を分割書き込み
 *   6. CMD1(0x08) に 4CC "PBMc" を書き込み, CMD1 が 0 に戻るまでポーリング
 *   7. 20ms 待機後 DATA1(0x09) の完了ステータスを確認
 *   8. MODE レジスタ(0x03) が 'APP ' になるまでポーリング
 */

#ifndef TPS25751_H
#define TPS25751_H

#include <stdint.h>
#include <stdbool.h>

/* FSP IIC マスタドライバ (インスタンスヘッダ) を直接使用する */
#include "r_iic_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*
 * I2C アドレス
 *---------------------------------------------------------------------------*/

/* レジスタアクセス用アドレス (Unique / Fundamental Address). 本サンプルの指定値. */
#define TPS25751_ADDR_REGISTER      (0x22U)

/* パッチバーストモードのデータ書き込み先アドレス.
 * ハード固定値ではなく, ホストが 'PBMs' の DATA1 入力 Byte5 で PD に通知する値.
 * JAJA940A / SLVUCR8B の作例と同じ 0x30 を使用する.
 * 選べない値 (SLVUCR8B Table 4-15 / データシート表8-5):
 *   - 0x00
 *   - ADCINx で選択される I2Ct ポートアドレス (本製品では 7bit 0x20-0x23)
 *   - I2C 予約アドレス (0x00-0x07, 0x78-0x7F)
 *   - 同一バス上の他デバイスのアドレス, および 0x22 自身 */
#define TPS25751_ADDR_PBM_BURST     (0x30U)

/*---------------------------------------------------------------------------*
 * レジスタ番号 (PTCH モードでアクセス可能なもの)
 *---------------------------------------------------------------------------*/
#define TPS25751_REG_MODE           (0x03U)   /* 4 バイト ASCII: "BOOT"/"PTCH"/"APP " */
#define TPS25751_REG_CMD1           (0x08U)   /* 4CC コマンド書き込み / 実行結果 */
#define TPS25751_REG_DATA1          (0x09U)   /* CMD1 の入出力データ (最大 64 バイト) */
#define TPS25751_REG_INT_EVENT1     (0x14U)   /* 割り込みイベント (11 バイト) */
#define TPS25751_REG_INT_MASK1      (0x16U)   /* 割り込みマスク (11 バイト) */
#define TPS25751_REG_INT_CLEAR1     (0x18U)   /* 割り込みクリア (11 バイト) */

/*---------------------------------------------------------------------------*
 * PBMs (Start Patch Burst Mode) パラメータ  ... SLVUCR8B Table 4-15
 *---------------------------------------------------------------------------*/
/* Byte 6: バーストモードタイムアウト. 0x32 = 5 秒 (100ms 単位). 非ゼロ必須. */
#define TPS25751_PBM_TIMEOUT_100MS  (0x32U)

/*---------------------------------------------------------------------------*
 * バースト書き込みの 1 トランザクション最大バイト数.
 * SLVUCR8B: PD コントローラは受信バイトごとにポインタを自動インクリメントし,
 * I2C の START/STOP ではリセットされないため, 分割送信して問題ない.
 * 値はホスト側の都合で自由に決められる (大きいほど転送効率は良い).
 *---------------------------------------------------------------------------*/
#ifndef TPS25751_BURST_CHUNK_SIZE
 #define TPS25751_BURST_CHUNK_SIZE  (1024U)
#endif

/*---------------------------------------------------------------------------*
 * ポーリング / タイムアウト調整用
 *---------------------------------------------------------------------------*/
#ifndef TPS25751_MODE_POLL_MAX       /* MODE レジスタのポーリング回数 (10ms 間隔) */
 #define TPS25751_MODE_POLL_MAX     (200U)    /* = 最大 2 秒 */
#endif
#ifndef TPS25751_CMD_POLL_MAX        /* CMD1 完了待ちのポーリング回数 (10ms 間隔) */
 #define TPS25751_CMD_POLL_MAX      (600U)    /* = 最大 6 秒 (PBM タイムアウトより長め) */
#endif
#ifndef TPS25751_I2C_WAIT_LOOP_MAX   /* 1 回の I2C 転送完了待ちのビジーループ上限 */
 #define TPS25751_I2C_WAIT_LOOP_MAX (5000000UL)
#endif

/*---------------------------------------------------------------------------*
 * 結果コード
 *---------------------------------------------------------------------------*/
typedef enum e_tps25751_result
{
    TPS25751_OK = 0,
    TPS25751_ERR_I2C,               /* I2C 転送そのものが失敗 (NACK / アービトレーションロス等) */
    TPS25751_ERR_NOT_PTCH,          /* 開始時に MODE が 'PTCH' でない */
    TPS25751_ERR_DATA1_VERIFY,      /* DATA1 書き戻し確認に失敗 */
    TPS25751_ERR_PBMS_REJECTED,     /* CMD1 が "!CMD" を返した (PBMs 拒否) */
    TPS25751_ERR_PBMS_TIMEOUT,      /* CMD1 が 0 に戻らない (PBMs) */
    TPS25751_ERR_PATCH_START,       /* PatchStartStatus != 0 (サイズ/アドレス/タイムアウト不正) */
    TPS25751_ERR_BURST,             /* バーストデータ書き込みに失敗 */
    TPS25751_ERR_PBMC_REJECTED,     /* CMD1 が "!CMD" を返した (PBMc 拒否) */
    TPS25751_ERR_PBMC_TIMEOUT,      /* CMD1 が 0 に戻らない (PBMc) */
    TPS25751_ERR_PATCH_COMPLETE,    /* PBMc 完了ステータスが失敗を示す */
    TPS25751_ERR_NOT_APP,           /* 最終的に MODE が 'APP ' にならない */
} tps25751_result_t;

/*---------------------------------------------------------------------------*
 * PBMc 完了診断情報  ... SLVUCR8B Table 4-16 (OUTPUT DATAX)
 *---------------------------------------------------------------------------*/
typedef struct st_tps25751_patch_status
{
    uint8_t patch_start_status;        /* PBMs: DATAX Byte1 (0=成功) */
    uint8_t device_patch_complete;     /* PBMc: DATAX bits 23:16 (0x00=成功) */
    uint8_t appcfg_patch_complete;     /* PBMc: DATAX bits 31:24 (0x00=成功,0x40=警告,0x80=失敗) */
    uint8_t rp_return_indicator;       /* PBMc: DATAX Byte1 上位ニブル (0=成功,4=警告,8=エラー) */
    uint8_t ac_return_indicator;       /* PBMc: DATAX Byte1 下位ニブル */
    uint8_t mode[4];                   /* 最終的に読み出した MODE レジスタ (ASCII) */
} tps25751_patch_status_t;

/*---------------------------------------------------------------------------*
 * ドライバコンテキスト
 *---------------------------------------------------------------------------*/
typedef struct st_tps25751_ctx
{
    /* FSP IIC マスタの制御ブロックと構成.
     * RZ Smart Configurator で r_iic_master を channel 1 (RZ/N2H の I2C1) として
     * 追加すると g_i2c1_ctrl (iic_master_instance_ctrl_t) と
     * g_i2c1_cfg (i2c_master_cfg_t) が生成される. それらを渡す. */
    iic_master_instance_ctrl_t * p_i2c_ctrl;
    i2c_master_cfg_t const     * p_i2c_cfg;

    /* ロードするパッチバンドル (Low Region Binary).
     * TI "USB-C PD アプリケーションカスタマイズツール" の C ファイル出力を使う.
     * codes/tps25751_patch_bundle.c を実データで置き換えること. */
    uint8_t const * p_bundle;
    uint32_t        bundle_size;      /* バイト数 (= SIZEOFLRB) */

    uint8_t reg_addr;                 /* レジスタアクセス用アドレス (通常 TPS25751_ADDR_REGISTER) */
    uint8_t burst_addr;              /* バースト書き込み先 (通常 TPS25751_ADDR_PBM_BURST) */

    /* --- 内部状態 (呼び出し側は触らない) --- */
    volatile bool               i2c_done;
    volatile i2c_master_event_t i2c_event;
} tps25751_ctx_t;

/*---------------------------------------------------------------------------*
 * API
 *---------------------------------------------------------------------------*/

/*
 * コンテキストを初期化し, R_IIC_MASTER_Open() で I2C1 を初期化して
 * R_IIC_MASTER_CallbackSet() で本ドライバのコールバックを登録する.
 *
 * p_ctx        : 呼び出し側で確保したコンテキスト (静的確保推奨)
 * p_i2c_ctrl   : FSP IIC マスタ制御ブロック   (例: &g_i2c1_ctrl)
 * p_i2c_cfg    : FSP IIC マスタ構成           (例: &g_i2c1_cfg)
 * p_bundle     : パッチバンドル先頭ポインタ
 * bundle_size  : パッチバンドルのバイト数
 *
 * 戻り値: FSP_SUCCESS / FSP エラーコード
 */
fsp_err_t tps25751_open(tps25751_ctx_t             * p_ctx,
                        iic_master_instance_ctrl_t * p_i2c_ctrl,
                        i2c_master_cfg_t const     * p_i2c_cfg,
                        uint8_t const              * p_bundle,
                        uint32_t                     bundle_size);

/*
 * R_IIC_MASTER_Close() を呼び出す.
 */
fsp_err_t tps25751_close(tps25751_ctx_t * p_ctx);

/*
 * パッチバンドルを I2Ct 経由でロードし, PTCH -> APP へ遷移させる.
 *
 * p_ctx     : tps25751_open() 済みのコンテキスト
 * p_status  : NULL 可. 非 NULL の場合, 完了診断情報を格納する.
 *
 * 戻り値: tps25751_result_t
 */
tps25751_result_t tps25751_load_patch_bundle(tps25751_ctx_t          * p_ctx,
                                             tps25751_patch_status_t * p_status);

/*
 * MODE レジスタを読み出す (4 バイト ASCII を dst[0..3] へ).
 * デバッグ用途に公開.
 */
tps25751_result_t tps25751_read_mode(tps25751_ctx_t * p_ctx, uint8_t dst[4]);

#ifdef __cplusplus
}
#endif

#endif /* TPS25751_H */
