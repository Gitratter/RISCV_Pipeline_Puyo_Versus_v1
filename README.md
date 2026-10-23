# RISCV Puyo Versus v3

既存の RV32I ソフトウェアゲームを、１人用を残して２人対戦へ拡張した版です。
ゲームルール・乱数・相殺・おじゃま・勝敗はすべて RV32I CPU 上で実行します。
Verilog は CPU、BRAM、入力、MMIO、VGA を担当します。専用ゲーム判定回路へ
戻す変更はしていません。GitHub への書込みは行っていません。

## すぐ使用する場合

配布用 `RISCV_Puyo_Versus_v3_Vivado.zip` をフォルダごと展開し、
`RISCV_Puyo_Versus_v3.xpr` を Vivado 2025.2 で開いてください。
HDL、XDC、MEM、テストベンチは登録済みで、手動の Add Sources は不要です。
まず配布物直下の `はじめに.txt` を参照してください。

- Basys 3 / xc7a35tcpg236-1、基板入力100 MHz、内部CPU/VGA25 MHz。
- SW0=0：１人用、SW0=1：２人対戦。開始前に選択します。
- SW14を0→1：開始。３秒カウントダウン。結果画面で0→1：再戦。
- P1：左右ボタン移動、下高速落下、上右回転、中央左回転。
- P2：J2 USB Type-Aにキーボード、←/→/↓、Z左回転、X右回転。
- 対戦中のSW14/SW0変更は試合をリセットしません。SW15は専用全体リセット。
- １人用のプレイ中は、従来どおりSW14によるリスタートを残しています。

## 文書

- [全体構成・MMIO・CPUと回路の分担](docs/ARCHITECTURE.md)
- [簡易対戦ルール・試験の対応表](docs/RULES_AND_TESTS.md)
- [段階別の変更点](docs/STAGES.md)
- [キーボードと公式接続資料](docs/KEYBOARD.md)
- [画面配置と表示形式](docs/VIDEO.md)
- [元コードの出所](docs/PROVENANCE.md)
- [合成・配置配線と検証範囲](docs/VERIFICATION.md)

## 開発用フォルダと配布用フォルダ

この README が開発用 `outputs/RISCV_Puyo_Versus_v3` にある場合、rtl/、sim/、
constrs/ が原本です。配布用ではそれらを XPR に隣接した `.srcs` に集約し、
この README と C/説明書/再構築スクリプトは `software/` に配置しています。
配布用 `.srcs` にある Verilog が Vivado から実際に参照されるソースです。

再ビルドは配布用のルートで次を実行します。Python3とRISC-V bare-metal GCCが
必要なのはCを変更する場合だけです。完成済みMEMで合成するだけなら不要です。

```powershell
python software/scripts/build_firmware.py --variant all --gcc "C:/AMDDesignTools/2025.2/gnu/riscv/nt/bin/riscv64-unknown-elf-gcc.exe"
python software/scripts/run_versus_host_tests.py --gcc "C:/AMDDesignTools/2025.2/tps/mingw/10.0.0/win64.o/nt/bin/gcc.exe"
```

MEMの生成後は登録済み `.srcs` 内へ自動反映します。その後Vivadoの合成・
Implementation・Generate Bitstreamを再実行してください。Cだけ変更して既存の
bitを使っても動作は変わりません。`programming/` のbitは配布時点の完成品であり、
再ビルド後の新しいbitへ自動置換されません。

バッチで再合成する場合は、`software/scripts/run_vivado.ps1` が利用できます。
標準インストール先が異なる場合は `-VivadoRoot` を指定してください。出力先は
`software/build/vivado/` です。GUIのGenerate Bitstreamも利用できます。

RTL回帰は `software/scripts/run_sim_tests.py`、Cの試験対応は
`software/tests/README_VERSUS.md` を参照してください。シミュレーション上の
短縮時計・50msカウントダウンはテスト用MEMだけで、実機用MEMは３秒です。
