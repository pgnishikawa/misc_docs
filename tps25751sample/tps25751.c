/*
 * tps25751.c
 *
 * TPS25751 パッチバンドル・ロード実装
 * (RZ/N2H + FSP r_iic_master を直接呼び出し, レジスタポーリング方式).
 * 詳細な手順とレジスタ定義は tps25751.h のヘッダコメントを参照.
 *
 * 使用する FSP API (r_iic_master.h):
 *   R_IIC_MASTER_Open / R_IIC_MASTER_Close
 *   R_IIC_MASTER_Write / R_IIC_MASTER_Read
 *   R_IIC_MASTER_SlaveAddressSet
 *   R_IIC_MASTER_CallbackSet
 *
 * I2Ct のレジスタアクセスプロトコル (SLVUCR8B Figure 1-2 / 1-3):
 *   書き込み: S [addr|W] [RegNum] [ByteCount=N] [D0]..[DN-1] P
 *   読み出し: S [addr|W] [RegNum] Sr [addr|R] [ByteCount=N] [D0]..[DN-1] P
 *             -> 読み出しの先頭 1 バイトは "バイト数". データはその次から.
 *   バースト: S [burst_addr|W] [D0]..[DM-1] P   (RegNum/ByteCount なし, 生データのみ)
 */

#include <string.h>

#include "tps25751.h"
#include "bsp_api.h"            /* R_BSP_SoftwareDelay, BSP_DELAY_UNITS_* */

/*---------------------------------------------------------------------------*
 * ローカル定数
 *---------------------------------------------------------------------------*/
#define TPS25751_4CC_LEN            (4U)

/* 4CC コマンド (ASCII, リトルエンド順にそのまま送出) */
static const uint8_t TPS25751_CMD_PBMS[TPS25751_4CC_LEN] = { 'P', 'B', 'M', 's' };
static const uint8_t TPS25751_CMD_PBMC[TPS25751_4CC_LEN] = { 'P', 'B', 'M', 'c' };

/* MODE レジスタの期待値 */
static const uint8_t TPS25751_MODE_PTCH[4] = { 'P', 'T', 'C', 'H' };
static const uint8_t TPS25751_MODE_APP[4]  = { 'A', 'P', 'P', ' ' };

/* CMD1 レジスタ先頭バイトの意味 (SLVUCR8B / JAJA940A 脚注4)
 *   0x00 : コマンド正常完了
 *   0x21 : '!' -> "!CMD" コマンド拒否 (無効なコマンド)                      */
#define TPS25751_CMD1_DONE         (0x00U)
#define TPS25751_CMD1_REJECT       (0x21U)   /* '!' */

/* I2C 転送に使うワークバッファ最大長 (DATA1 読み出し 64B + バイト数 1B に余裕) */
#define TPS25751_IO_BUF_LEN        (72U)

/*---------------------------------------------------------------------------*
 * I2C 完了コールバック
 *---------------------------------------------------------------------------*/
static void tps25751_i2c_callback(i2c_master_callback_args_t * p_args)
{
    tps25751_ctx_t * p_ctx = (tps25751_ctx_t *) p_args->p_context;

    if (NULL != p_ctx)
    {
        p_ctx->i2c_event = p_args->event;
        p_ctx->i2c_done  = true;
    }
}

/* 直近に発行した I2C 転送の完了をポーリングで待つ (ベアメタル). */
static tps25751_result_t tps25751_i2c_wait(tps25751_ctx_t * p_ctx)
{
    uint32_t guard = TPS25751_I2C_WAIT_LOOP_MAX;

    while (!p_ctx->i2c_done)
    {
        if (0U == --guard)
        {
            return TPS25751_ERR_I2C;
        }
    }

    if (I2C_MASTER_EVENT_ABORTED == p_ctx->i2c_event)
    {
        return TPS25751_ERR_I2C;
    }

    return TPS25751_OK;
}

/*---------------------------------------------------------------------------*
 * 低レベル I2C ヘルパ (R_IIC_MASTER_* を直接呼ぶ)
 *---------------------------------------------------------------------------*/

/* 現在の I2C ターゲットアドレスを設定する. */
static tps25751_result_t tps25751_set_addr(tps25751_ctx_t * p_ctx, uint8_t addr)
{
    fsp_err_t err = R_IIC_MASTER_SlaveAddressSet(p_ctx->p_i2c_ctrl,
                                                 addr,
                                                 I2C_MASTER_ADDR_MODE_7BIT);
    return (FSP_SUCCESS == err) ? TPS25751_OK : TPS25751_ERR_I2C;
}

/*
 * レジスタブロック書き込み:  S [reg|W] [reg_num] [len] [data..] P
 *   reg_num : レジスタ番号
 *   p_data  : データ本体 (len バイト)
 */
static tps25751_result_t tps25751_reg_write(tps25751_ctx_t * p_ctx,
                                            uint8_t          reg_num,
                                            uint8_t const  * p_data,
                                            uint8_t          len)
{
    uint8_t buf[TPS25751_IO_BUF_LEN];

    if ((uint32_t) len + 2U > sizeof(buf))
    {
        return TPS25751_ERR_I2C;
    }

    buf[0] = reg_num;
    buf[1] = len;
    if (len > 0U)
    {
        memcpy(&buf[2], p_data, len);
    }

    p_ctx->i2c_done = false;
    fsp_err_t err = R_IIC_MASTER_Write(p_ctx->p_i2c_ctrl,
                                       buf,
                                       (uint32_t) len + 2U,
                                       false /* 転送後 STOP */);
    if (FSP_SUCCESS != err)
    {
        return TPS25751_ERR_I2C;
    }

    return tps25751_i2c_wait(p_ctx);
}

/*
 * レジスタブロック読み出し:  S [reg|W] [reg_num] Sr [reg|R] [len] [data..] P
 *   p_data / len : 呼び出し側が欲しいデータバイト数. 先頭のバイト数フィールドは
 *                  本関数内で読み飛ばし, データ本体のみを p_data へ格納する.
 */
static tps25751_result_t tps25751_reg_read(tps25751_ctx_t * p_ctx,
                                           uint8_t          reg_num,
                                           uint8_t        * p_data,
                                           uint8_t          len)
{
    uint8_t   buf[TPS25751_IO_BUF_LEN];
    fsp_err_t err;

    if ((uint32_t) len + 1U > sizeof(buf))
    {
        return TPS25751_ERR_I2C;
    }

    /* 1) レジスタ番号を書き込み (STOP せず repeated START に備える) */
    p_ctx->i2c_done = false;
    err = R_IIC_MASTER_Write(p_ctx->p_i2c_ctrl, &reg_num, 1U, true /* restart */);
    if (FSP_SUCCESS != err)
    {
        return TPS25751_ERR_I2C;
    }
    tps25751_result_t res = tps25751_i2c_wait(p_ctx);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /* 2) [バイト数][データ..] を読み出し */
    p_ctx->i2c_done = false;
    err = R_IIC_MASTER_Read(p_ctx->p_i2c_ctrl, buf, (uint32_t) len + 1U, false /* STOP */);
    if (FSP_SUCCESS != err)
    {
        return TPS25751_ERR_I2C;
    }
    res = tps25751_i2c_wait(p_ctx);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /* buf[0] = デバイスが返したバイト数. データ本体は buf[1] 以降. */
    memcpy(p_data, &buf[1], len);
    return TPS25751_OK;
}

/*
 * バーストデータ書き込み: S [burst_addr|W] [raw data..] P
 * ターゲットアドレスは呼び出し前に tps25751_set_addr() で burst_addr に切替済みであること.
 */
static tps25751_result_t tps25751_burst_write(tps25751_ctx_t * p_ctx,
                                              uint8_t const  * p_data,
                                              uint32_t         len)
{
    p_ctx->i2c_done = false;
    fsp_err_t err = R_IIC_MASTER_Write(p_ctx->p_i2c_ctrl,
                                       (uint8_t *) p_data,
                                       len,
                                       false /* STOP */);
    if (FSP_SUCCESS != err)
    {
        return TPS25751_ERR_BURST;
    }

    return (TPS25751_OK == tps25751_i2c_wait(p_ctx)) ? TPS25751_OK : TPS25751_ERR_BURST;
}

/*---------------------------------------------------------------------------*
 * 中位ヘルパ
 *---------------------------------------------------------------------------*/

static void tps25751_delay_us(uint32_t us)
{
    R_BSP_SoftwareDelay(us, BSP_DELAY_UNITS_MICROSECONDS);
}

static void tps25751_delay_ms(uint32_t ms)
{
    R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
}

/* INT_EVENT1 の滞留分をクリアする (JAJA940A ステップ 3/11 相当). */
static tps25751_result_t tps25751_clear_interrupts(tps25751_ctx_t * p_ctx)
{
    static const uint8_t all_ones[11] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    return tps25751_reg_write(p_ctx, TPS25751_REG_INT_CLEAR1, all_ones, sizeof(all_ones));
}

/*
 * CMD1 に 4CC を書き込み, CMD1 先頭バイトが 0x00 (完了) になるまでポーリングする.
 * 割り込みラインは使わず MODE/CMD1 レジスタのみで完了判定する (SLVUCR8B Figure 5-1).
 *
 * reject_code / timeout_code : エラー時に返す結果コード
 */
static tps25751_result_t tps25751_run_4cc(tps25751_ctx_t   * p_ctx,
                                          uint8_t const    * p_cmd4,
                                          tps25751_result_t  reject_code,
                                          tps25751_result_t  timeout_code)
{
    tps25751_result_t res = tps25751_reg_write(p_ctx, TPS25751_REG_CMD1, p_cmd4, TPS25751_4CC_LEN);
    if (TPS25751_OK != res)
    {
        return res;
    }

    for (uint32_t i = 0U; i < TPS25751_CMD_POLL_MAX; i++)
    {
        uint8_t cmd[TPS25751_4CC_LEN];

        res = tps25751_reg_read(p_ctx, TPS25751_REG_CMD1, cmd, TPS25751_4CC_LEN);
        if (TPS25751_OK != res)
        {
            return res;
        }

        if (TPS25751_CMD1_DONE == cmd[0])
        {
            return TPS25751_OK;                 /* コマンド正常完了 */
        }
        if (TPS25751_CMD1_REJECT == cmd[0])
        {
            return reject_code;                 /* "!CMD" -> 拒否 */
        }
        /* それ以外 = まだ 4CC 文字列が残っている -> 実行中 */

        tps25751_delay_ms(10U);
    }

    return timeout_code;
}

/*---------------------------------------------------------------------------*
 * 公開 API
 *---------------------------------------------------------------------------*/

fsp_err_t tps25751_open(tps25751_ctx_t             * p_ctx,
                        iic_master_instance_ctrl_t * p_i2c_ctrl,
                        i2c_master_cfg_t const     * p_i2c_cfg,
                        uint8_t const              * p_bundle,
                        uint32_t                     bundle_size)
{
    if ((NULL == p_ctx) || (NULL == p_i2c_ctrl) || (NULL == p_i2c_cfg) ||
        (NULL == p_bundle) || (0U == bundle_size))
    {
        return FSP_ERR_ASSERTION;
    }

    memset(p_ctx, 0, sizeof(*p_ctx));
    p_ctx->p_i2c_ctrl  = p_i2c_ctrl;
    p_ctx->p_i2c_cfg   = p_i2c_cfg;
    p_ctx->p_bundle    = p_bundle;
    p_ctx->bundle_size = bundle_size;
    p_ctx->reg_addr    = TPS25751_ADDR_REGISTER;
    p_ctx->burst_addr  = TPS25751_ADDR_PBM_BURST;
    p_ctx->i2c_done    = false;

    /* I2C1 を初期化. Smart Configurator 側で既に open 済み (hal_data 経由) の
     * 場合は ALREADY_OPEN が返るので許容する. */
    fsp_err_t err = R_IIC_MASTER_Open(p_ctx->p_i2c_ctrl, p_ctx->p_i2c_cfg);
    if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
    {
        return err;
    }

    /* 本ドライバのコールバックとコンテキストを登録.
     * (p_callback_memory はコールバック中のみ有効な作業領域. 割り込み文脈で
     *  参照するため static で確保する.) */
    static i2c_master_callback_args_t s_cb_mem;
    err = R_IIC_MASTER_CallbackSet(p_ctx->p_i2c_ctrl, tps25751_i2c_callback, p_ctx, &s_cb_mem);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* 起動アドレスをレジスタアクセス用に設定 */
    return (TPS25751_OK == tps25751_set_addr(p_ctx, p_ctx->reg_addr)) ? FSP_SUCCESS : FSP_ERR_ABORTED;
}

fsp_err_t tps25751_close(tps25751_ctx_t * p_ctx)
{
    if ((NULL == p_ctx) || (NULL == p_ctx->p_i2c_ctrl))
    {
        return FSP_ERR_ASSERTION;
    }
    return R_IIC_MASTER_Close(p_ctx->p_i2c_ctrl);
}

tps25751_result_t tps25751_read_mode(tps25751_ctx_t * p_ctx, uint8_t dst[4])
{
    return tps25751_reg_read(p_ctx, TPS25751_REG_MODE, dst, 4U);
}

tps25751_result_t tps25751_load_patch_bundle(tps25751_ctx_t          * p_ctx,
                                             tps25751_patch_status_t * p_status)
{
    tps25751_result_t res;
    uint8_t           mode[4];
    tps25751_patch_status_t status;

    memset(&status, 0, sizeof(status));

    if ((NULL == p_ctx) || (NULL == p_ctx->p_i2c_ctrl) || (NULL == p_ctx->p_bundle))
    {
        return TPS25751_ERR_I2C;
    }

    /* 念のためレジスタアクセス用アドレスへ戻しておく */
    res = tps25751_set_addr(p_ctx, p_ctx->reg_addr);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 1. MODE == 'PTCH' を確認 (JAJA940A ステップ 2)
     *    コールドブート後, EEPROM 非搭載構成では PTCH モードで停止している.
     *-----------------------------------------------------------------*/
    {
        bool is_ptch = false;
        for (uint32_t i = 0U; i < TPS25751_MODE_POLL_MAX; i++)
        {
            res = tps25751_read_mode(p_ctx, mode);
            if (TPS25751_OK != res)
            {
                return res;
            }
            if (0 == memcmp(mode, TPS25751_MODE_PTCH, 4))
            {
                is_ptch = true;
                break;
            }
            if (0 == memcmp(mode, TPS25751_MODE_APP, 4))
            {
                /* 既に APP モード. 何もしない (二重ロード防止). */
                memcpy(status.mode, mode, 4);
                if (NULL != p_status)
                {
                    *p_status = status;
                }
                return TPS25751_ERR_NOT_PTCH;
            }
            tps25751_delay_ms(10U);
        }
        if (!is_ptch)
        {
            memcpy(status.mode, mode, 4);
            if (NULL != p_status)
            {
                *p_status = status;
            }
            return TPS25751_ERR_NOT_PTCH;
        }
    }

    /*-----------------------------------------------------------------*
     * 2. 滞留割り込みのクリア (JAJA940A ステップ 3)
     *-----------------------------------------------------------------*/
    res = tps25751_clear_interrupts(p_ctx);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 3. DATA1(0x09) に PBMs パラメータを書き込み (JAJA940A ステップ 4)
     *    [0..3] バンドルサイズ (リトルエンディアン, バイト数)
     *    [4]    I2C バーストターゲットアドレス
     *    [5]    バーストモードタイムアウト (100ms 単位)
     *-----------------------------------------------------------------*/
    uint8_t pbms_param[6];
    pbms_param[0] = (uint8_t) (p_ctx->bundle_size & 0xFFU);
    pbms_param[1] = (uint8_t) ((p_ctx->bundle_size >> 8) & 0xFFU);
    pbms_param[2] = (uint8_t) ((p_ctx->bundle_size >> 16) & 0xFFU);
    pbms_param[3] = (uint8_t) ((p_ctx->bundle_size >> 24) & 0xFFU);
    pbms_param[4] = p_ctx->burst_addr;
    pbms_param[5] = TPS25751_PBM_TIMEOUT_100MS;

    res = tps25751_reg_write(p_ctx, TPS25751_REG_DATA1, pbms_param, sizeof(pbms_param));
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 4. DATA1 を読み戻して書き込み成功を確認 (JAJA940A ステップ 5, 500us 待ち)
     *-----------------------------------------------------------------*/
    tps25751_delay_us(500U);
    {
        uint8_t readback[6];
        res = tps25751_reg_read(p_ctx, TPS25751_REG_DATA1, readback, sizeof(readback));
        if (TPS25751_OK != res)
        {
            return res;
        }
        if (0 != memcmp(readback, pbms_param, sizeof(pbms_param)))
        {
            return TPS25751_ERR_DATA1_VERIFY;
        }
    }

    /*-----------------------------------------------------------------*
     * 5. CMD1 = "PBMs" を発行し完了待ち (JAJA940A ステップ 6-8)
     *-----------------------------------------------------------------*/
    res = tps25751_run_4cc(p_ctx, TPS25751_CMD_PBMS,
                           TPS25751_ERR_PBMS_REJECTED, TPS25751_ERR_PBMS_TIMEOUT);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 6. DATA1 の PatchStartStatus を確認 (JAJA940A ステップ 9)
     *    0x00=成功 / 0x04=サイズ不正 / 0x05=アドレス不正 / 0x06=タイムアウト値不正
     *-----------------------------------------------------------------*/
    {
        uint8_t data1[8];
        res = tps25751_reg_read(p_ctx, TPS25751_REG_DATA1, data1, sizeof(data1));
        if (TPS25751_OK != res)
        {
            return res;
        }
        status.patch_start_status = data1[0];
        if (0x00U != data1[0])
        {
            if (NULL != p_status)
            {
                *p_status = status;
            }
            return TPS25751_ERR_PATCH_START;
        }
    }

    /*-----------------------------------------------------------------*
     * 7. バーストデータ書き込み (JAJA940A ステップ 10)
     *    ターゲットアドレスを burst_addr に切替え, バンドル本体を分割送信.
     *    各トランザクション間に 500us の待ちを入れる.
     *-----------------------------------------------------------------*/
    res = tps25751_set_addr(p_ctx, p_ctx->burst_addr);
    if (TPS25751_OK != res)
    {
        return res;
    }

    {
        uint32_t offset = 0U;
        while (offset < p_ctx->bundle_size)
        {
            uint32_t chunk = p_ctx->bundle_size - offset;
            if (chunk > TPS25751_BURST_CHUNK_SIZE)
            {
                chunk = TPS25751_BURST_CHUNK_SIZE;
            }

            res = tps25751_burst_write(p_ctx, &p_ctx->p_bundle[offset], chunk);
            if (TPS25751_OK != res)
            {
                (void) tps25751_set_addr(p_ctx, p_ctx->reg_addr);
                return res;
            }

            offset += chunk;
            tps25751_delay_us(500U);
        }
    }

    /* レジスタアクセス用アドレスへ戻す */
    res = tps25751_set_addr(p_ctx, p_ctx->reg_addr);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 8. 滞留割り込みの再クリア (JAJA940A ステップ 11)
     *-----------------------------------------------------------------*/
    res = tps25751_clear_interrupts(p_ctx);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 9. CMD1 = "PBMc" を発行し完了待ち (JAJA940A ステップ 12-14)
     *    CRC チェックが行われ, 成功するとパッチが適用される.
     *-----------------------------------------------------------------*/
    res = tps25751_run_4cc(p_ctx, TPS25751_CMD_PBMC,
                           TPS25751_ERR_PBMC_REJECTED, TPS25751_ERR_PBMC_TIMEOUT);
    if (TPS25751_OK != res)
    {
        return res;
    }

    /*-----------------------------------------------------------------*
     * 10. 20ms 待機 (JAJA940A ステップ 15) — PD がイメージを読み込み適用する時間
     *-----------------------------------------------------------------*/
    tps25751_delay_ms(20U);

    /*-----------------------------------------------------------------*
     * 11. DATA1 から完了ステータスを読み出し (JAJA940A ステップ 16)
     *     SLVUCR8B Table 4-16 (OUTPUT DATAX):
     *       DATAX Byte1  bits 7:4 = rpReturnIndicator, bits 3:0 = acReturnIndicator
     *       DATAX Byte3  (bits 23:16) = DevicePatchCompleteStatus
     *       DATAX Byte4  (bits 31:24) = AppConfigPatchCompleteStatus
     *-----------------------------------------------------------------*/
    {
        uint8_t data1[40];
        res = tps25751_reg_read(p_ctx, TPS25751_REG_DATA1, data1, sizeof(data1));
        if (TPS25751_OK != res)
        {
            return res;
        }
        status.rp_return_indicator   = (uint8_t) ((data1[0] >> 4) & 0x0FU);
        status.ac_return_indicator   = (uint8_t) (data1[0] & 0x0FU);
        status.device_patch_complete = data1[2];   /* DATAX Byte3 */
        status.appcfg_patch_complete = data1[3];   /* DATAX Byte4 */

        /* Device: 0x00=成功. AppConfig: 0x80=失敗. */
        if ((0x00U != status.device_patch_complete) ||
            (0x80U == status.appcfg_patch_complete))
        {
            if (NULL != p_status)
            {
                *p_status = status;
            }
            return TPS25751_ERR_PATCH_COMPLETE;
        }
    }

    /*-----------------------------------------------------------------*
     * 12. MODE == 'APP ' を確認 (JAJA940A ステップ 17)
     *-----------------------------------------------------------------*/
    {
        bool is_app = false;
        for (uint32_t i = 0U; i < TPS25751_MODE_POLL_MAX; i++)
        {
            res = tps25751_read_mode(p_ctx, mode);
            if (TPS25751_OK != res)
            {
                return res;
            }
            if (0 == memcmp(mode, TPS25751_MODE_APP, 4))
            {
                is_app = true;
                break;
            }
            tps25751_delay_ms(10U);
        }
        memcpy(status.mode, mode, 4);
        if (NULL != p_status)
        {
            *p_status = status;
        }
        if (!is_app)
        {
            return TPS25751_ERR_NOT_APP;
        }
    }

    return TPS25751_OK;
}
