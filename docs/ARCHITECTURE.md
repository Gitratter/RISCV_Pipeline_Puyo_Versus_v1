# 構成と設計判断

## 研究上の位置づけ

採用案は「１個の既存RV32IパイプラインCPUが、同じゲーム処理を２つの状態に
対して実行する」です。ハードウェアゲームエンジンを２個置く構成ではありません。
元リポジトリがすでにRV32Iソフトウェア版であること、およびCPUでゲームを
実行する研究目的を踏まえ、承認された案1に従っています。

```text
P1ボタン → 2FF/デバウンス ─┐
P2 USB HID → PS/2受信器 ──┼→ MMIO読出し → 既存RV32I CPU
25MHz → ミリ秒時計 ──────┘                  │
                                   データBRAM内の PuyoMatch
                                     ├ PuyoGame[0]
                                     ├ PuyoGame[1]
                                     └ 相殺・降下予約・勝敗
                                             │ 表示情報を書込み
                                      ２面MMIO一括commit
                                             │ 垂直帰線でcapture
                                         VGA描画・出力
```

CPUのALU、命令デコーダ、パイプライン、同期BRAM待ち制御は変更していません。
CPUを25MHzで常時実行し、ゲームの落下・長押し・演出周期はミリ秒時刻から
独立に判定します。ゲームを1Hz/10HzでCPUごと進める構成ではありません。

## ファイルの担当

| ファイル | 役割 |
|---|---|
| firmware/game.c, game.h | 共通の移動・回転・衝突・重力・BFS・消去・得点、追加のおじゃま処理 |
| firmware/match.c, match.h | ２人の管理、同時開始、相殺、降下予約、結果・再戦 |
| firmware/versus_main.c | 入力時刻読出し、共通更新、２人分の表示書込みとcommit |
| firmware/start.S, link.ld, runtime.c | 起動、BSS初期化、メモリ配置、C実行補助 |
| PuyoKeyboard.v | PS/2受信、押下/解放、E0/F0、異常検出 |
| PuyoVersusSystem.v | 新top、既存CPU・周辺回路の接続、モードSWの同期化 |
| PuyoVersusMMIO.v | ２面分の表示メールボックス、入力・時刻・カウンタ |
| PuyoVersusVideo.v | 左右２面の描画、NEXT、得点、連鎖、予告、結果 |
| PuyoVideo.v | 変更しない従来の１人用描画回路 |

従来のPuyoSoftwareSystem/MMIOとmain.cは回帰確認用に残しています。新topからは
それらの旧システムをインスタンス化しません。未使用の旧topがファイル一覧に
あっても、CPUが２個合成されることはありません。

## 「同時」の定義と盤面の所有

CPUは１個なので、命令実行自体は直列です。しかしP1とP2の手番を交互に待つ
ゲームではありません。主ループで共通の `now` を読み、両者のキー状態を
読み取って同一論理更新に渡し、両 `game_step` を各１回呼びます。キーのMMIO
読出しは数命令分ずれます。電気的な完全同時ラッチではなく、同一更新に属する
入力として扱う設計です。相殺・敗北は両者の更新が済んでから中央で決定します。

各プレイヤーの待ち時間は「現在のphaseと開始時刻を記憶して戻る」方式です。
消去演出の終了までCPUを待機させないため、片方の連鎖中にも相手を更新できます。
現在時刻が前回と等しい場合は再更新しません。処理が遅れて時刻が飛んだ場合も
無制限の追付きループは使わず、落下や横移動を何マスも一度に飛ばしません。
1ms単位の呼出しを目標としますが、すべての盤面で毎1ms以内に処理完了すると
いう最悪実行時間の保証ではありません。実機の操作感は別途確認対象です。

２盤面の正本はCPUデータRAMの `match.game[i].board` だけです。表示回路の
盤面は読み出し専用コピーです。Cの更新も一つのメインループが行い、割込みや
別ハードウェアエンジンとの多重書込みはありません。

## 状態遷移

試合全体は `MENU → COUNTDOWN → PLAY → RESULT`。COUNTDOWNは３秒であり、
両者を同じ更新で開始します。結果確定後は両者の操作・攻撃・降下を停止します。

```text
ACTIVE（操作）
 → 固定 → GRAVITY ↔ FALL_WAIT（全列の落下を待つ）
 → SCAN（全盤面の同色連結探索）
     ├ 消去あり → CLEAR_WAIT → 一括消去 → GRAVITY → 再SCAN
     └ 消去なし → BOUNDARY（turn_readyを一度だけ提出）
                      ↓ 共通match処理
              両者の攻撃相殺 → 受信加算 → 最大30個を降下予約
                      ├ 0個 → SPAWN
                      └ あり → GARBAGE_INSERT → GARBAGE_FALL ↔ WAIT
                                              ↓ 完了
                                   あふれ判定 → SPAWN
```

SPAWNの２マス占有判定とおじゃま完了時のあふれ判定は、両 `game_step` と同じ
論理更新の中で評価します。相殺関数の片側処理中に一方だけ次の組を出して
先に敗北確定することはありません。異なる敗北要因でも同一更新ならDRAWです。

SCANでは最大84要素のキューと配列を使います。再帰・ヒープ確保はありません。
表示待ちは複数更新へ分割します。BFS内の走査は有限84マスであり、CをCPUが
実行するため、可変長for/whileが巨大な組合せVerilog回路へ展開されることもありません。

## 座標・時間・調整箇所

x=0..5、y=0..13、index=y*6+x。y=0,1は非表示、y=2..13が表示です。
出現は軸(2,1)、子(2,0)、向き0=上、1=右、2=下、3=左。盤面値は0=空、
1..4=色、5=おじゃまです。操作中の組は固定盤面に含めません。

| 設定 | 初期値 | 調整箇所 |
|---|---:|---|
| 自動落下 | 700ms | game_init / config.auto_drop_ms |
| 高速落下 | 40ms | config.soft_drop_ms |
| 左右長押し待ち | 180ms | config.das_ms |
| 左右反復間隔 | 60ms | config.arr_ms |
| 固定後の重力１段 | 40ms | config.fall_ms |
| 消去表示待ち | 180ms | config.clear_ms |
| 開始カウントダウン | 3000ms | match_init / countdown_ms |
| おじゃま１回上限 | 30個 | MATCH_DROP_MAX |
| 予告上限 | 65535個 | MATCH_PENDING_MAX |
| 攻撃係数 | 1,2,4,8,12,18 | game_attack_factor |

同じconfigを両者に設定します。設定変更後はCを再ビルドし、MEMとbitを更新します。
固定シードは `match_start` のseed、固定出現列は `game_init` のfixed_pairs/
fixed_countで指定できます。対戦の固定出現試験ではmatch_start後、PLAY前に
両gameを同じ固定配列でgame_initし、managed=1を再設定します。通常実機版は
固定配列を使わず、元のxorshiftと継続session_rng・開始時刻を使用します。

## メモリとMEMファイル

命令ROMは16KiB、データRAMは4KiB。いずれも既存の同期BRAM対応CPUを使います。
通常の対戦ファームウェアは `.text=6408 byte`、`.rodata=8 byte`、`.bss=1132 byte`。
実行時スタック領域は別に使います。リンカは1536byteを最低予約し、コンパイラの
`.su` も保存していますが、再帰なしであることだけから最大スタック量の実測済み
とは主張しません。

`puyo_versus.mem` はRV32I機械語、`puyo_versus_data.mem` は定数を含むデータ
BRAMの初期値です。画像ファイルではありません。Cは直接Vivadoで合成せず、
GCCがRV32I/ILP32へコンパイルし、そのMEMをCPUが実行します。起動コードが
リセット時にBSSを初期化します。命令ROMをCの定数データ読出し先と誤認しない
よう、Harvard構成に合わせたリンカ・２種類のMEMを維持しています。

## MMIO（32bitアクセス）

| 番地 | R/W | 内容 |
|---|---|---|
| 0x2000 | R | P1入力 bit0左/1右/2下/3左回転/4右回転/5開始SW14 |
| 0x2004 | R | 共通ミリ秒時刻 |
| 0x2008 | R | CPU cycle |
| 0x200C | R | ID 0x50565933 |
| 0x2010 | R | VGA frameカウンタ |
| 0x2014 | R | 公開commitカウンタ |
| 0x2018 | R | P2入力 bit0..4はP1と同じ |
| 0x201C | R | bit0=SW0モード |
| 0x2020 | W | 選択プレイヤーの盤面index（0..83のみ有効） |
| 0x2024 | W | cell色bit2:0、clear bit8 |
| 0x2028 | W | 操作中のx/y/向き/２色 |
| 0x202C | W | NEXT２色 |
| 0x2030 | W | 表示スコア |
| 0x2034 | W | chain/last_chain/phase/active/busy/over/boot |
| 0x2038 | W | bit0=1で両者と試合情報を一括公開 |
| 0x203C | W | bit0=0:P1、1:P2の書込みbank選択 |
| 0x2040..0x2050 | R | cycle/instret/mem stall/load stall/flush、4byte刻み |
| 0x2060 | W | 選択プレイヤーの予告数16bit |
| 0x2064 | W | 共通 mode/state/result/countdown |
| 0x2068 | R | PS/2エラーカウンタ |

表示情報の詳細bit配置はVIDEO.md、旧CPUの0x100入力/0x104出力領域は既存の
互換領域です。MMIOはルールを決めず、ソフトウェアの計算結果だけを受け取ります。
