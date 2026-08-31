/*
 * tps25751_patch_bundle.c
 *
 * ★★★ これはプレースホルダです ★★★
 *
 * このファイルの g_tps25751_patch_bundle[] を, TI "USB-C PD アプリケーション
 * カスタマイズツール" が生成した Low Region Binary (C 配列) で置き換えてください.
 * TPS25751_PATCH_BUNDLE_SIZE (tps25751_patch_bundle.h) も実サイズに合わせること.
 *
 * JAJA940A セクション 5 の例:
 *
 *   const uint8_t tps25751_lowRegion_i2c_array[SIZEOFLRB] = {
 *   0x01, 0x00, 0xe0, 0xac, 0xfe, 0xff, 0xff, 0xff, 0x80, 0x06, 0x00, 0x00, ...
 *   ...
 *   0x00, 0x00, 0x00, 0x00 };
 *
 * 配列の内容はデバイス構成 + パッチ本体 (バンドル) であり,
 * PBMs コマンドで通知した「バンドルサイズ」と完全に一致している必要があります.
 */

#include "tps25751_patch_bundle.h"

/* ダミーデータ (全 0). 実機では必ず実バンドルに差し替えること.
 * 全 0 のままロードを試みると PBMc の CRC チェックで失敗します. */
const uint8_t g_tps25751_patch_bundle[TPS25751_PATCH_BUNDLE_SIZE] = { 0 };
