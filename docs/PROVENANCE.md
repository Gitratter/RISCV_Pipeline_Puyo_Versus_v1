# 出所と維持範囲

参照リポジトリは [Gitratter/RISCV_Pipeline_Puyo_v2](https://github.com/Gitratter/RISCV_Pipeline_Puyo_v2)、
確認した固定commitは `024c87fe37754b1cbe640dd2fb239cb14eda7dc0` です。
GitHubへは書き込んでいません。

このcommitのRTL12ファイルは、以前作成したローカルの
`TD8_RISCV_Puyo_SW_v5` と改行を除いて一致していました。リポジトリには元の
Cソースが含まれていなかったため、同じ作業履歴に残っているv5のCとビルド
スクリプトを開発元として使用しました。元の命令MEM・データMEMはリポジトリと
一致を確認しました（以下はCRLFを含めたGit blob ID）。

- 元puyo_game.mem：`d671837cff7e0592adfb44a33269bd05fc6f94ca`
- 元puyo_game_data.mem：`87704c184e1549a53a6db1fddd7cc6fb04e57f5b`

新しいgame.cは対戦用の追加を含むため、この元MEMと同一にはなりません。
上記IDは出発点の一致確認であり、新版bitのハッシュではありません。
配布物の各ファイルのSHA256はパッケージのmanifestを参照してください。

ライセンスは元リポジトリのMIT文面を同梱しています。公式PS/2デモのコードを
そのまま転記するのではなく、接続とプロトコルの根拠を確認したうえで新規受信器を
作成しました。公式資料と確認commitはKEYBOARD.mdにまとめています。
