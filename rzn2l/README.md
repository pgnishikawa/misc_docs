# RZ/N2L 調査ドキュメント

ルネサス RZ/N2L に関する調査用のまとめです。元資料は本リポジトリ
`rzn2l/r01uh0955jj0130-rzn2l-users-manual-hardware/` 配下の PDF です。

| 資料 | ファイル | 版数 | ページ数 |
|------|----------|------|----------|
| ユーザーズマニュアル ハードウェア編 | `r01uh0955jj0130-rzn2l.pdf` | R01UH0955JJ0130 Rev.1.30 (Dec 8, 2023) | 2500 |

> RZ/N2L は Arm Cortex-R52 **シングルコア**の ASSP。同じ RZ/N シリーズの RZ/N2H
> （[../../rzn2h/docs/](../../rzn2h/docs/) に別途調査あり、Cortex-A55クワッド＋Cortex-R52デュアル）
> とは別冊のマニュアルで、機能規模も異なる（ローエンド機）。RZ/N2L は **EtherCAT スレーブ
> コントローラ (ESC) を内蔵**しているが、本リポジトリでは外付け ET1100
> （[ET1100/docs/](../../ET1100/docs/)）を RZ/N2L の外部バス経由で接続する調査も別途行っている。

## 現在の調査タスク

1. **全体概要の把握** → [01_overview.md](01_overview.md)
2. **xSPI0/xSPI1 ブートモードの詳細**（ブートROM挙動、ローダ配置制約、ランタイムでの
   xSPI操作の可否）→ [04_xspi_boot_and_runtime.md](04_xspi_boot_and_runtime.md)
3. **Quad SPI フラッシュ接続時のプロトコルモード選定**（1S-1S-1S〜4S-4S-4S、QE bit、
   0xEB読み出しコマンド）→ [05_xspi_protocol_modes_and_quad_flash.md](05_xspi_protocol_modes_and_quad_flash.md)
4. **アドレス空間（メモリマップ）の完全版**（周辺モジュール詳細ベースアドレス、バスマスタ別
   アクセス可否、外部ホストインタフェースのメモリマップ）→ [06_memory_map.md](06_memory_map.md)
5. **BSC CS0 に ET1100 を接続するためのレジスタ設定**（ライトプロテクション解除、
   I/Oポートのピン設定、モジュールストップ解除、CS0BCR/CS0WCR_0）→
   [07_bsc_cs0_et1100_register_setup.md](07_bsc_cs0_et1100_register_setup.md)

（以降、必要に応じてクロック/リセット、割り込みコントローラ等を追加調査予定。）

## ドキュメント一覧

| ファイル | 内容 |
|----------|------|
| [01_overview.md](01_overview.md) | チップ概要、CPU/メモリ仕様、動作モード概観、クロック、周辺機能一覧、ブロック図（テキスト）、製品ラインナップ |
| [02_manual_toc.md](02_manual_toc.md) | ユーザーズマニュアルの章・節インデックス（PDFページ番号付き）。調査時にジャンプ先を探すための地図 |
| [03_boot_modes.md](03_boot_modes.md) | 動作モード（8ブートモード）、MDn/MDVn端子、ブートフロー、ローダ用パラメータ、アドレス空間（ユニファイドメモリマップ） |
| [04_xspi_boot_and_runtime.md](04_xspi_boot_and_runtime.md) | xSPI0/xSPI1（x1/x8）ブートモードの詳細（ブートROM挙動、ローダ配置制約）とランタイムでのxSPI操作（メモリマッピング／マニュアルコマンド／XiP直接実行の可否／NORフラッシュ書き込みの可否／パターン制御によるフラッシュリセット） |
| [05_xspi_protocol_modes_and_quad_flash.md](05_xspi_protocol_modes_and_quad_flash.md) | xSPIプロトコルモード（1S-1S-1S〜4S-4S-4S等）の記法とRZ/N2Lが対応する7種類の一覧、Quad SPI NORフラッシュ接続時の実践的な設計（QE bit、0xEB Quad I/O読み出し、Page Programの扱い、ブート時との関係） |
| [06_memory_map.md](06_memory_map.md) | アドレス空間の完全版（ユニファイドメモリマップの周辺モジュール詳細ベースアドレス、バスマスタ別アクセス可否の違い、TCMのAXIS経由エイリアス、外部ホストインタフェースから見えるメモリマップ） |
| [07_bsc_cs0_et1100_register_setup.md](07_bsc_cs0_et1100_register_setup.md) | BSC CS0にET1100を接続するための全レジスタ設定（レジスタライトプロテクション解除、RSELPm/PMCm/PFCmによるピン設定、MSTPCRAによるBSCモジュールストップ解除、CS0BCR/CS0WCR_0、サンプル初期化コード） |

## 元テキストの扱い（調査用）

PDF から抽出したテキストを `work/` 配下に置いています（Git 管理対象外を想定）。

- `work/manual_full.txt` … マニュアル全文（`pdftotext -layout`）
- `work/pages/pXXXX.txt` … マニュアルを 1 ページ 1 ファイルに分割（`pXXXX` = PDF ページ番号と一致）
- `work/toc_full.txt` … しおり（ブックマーク）の全階層抽出（`pypdf` 経由）
- `work/toc_depth2.txt` / `work/toc_body.md` … 章・節（深さ2まで）に絞った中間ファイル（[02_manual_toc.md](02_manual_toc.md) の元）

再生成する場合:

```bash
pdftotext -layout r01uh0955jj0130-rzn2l-users-manual-hardware/r01uh0955jj0130-rzn2l.pdf work/manual_full.txt
mkdir -p work/pages
python3 -c "d=open('work/manual_full.txt').read().split('\x0c'); [open(f'work/pages/p{i:04d}.txt','w').write(p) for i,p in enumerate(d,1)]"

# しおり（TOC）の抽出
python3 - <<'EOF'
import pypdf
r = pypdf.PdfReader("r01uh0955jj0130-rzn2l-users-manual-hardware/r01uh0955jj0130-rzn2l.pdf")
def walk(items, depth=0):
    for it in items:
        if isinstance(it, list):
            walk(it, depth+1)
        else:
            p = r.get_destination_page_number(it) + 1
            print(f"{'  '*depth}{it.title}  (p.{p})")
walk(r.outline)
EOF
```

特定トピックを調べるときは [02_manual_toc.md](02_manual_toc.md) でページを特定し、
`work/pages/pXXXX.txt` を読むのが速いです。

## 既知の要点メモ（調査で判明）

- 対象製品は **R9A07G084M0xGBG/GBA**（225ピンFBGA or 121ピンFBGA、末尾08=セキュリティ有効/04=無効）。
- CPUは **Arm Cortex-R52 シングルコア**（r1p2）のみ。RZ/N2Hのような複数コア/AMP構成は無い。
- ブートは常に唯一のCPUが実行。ローダプログラムはBTCM（`0x0010_2000`〜`0x0011_FFFF`、
  **最大120KB**）に展開される。RZ/N2H（BTCM64KB中52KB使用可）より一段目ローダの余裕が大きい。
- xSPI0/xSPI1のブート用アドレス空間は `0x6000_0000`/`0x6800_0000`（RZ/N2Hの`0x4000_0000`/
  `0x5000_0000`とは異なるベースアドレス。**RZ/N2HとRZ/N2Lでアドレス値を混同しないこと**）。
- BSC（バスステートコントローラ）の外部アドレス空間（CS0/2/3/5）は `0x7000_0000`〜256MB、
  ミラーが`0x5000_0000`〜。この実アドレスは `rz-fsp`（`bsp_feature.h`）の
  `BSP_FEATURE_BSC_NOR_CS0_BASE_ADDRESS = 0x70000000` 等と一致（[ET1100/docs/](../../ET1100/docs/)参照）。
- xSPIのメモリマッピング書き込みは「1AHBアクセス→固定1コマンド」の単純な対応のみで、
  NORフラッシュのプログラム/イレース（WREN前置・イレース・ステータスポーリングが必要）とは
  構造的に噛み合わない。書き込みにはマニュアルコマンドモードの実装が必要
  （[04_xspi_boot_and_runtime.md](04_xspi_boot_and_runtime.md) §2.5、マニュアル36.3.3.2に明記）。
- xSPIのXiP実行（メモリマッピングモードでの直接命令フェッチ）は技術的に可能（マニュアル
  36.3.3.6に明記）だが、RAW 200MB/sとTCM/システムRAMとの速度差からCortex-R52の
  リアルタイム制御ループには不適。ブートROM自身もXiPを使わずTCM展開してから実行している。
- Quad SPIフラッシュを使う場合、RZ/N2Lのプロトコルモードは`1S-1S-1S`/`1S-4S-4S`等の
  **7種類の列挙値のみ**で任意組み合わせ不可（マニュアル36.2.1.7に明記）。一般的な
  Quad Outputコマンド(`0x6B`, 1S-1S-4S)やQuad Page Program(`0x32`, 1S-1S-4S)は非対応で、
  **`0xEB`（Quad I/O, 1S-4S-4S）系のみ対応**。詳細は
  [05_xspi_protocol_modes_and_quad_flash.md](05_xspi_protocol_modes_and_quad_flash.md)。
- **TCM（ATCM/BTCM）にはCPU0直結ポート(`0x0000_0000`/`0x0010_0000`)とは別に、他バスマスタ
  （DMAC/GMAC,USB/CoreSight）専用のAXIS経由エイリアス(`0x2000_0000`/`0x2010_0000`)が存在**し、
  DMA転送でTCMを指定する際は後者を使う必要がある（マニュアル図4.1注1、図4.2）。詳細は
  [06_memory_map.md](06_memory_map.md) §2.1。
- **★ `RSELPm` はリセット後デフォルトで全ピンが「セーフティ領域」選択になっており、
  通常のノンセーフティアドレス（`0x800A_0xxx`）から `PMCm`/`PFCm`/`DRCTLm` に書き込んでも
  対象ビットを`RSELPm`で1にするまでは読み出し専用のまま**（マニュアル16.3.7）。GPIO/BSC等
  ピン設定が反映されない不具合の典型原因になるため要注意。詳細は
  [07_bsc_cs0_et1100_register_setup.md](07_bsc_cs0_et1100_register_setup.md) §2。
- BSC CS0 を「通常空間・16bit・外部WAIT有効」（ET1100接続の方式Aが要求する設定）で使う場合、
  **`CS0BCR`/`CS0WCR_0` は実はリセット値のままで要件を満たす**（`TYPE=000b`, `BSZ=10b`,
  `WM=0`, `BAS=0` がいずれもデフォルト）。変更が必須なのは `MSTPCRA`（BSCモジュールストップ
  解除）と `RSELPm`/`PMCm`/`PFCm`（ピン設定）の方。詳細は
  [07_bsc_cs0_et1100_register_setup.md](07_bsc_cs0_et1100_register_setup.md)。
