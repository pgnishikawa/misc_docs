/*
 * tps25751_i2c1_cfg.c
 *
 * RZ/N2H の I2C1 (FSP r_iic_master, channel 1) を手書きで構成する参考テンプレート.
 *
 * ============================================================================
 *  通常は RZ Smart Configurator で構成することを強く推奨します.
 * ============================================================================
 *  Smart Configurator で "r_iic_master" スタックを追加し, 以下を設定すると
 *  hal_data.c に g_i2c1_ctrl (iic_master_instance_ctrl_t) /
 *  g_i2c1_cfg (i2c_master_cfg_t) / g_iic_master1_extended_cfg
 *  および vector_data.c (ICU イベントリンク, NVIC ベクタ) が自動生成されます.
 *  その場合はこのファイルは不要で, サンプルからは
 *  &g_i2c1_ctrl / &g_i2c1_cfg を渡してください.
 *
 *  [Smart Configurator の設定例]
 *    - Channel                 : 1
 *    - Rate                    : Fast-mode (400kHz)   ※RZ/N2H は Fm+ 非対応
 *    - Slave Address           : 0x22 (7-bit)          ※実行時に切替えるので任意
 *    - Address Mode            : 7-Bit
 *    - Timeout Mode            : Short
 *    - Callback                : NULL でも可 (tps25751_open() が CallbackSet する)
 *    - DMAC 転送               : 不要 (未使用)
 *    - 割り込み優先度          : 任意 (例 12)
 *    - Pin 設定                : P**/P** を I2C1 SCL/SDA に割当て, 外部プルアップ確認
 *
 * ----------------------------------------------------------------------------
 *  このファイル (手書き) を使う場合は TPS25751_PROVIDE_I2C1_CONFIG を定義し,
 *  かつ vector_data.c 相当 (IIC1_RXI/TXI/TEI/EEI の ISR 登録と ICU イベント
 *  リンク) を別途用意する必要があります. ISR 実体は r_iic_master.c 内の
 *  iic_master_rxi_isr / iic_master_txi_isr / iic_master_tei_isr / iic_master_eri_isr.
 * ----------------------------------------------------------------------------
 */

#ifdef TPS25751_PROVIDE_I2C1_CONFIG

#include "r_iic_master.h"
#include "vector_data.h"       /* VECTOR_NUMBER_IIC1_RXI 等 (要生成) */

#ifndef VECTOR_NUMBER_IIC1_RXI
 #error "IIC1 の割り込みベクタが未定義です. Smart Configurator で IIC1 の割り込みを有効化して vector_data.h を生成するか, 手動で ICU イベントリンクと NVIC ベクタを設定してください."
#endif

/* 制御ブロック (ゼロ初期化, 内容には触れない) */
iic_master_instance_ctrl_t g_i2c1_ctrl;

/* 拡張設定: クロック設定はコア/ペリフェラルクロックに依存する.
 * 下の値は「例」であり, 実際の P0CLK (RZ/N2H では通常 100MHz) に合わせて
 * Smart Configurator に再計算させた値へ置き換えること.
 *   SCL 周波数 f = PCLKL / ((BRH+1 + BRL+1) * 2^CKS + ...)   ※詳細は RZ/N2H UM
 */
static const iic_master_extended_cfg_t g_i2c1_extend =
{
    .timeout_mode    = IIC_MASTER_TIMEOUT_MODE_SHORT,
    .timeout_scl_low = IIC_MASTER_TIMEOUT_SCL_LOW_ENABLED,
    .clock_settings  =
    {
        /* ↓ 400kHz 相当の「仮値」. 必ず実クロックで再計算すること. */
        .cks_value = 0x02,
        .brh_value = 0x11,
        .brl_value = 0x13,
    },
    .p_reg = R_IIC1,
};

const i2c_master_cfg_t g_i2c1_cfg =
{
    .channel       = 1,
    .rate          = I2C_MASTER_RATE_FAST,          /* 400kHz */
    .slave         = 0x22,                          /* 初期アドレス. 実行時に 0x22/0x30 を切替 */
    .addr_mode     = I2C_MASTER_ADDR_MODE_7BIT,
    .ipl           = 12,
    .rxi_irq       = VECTOR_NUMBER_IIC1_RXI,
    .txi_irq       = VECTOR_NUMBER_IIC1_TXI,
    .tei_irq       = VECTOR_NUMBER_IIC1_TEI,
    .eri_irq       = VECTOR_NUMBER_IIC1_EEI,
    .p_transfer_tx = NULL,
    .p_transfer_rx = NULL,
    .p_callback    = NULL,                          /* tps25751_open() が CallbackSet する */
    .p_context     = NULL,
    .p_extend      = &g_i2c1_extend,
};

/* 本サンプルは R_IIC_MASTER_* を直接呼ぶため, 汎用インタフェースの
 * i2c_master_instance_t (g_i2c1) は不要. 必要なら以下を有効化する.
 *
 * const i2c_master_instance_t g_i2c1 =
 * {
 *     .p_ctrl = &g_i2c1_ctrl,
 *     .p_cfg  = &g_i2c1_cfg,
 *     .p_api  = &g_i2c_master_on_iic,
 * };
 */

#endif /* TPS25751_PROVIDE_I2C1_CONFIG */
