# WebSocketプラグインのひな型

2026-10-23。実装済みの検証用構成。ホスト受入完了を意味しない。

## 共通部分と固有部分

| 場所 | 責務 | 別プラグインへの流用 |
| --- | --- | --- |
| `source/control/ServerSettings.h` | Documents既知フォルダー・ホスト確認、本体server.xmlの有限サイズ読取りと検査 | Windows／実測Arena形式のみ |
| `source/control/WebSocket.h` | Windows WinHTTP、ローカル接続、分割受信、送信、終了 | そのまま利用。Windows以外ではこのアダプターを交換 |
| `source/control/Composition.h` | Composition判別、実行時キーからClipを解決、Active判定、パラメータIDの検査 | Resolume連携の共通部分 |
| `source/control/StateReconciler.h` | 現在値との差分、競合・未完了・失敗要求の抑止 | LayerやFFGLを知らない純粋ロジック |
| `source/control/Runtime.h` | DLL内の共有接続、ControlSourceの評価、通知処理、読戻し、診断 | 新しいControlSourceも同じ基盤へ登録可能 |
| `source/control/PassThrough.h` | テクスチャのパススルーと変更したGL状態の復元 | FFGL映像アダプターとして流用 |
| `source/plugins/LayerControl/LayerPolicy.h` | 今回のパラメータ定義、Transition／Mask選択肢、Layer番号・実値の対応 | 新用途ではここを交換 |
| `source/plugins/LayerControl/LayerControl.h/.cpp` | FFGL UI、パラメータ保持、識別キー、上記を結ぶSource | 新用途に合わせてUIとSourceを変更 |

`Intent`はowner、解決済みpath、actual、desiredの4項目を持つ絶対値設定である。実値不明を送信済み値で埋めない。Runtimeは通知ごとに各SourceからIntentを取得し、同一pathを複数Sourceが要求すると、同じdesiredでも送信を止める。Applyされていて既に一致する項目もIntentへ含め、競合検出から除外しない。

## 追加・変更・削除

1. `LayerPolicy.h`のような固有ポリシーで、確認済みのComposition構造から実値とIDを解決する。
2. Apply条件に応じてIntentを返す。切替・削除・不明状態は空の結果、または理由を伴う例外で送信を止める。
3. FFGL側に意味のあるUIを設ける。UI編集は設定を保持するだけで、通信を直接呼ばない。
4. `ControlSource::Evaluate`で設定のスナップショットとrevisionを取得する。編集・破棄でrevisionを更新する。
5. 新しいDLLには別のFFGL 4文字ID・表示名・ビルドターゲットを割り当てる。
6. 変更するポリシーの変換・未確認値・競合を試験し、ホスト上で通知と実値反映を確認する。

既存プラグインの公開済みパラメータ番号・選択肢番号は再利用しない。削除する項目は番号を予約し、保存済みCompositionを別の意味に解釈しない。今回の0.3は実機受入前の試作であり、安定版の互換性保証はまだない。

任意コマンドを投げるだけの拡張にはしない。実値の読めない操作、trigger、toggle、相対操作、非冪等な操作にはこの差分同期基盤をそのまま適用できない。それらは別の実行ポリシーと検証が必要。利用者が生のJSON／pathを入力するUIは設けない。

## 通知と寿命

- 実測・公式資料で確認した、接続直後および変更時のトップレベル`layers`付きCompositionを入力とする。未確認の`subscribe /composition`を創作しない。必要なComposition通知が来ない操作は未対応として実機で記録する。
- 個別の`parameter_update`は読戻しの確認にのみ利用する。位置更新を含め、それだけで新しい同期を開始しない。
- 1 DLLにつきRuntimeを共有。最大128インスタンス、未完了／失敗pathは合計384件、受信メッセージは16 MiBまで。
- 送信キューは持たない。通知の評価結果だけをその場で送信し、直前にSourceのrevisionを再確認する。編集・終了による古い候補は破棄する。
- 送信済み要求は取り消せない。サーバー側で起きた切替をまだ受信していない間の送信や、評価と送信の間のホスト変更を原子的には防げない。高速切替はAT-13で検証する。
- `set`送信後にWebSocketの`get`を送り、`parameter_set`応答とは別に値を確認する。不一致の読戻しで同じ要求を再送しない。
- 5秒を過ぎた未完了要求は、次の受信時にタイムアウトとして隔離する。受信が途絶えれば期限の記録も遅れる。タイムアウトを契機にset／getを再送しない。
- 接続失敗・切断・不正JSONではワーカーを停止し、自動再接続・自動再送は行わない。全インスタンスの解放後の再生成、またはArena再起動で新しい接続を開始する。
- 最後のインスタンスが終了すると受信をキャンセルし、ワーカーをjoinしてハンドルを解放する。描画処理は通信・ログ・設定mutexに触れない。

## Clip識別の設計案と限界

インスタンス生成時にGUIDを作り、FFGLの`Instance Key`テキスト値として公開する。復元・複製された旧キーは採用せず、FFGLのVALUEイベントでホストに現在値の再取得を要求する。保存されたキーは実行時の同一性の根拠にしない。利用者がキーを編集する必要はない。

Composition全体でキーが1件だけ存在し、その場所がClipのvideo.effects.paramsにあり、Clip IDが存在する場合だけ対応付ける。キー不在・重複・Layer／Compositionへの配置では送らない。効果名やClip名だけで同定しない。

SDK上にVALUEイベントが存在することは確認済み。Arenaがテキスト値の変更をCompositionへ反映すること、複製・Preset・再読込での一意性は未確認。成立しなければ本方式のまま製品利用できない。`Instance Key`は当面、診断用にUIで見える状態にしている。

## 依存関係

既存の同梱FFGL SDKとGLEWを使用。上流SDK commitとffgl-effectテンプレートの由来は引き続き未確定。JSONはnlohmann/json v3.11.3を`deps/nlohmann`に固定しMITライセンスを同梱。WinHTTP／GUID生成はWindows SDKを使用する。プラグインからOSC／UDPは送信しない。従来のProbeは履歴・診断資料として保持し、CMakeの既定ビルドから外した。
