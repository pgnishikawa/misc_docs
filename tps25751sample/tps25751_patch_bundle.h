/*
 * tps25751_patch_bundle.h
 *
 * TPS25751 の Low Region Binary (パッチバンドル) 宣言.
 *
 * ★ 実データの入手方法 ★
 *   1. TI "USB-C PD アプリケーションカスタマイズツール" (GUI) で設定を作成
 *   2. Export メニューから "Low Region" バイナリを生成
 *   3. 出力形式に「C ファイル」を選択  (JAJA940A セクション 4.2 / 図 4-13)
 *   4. 生成された配列を tps25751_patch_bundle.c に貼り付け,
 *      TPS25751_PATCH_BUNDLE_SIZE を実際のサイズに合わせる
 *
 * JAJA940A の例では以下の値だった:
 *   #define SIZEOFLRB 0x2C80    // 11392 バイト
 */

#ifndef TPS25751_PATCH_BUNDLE_H
#define TPS25751_PATCH_BUNDLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* パッチバンドルのバイト数. GUI が生成した実サイズに置き換えること. */
#define TPS25751_PATCH_BUNDLE_SIZE   (0x2C80U)   /* 11392 (JAJA940A の例と同じ) */

/* パッチバンドル本体. tps25751_patch_bundle.c で定義. */
extern const uint8_t g_tps25751_patch_bundle[TPS25751_PATCH_BUNDLE_SIZE];

#ifdef __cplusplus
}
#endif

#endif /* TPS25751_PATCH_BUNDLE_H */
