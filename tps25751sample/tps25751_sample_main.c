/*
 * tps25751_sample_main.c
 *
 * RZ/N2H + FSP で TPS25751 の初期化 (パッチバンドル・ロード) を行うサンプル.
 *
 * 前提:
 *   - RZ Smart Configurator で r_iic_master を channel 1 (I2C1) として追加し,
 *     g_i2c1_ctrl (iic_master_instance_ctrl_t) と g_i2c1_cfg (i2c_master_cfg_t)
 *     が生成済み  (詳細は tps25751_i2c1_cfg.c のコメント)
 *   - I2C1 の SCL/SDA ピンを割当て済み, 外部プルアップあり
 *   - TPS25751 は EEPROM 非搭載 (SDA 切断) 構成で, コールドブート後 PTCH モードで待機
 *   - TPS25751 のレジスタアクセス用アドレス = 0x22, PBM バーストアドレス = 0x30
 *
 * この関数を hal_entry() などから呼び出してください.
 */

#include "hal_data.h"                 /* g_i2c1_ctrl / g_i2c1_cfg (Smart Configurator 生成) */
#include "tps25751.h"
#include "tps25751_patch_bundle.h"

/* ドライバコンテキスト (静的確保: 割り込みコールバックから参照するため) */
static tps25751_ctx_t g_tps25751;

/* 結果を LED / デバッガ等で確認できるよう保持 */
volatile tps25751_result_t        g_tps25751_last_result;
volatile tps25751_patch_status_t  g_tps25751_last_status;
volatile fsp_err_t                g_tps25751_last_fsp_err;

void tps25751_sample_run(void);

void tps25751_sample_run(void)
{
    fsp_err_t err;

    /* 1. ドライバを open し, R_IIC_MASTER_Open() で I2C1 を初期化 */
    err = tps25751_open(&g_tps25751,
                        &g_i2c1_ctrl,
                        &g_i2c1_cfg,
                        g_tps25751_patch_bundle,
                        TPS25751_PATCH_BUNDLE_SIZE);
    g_tps25751_last_fsp_err = err;
    if (FSP_SUCCESS != err)
    {
        /* I2C の open に失敗 (ピン未設定 / チャネル無効 / 割り込み未設定 等) */
        __BKPT(0);
        return;
    }

    /* 2. パッチバンドルをロードし PTCH -> APP へ遷移させる */
    tps25751_patch_status_t status;
    tps25751_result_t       res = tps25751_load_patch_bundle(&g_tps25751, &status);

    g_tps25751_last_result = res;
    g_tps25751_last_status = status;

    if (TPS25751_OK != res)
    {
        /* 失敗時: res と status の各フィールドを確認する.
         *   res == TPS25751_ERR_NOT_PTCH      : 既に APP か, PTCH に入っていない
         *   res == TPS25751_ERR_PATCH_START   : status.patch_start_status
         *                                       (0x04=サイズ 0x05=アドレス 0x06=タイムアウト)
         *   res == TPS25751_ERR_PATCH_COMPLETE: status.device_patch_complete /
         *                                       status.appcfg_patch_complete
         *   res == TPS25751_ERR_*_TIMEOUT     : CMD1 が完了しない (配線/クロック/バンドル)
         */
        __BKPT(0);
        return;
    }

    /* 3. 成功. TPS25751 は APP モードで動作中 (status.mode = "APP ").
     *    以降は 0x22 宛てに通常の PD レジスタ (0x14 INT_EVENT1, 0x1A STATUS, ...) を
     *    アクセスできる. */
}
