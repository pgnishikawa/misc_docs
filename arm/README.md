# Arm アーキテクチャ参考資料

RZ/N2H の Cortex-A55／Cortex-R52 の MMU/MPU・キャッシュ・ブート手順を正確に記述するために取得した、Arm 公式ドキュメント。
`docs/11_memory_performance_comparison.md`・`docs/12_ddr_noncacheable_region_setup.md` の記述はこれらの一次資料で裏付けている。

| ファイル | 内容 | RZ/N2H マニュアルとの対応 |
|----------|------|---------------------------|
| `cortex_r52_trm_r1p3.pdf` | Arm® Cortex®-R52 Processor Technical Reference Manual, revision **r1p3**（694p） | RZ/N2H マニュアルは Cortex-R52 の revision を **r1p4-00rel0** と記載（1.1 節）。r1p3 との版差は誤記訂正（errata）が中心で、MPU/TCM/キャッシュのアーキテクチャ的な記述（本書で参照した箇所）に影響する差分はない見込み。r1p4 の TRM が別途必要な場合は `developer.arm.com/documentation/100026/0104/` を参照 |
| `cortex_r52_programmers_guide.pdf` | Cortex-R52 and Cortex-R52+ Programmer's Guide（42p）| MPU 設定・TCM 設定・EL2→EL1 ブートコードの具体例を含む実践的ガイド。RZ/N2H マニュアル「2.6.1 Cortex-A55の起動方法」「3.6.1/3.6.2 Cortex-R52の起動方法」と対応する Arm 側の一般知識を補完 |
| `cortex_a55_trm_r2p0.pdf` | Arm® Cortex®-A55 Core Technical Reference Manual, revision **r2p0**（810p）| RZ/N2H マニュアルの Cortex-A55 revision 記載（**r2p0**）と**完全一致**。**A55 は AArch32 実行状態で使用する確定仕様のため、本資料の Table A1-1（実装オプション一覧＝AArch32 無効化オプションが存在しないことの確認）・Table A1-2（AArch64/AArch32 両対応の明記）・Chapter B1（AArch32 system registers、特に §B1.83 TTBCR／§B1.84 TTBCR2／§B1.85 TTBR0）が中心的な参照箇所** |
| `armv8r_aarch32_supplement_ddi0568.pdf` | Arm® Architecture Reference Manual Supplement, Armv8, for the Armv8-R AArch32 architecture profile（356p、非公式ミラー経由で取得）| Cortex-R52 の MPU（PMSA）アーキテクチャ仕様そのもの。TRM が実装固有の記述、この資料がアーキテクチャ標準の記述、という役割分担 |
| `aarch64_memory_management_guide.pdf` | Arm「Learn the Architecture」シリーズ、AArch64 memory management ガイド v1.3（33p）| **参考資料**（A55 は AArch32 で使用するため直接の一次資料ではない）。AArch64 の変換テーブル記述子の一般構造（Table/Block/Page/Fault、Lower/Upper Attributes の分離）を把握するために参照。AArch32 Long-descriptor（LPAE）形式もほぼ同型と考えられるが、正確なビット位置は DDI0487 の AArch32 側章で要確認 |

### A55 が AArch32 で使用可能であることの根拠

RZ/N2H マニュアル表2.1（p.176）は A55 の「命令セット」を「Armv8.2-A A64命令セット (AArch64)」と記載しているが、`cortex_a55_trm_r2p0.pdf` の実装オプション一覧（Table A1-1）には「AArch32 サポートの有無」という構成オプション自体が存在せず、Table A1-2 には **"AArch64 and AArch32 execution states at all Exception levels"**（全 Exception Level で AArch64 と AArch32 の両方の実行状態をサポート）と明記されている。したがって **RZ/N2H の A55 実装で AArch32 は常時利用可能**であり、Renesas マニュアルの記載は「AArch64 にも対応する」という趣旨の要約と解釈できる。

`txt/` 配下に `pdftotext -layout` で抽出したテキスト版を置いている（検索用、Git 管理対象外を想定）。

## 入手元 URL

- Cortex-R52 TRM r1p3: https://documentation-service.arm.com/static/5f905fedf86e16515cdc25e2
- Cortex-R52/R52+ Programmer's Guide: https://documentation-service.arm.com/static/684168dd0e569e6ff14edad1
- Cortex-A55 TRM r2p0: https://documentation-service.arm.com/static/5e7e09f6a3736a0d2e862d2f
- Armv8-R AArch32 Architecture Reference Manual Supplement (DDI0568): http://kib.kiev.ua/x86docs/ARM/ARMARMv8/DDI0568_armv8_r32_supplement.pdf （非公式ミラー。公式は developer.arm.com/documentation/ddi0568/latest/ だが JS レンダリングのため自動取得不可）
- Learn the Architecture: AArch64 memory management: https://documentation-service.arm.com/static/655de9bdee7b9d0d88eb7dbb

## 未取得・今後必要になり得る資料

- **Arm® Architecture Reference Manual Armv8, for Armv8-A architecture profile (DDI0487)** — A55 の MMU 変換テーブルディスクリプタの正確なビット位置（AttrIndx/SH/AP の位置等）が必要な場合。数千ページの巨大文書のため、必要な章（Translation table descriptor formats）だけを絞って取得することを推奨。
- **Cortex-R52 TRM r1p4** — RZ/N2H が採用する正確なコア revision。developer.arm.com は JS レンダリングのため自動取得できず、r1p3 で代用している（内容差分は軽微と判断）。
