# P2 キーボード入力

## 接続と公式資料

Basys 3 の J2（USB Type-A、USB HID Host）へキーボードを直接接続する。
基板上の PIC24 が USB HID を PS/2 形式に変換する。FPGA の実装は USB
ホストそのものではなく、この PS/2 信号の受信器である。J4 の Micro-USB
（書込み/JTAG/UART）へ接続するものではない。

- [Digilent Basys 3 Reference Manual、§6](https://digilent.com/reference/_media/reference/programmable-logic/basys-3/basys3_rm.pdf)
- [公式 Master XDC](https://github.com/Digilent/digilent-xdc/blob/master/Basys-3-Master.xdc)
- [公式 Keyboard Demo](https://github.com/Digilent/Basys-3-Keyboard)
- [公式 PS2Receiver.v](https://github.com/Digilent/Basys-3-Keyboard/blob/master/src/hdl/PS2Receiver.v)
- [公式 top.v](https://github.com/Digilent/Basys-3-Keyboard/blob/master/src/hdl/top.v)

公式デモの確認時 commit は `f42a62c9a1ab41298e0258191694a8c8a7c6ffec`。
FPGA の clock/data 端子は C17/B17、LVCMOS33、両方とも PULLUP=true。
マニュアルに従い、受信専用なので入力端子として使用する。
ハブは対応しない。実際の USB キーボード、複合デバイス、無線受信機との
互換性は実機で確認する必要があり、USB 端子へ挿せるすべての機器が使える
という意味ではない。

## 受信処理

`PuyoKeyboard` は内部クロックだけで動作する。PS2Clk を新たなクロックに
せず、clock/data を各 2FF 同期し、clock が FILTER_CYCLES 回連続で変化した
状態を保った場合だけフィルタ出力を更新する。標準値は 25 MHz / 8 回。
フィルタ後の立下りで start、8 bit LSB-first、odd parity、stop を検査する。
`sync_meta` が非同期入力を受ける最初の同期 FF であり、XDC の例外はこの
FF の D 入力までに限る。

`byte_valid` は妥当なフレームごとに 1 クロックだけ立ち、`last_byte` はその
値を保持する。キー以外の正常なバイトでも byte_valid は出る。フレームの
途中または E0/F0/E1 の途中で 2000 us クロックエッジが来なければ timeout。
異常フレーム・不正な prefix・timeout はカウンタを増やし、prefix と押下
状態をクリアする。error_count は 32 bit 飽和カウンタであり負数化しない。
設定値 CLK_HZ は本プロジェクトの 25 MHz、試験時は 1 MHz を使用する。

E0 は拡張キー、F0 は解放。通常の解放は F0 code、矢印の解放は E0 F0 code。
未知のコードはゲーム入力に変換せず、そのコードで prefix を消費する。
Pause は E1 の後ろ 7 バイトを丸ごと捨てる。内部の E1/F0 を別キーの
prefix に誤解しない。BAT 成功 AA は押下状態をクリアし、BAT 失敗 FC/FD
および overrun 00/FF はクリアとエラーカウント増加を行う。

| keys bit | 操作 | make | break |
|---:|---|---|---|
| 0 | 左 | E0 6B | E0 F0 6B |
| 1 | 右 | E0 74 | E0 F0 74 |
| 2 | 高速落下 | E0 72 | E0 F0 72 |
| 3 | Z：左回転 | 1A | F0 1A |
| 4 | X：右回転 | 22 | F0 22 |

## ゲームとの役割分担

出力は押下イベントではなく、5 個独立した押下状態である。同じ make が
繰り返されても既に 1 の bit を 1 にするだけなので、新しい回転の立上りを
作らない。左右・回転・下を同時保持できる。左右同時押しの無移動、DAS/ARR、
回転の一回化は P1 と共通の RV32I ゲーム処理で行う。キーボード自身の
typematic 速度はゲームの移動速度を決めない。Enter やゲームキーから
試合の再スタート・システムリセットは行わない。

公式デモ top.v にも、複数キー操作中に保持キーの make が再送される場合が
ある旨の注記がある。本実装の状態保持方式はこの重複を無害化する。
公式サンプルをそのまま複製せず、上記の同期・検査・状態管理を加えた。

**保持キーを無通信時間だけで自動解除してはいけない。** 複数キーのうち
すべてのキーが継続して typematic 送信される保証はない。本受信器にも
そのような保持タイムアウトはない。したがって、解放フレームを失う切断の
検出までは保証しない。エラー時には安全側へ状態をクリアするが、再接続時の
動作は実機確認が必要。再戦時の保持キーはソフトウェアが開始時のキーを
記録し、物理 release を観測するまで抑止する。受信器の単純 reset だけでは、
後から届く typematic を新押下と誤認し得るためである。

## テスト

`tb_keyboard.v` は PS/2 のシリアル波形を生成し、短い clock glitch、正常な
押下/解放、矢印 E0/F0、Z/X の make 再送、左右と回転/下の同時保持、保持時の
長時間無通信、未知コード、Pause、parity/stop/start 異常、部分フレームと
prefix の timeout、BAT と overrun、error_count の飽和、reset 後の復帰を検査する。
実行結果、全体の合成・タイミング、実機検証の区別は全体の検証報告を参照。
