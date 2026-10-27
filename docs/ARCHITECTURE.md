# WebSocketプラグインのひな型

2026-10-23。実装済みの検証用構成。ホスト受入完了を意味しない。

## 2026-10-27 通知集約・自動再接続への改訂

以下の過去記述よりSPECIFICATION.mdの「通知集中と通信復帰」を優先する。WebSocketはI/Oスレッドで受信し、完全なCompositionだけを最新1件へ集約する。応答・エラーは別枠で保持。Runtimeの評価ワーカーは新たなCompositionを受けた時だけ同期し、CompositionIndexを一度作って各Sourceへ渡す。GroupのLayer複写とactive_clipはJSON展開時に省く。

WebSocket++の同梱版ではget_buffered_amountにロックがないため、送信もI/Oスレッドへpostし、原子的な待機バイト数で1 MiBを管理する。Receiveは条件変数で待機し、Cancelで解除する。接続変更・終了時にI/Oスレッドを停止・joinして旧ソケットを閉じる。

Runtimeは接続失敗・切断・受信上限超過後にバックオフして再接続。旧pending／期限／エラー待機を破棄し、ベースラインと所有者情報は保持する。新しい完全なCompositionなしでは送信しない。新接続の旧IDが存在しなければ他IDへ復元しない。上限は1024 Source、最新Composition 64 MiB、その他待機32件／32 MiB、未完了384件。合成負荷での検証範囲と実機未検証事項はVALIDATION.mdを参照。

## 共通部分と固有部分

| 場所 | 責務 | 別プラグインへの流用 |
| --- | --- | --- |
| `source/control/ServerSettings.h` | TinyXML-2による本体server.xmlの有限サイズ読取りと検査 | 両OS共通。Macの実ファイル形式は未検証 |
| `source/control/Platform.h`とWindows.cpp／Mac.mm | Documents・ホスト・環境変数文字列・デバッグ出力・UUID | OS固有処理だけを分離 |
| `source/control/WebSocket.h/.cpp` | WebSocket++／Asio、ローカル接続、分割受信、送信、終了 | 両OS共通。Windows側もWinHTTPを廃止 |
| `source/control/Composition.h` | Composition判別、実行時キーからClipを解決、Active判定、パラメータIDの検査 | Resolume連携の共通部分 |
| `source/control/StateReconciler.h` | 現在値との差分、競合・未完了・失敗要求の抑止 | LayerやFFGLを知らない純粋ロジック |
| `source/control/Runtime.h` | DLL内の共有接続、ControlSourceの評価、通知処理、読戻し、診断 | 新しいControlSourceも同じ基盤へ登録可能 |
| `source/control/PassThrough.h` | テクスチャのパススルーと変更したGL状態の復元 | FFGL映像アダプターとして流用 |
| `definitions/LayerControl.json`／`LayerMaskControl.json` | プラグイン名、パラメータ、初期値、範囲、選択肢、制御対象property | 機能ごとの定義を編集 |
| `definitions/abi.json` | 既存パラメータ番号・型・選択肢等の互換性検査基準 | 既存の保存値の意味を維持 |
| `tools/generate_plugin.py`／`source/generated/` | JSON定義の検証と組込みヘッダー生成 | 生成ヘッダーは手編集しない |
| `source/control/DefinitionPolicy.h` | 定義に基づく設定値検査、Layer番号・実値の解決、Intent生成 | 共通の変換・制御条件を変更する場合に編集 |
| `source/control/DefinedPlugin.h` | 定義に基づくFFGL UI、パラメータ保持、識別キー、DefinedSource | 両プラグイン共通のFFGLアダプター |
| `source/plugins/LayerControl/LayerControl.cpp`／`LayerMaskControl/LayerMaskControl.cpp` | 生成定義と共通アダプターを結ぶプラグイン登録 | 新しいプラグインの登録時に追加 |

`Intent`はowner、解決済みpath、actual、desiredの4項目を持つ絶対値設定である。実値不明を送信済み値で埋めない。Runtimeは通知ごとに各SourceからIntentを取得し、同一pathを複数Sourceが要求すると、同じdesiredでも送信を止める。Applyされていて既に一致する項目もIntentへ含め、競合検出から除外しない。

## 追加・変更・削除

1. `definitions/*.json`でUIと制御対象を定義する。`property`は確認済みのComposition内のLayerプロパティに対応させる。既存の定義形式で扱えない変換は`DefinitionPolicy.h`で実装する。
2. `DefinitionPolicy.h`の`BuildIntents`でEnabled・Active等の条件を確認し、実値と指定値を持つIntentを返す。不明な値は理由を伴う例外等で送信を止める。
3. `DefinedPlugin.h`が定義からFFGL UIを構築する。UI編集は設定を保持するだけで、通信を直接呼ばない。`DefinedSource::Evaluate`が設定とrevisionを取得し、編集・破棄でrevisionを更新する。
4. JSON定義を変更したら`tools/generate_plugin.py`で生成ヘッダーを更新し、定義テストを実行する。生成ヘッダーは手編集しない。ビルド・テスト手順は[SETUP.md](SETUP.md)を参照する。
5. 新しいDLLには別のFFGL 4文字ID・表示名・ビルドターゲットを割り当て、`source/plugins/`に登録用の`.cpp`とビルド設定を追加する。
6. 変更する変換・未確認値・競合を試験し、ホスト上で通知と実値反映を確認する。共通処理の変更は両プラグインへの影響を確認する。

既存プラグインの公開済みパラメータ番号・選択肢番号は再利用しない。削除する項目は番号を予約し、保存済みCompositionを別の意味に解釈しない。今回の0.3は実機受入前の試作であり、安定版の互換性保証はまだない。

任意コマンドを投げるだけの拡張にはしない。実値の読めない操作、trigger、toggle、相対操作、非冪等な操作にはこの差分同期基盤をそのまま適用できない。それらは別の実行ポリシーと検証が必要。利用者が生のJSON／pathを入力するUIは設けない。

## 通知と寿命

- 実測・公式資料で確認した、接続直後および変更時のトップレベル`layers`付きCompositionを入力とする。未確認の`subscribe /composition`を創作しない。必要なComposition通知が来ない操作は未対応として実機で記録する。
- 個別の`parameter_update`は読戻しの確認にのみ利用する。位置更新を含め、それだけで新しい同期を開始しない。
- 1 DLLにつきRuntimeを共有。最大128インスタンス、未完了／失敗pathは合計384件、受信メッセージは16 MiBまで。
- 通知の評価結果を送信キューへ渡す直前にSourceのrevisionを再確認する。ライブラリの送信待機量は1 MiB、受信待機は32件／32 MiBに制限する。`set_sent`はキュー受理であり、送信完了やホスト反映ではない。編集・終了による古い評価候補は破棄する。すでに渡した要求の取消しは保証しない。
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

既存の同梱FFGL SDKとGLEWを使用。上流SDK commitとffgl-effectテンプレートの由来は引き続き未確定。JSONはnlohmann/json v3.11.3を`deps/nlohmann`に固定しMITライセンスを同梱。通信はWebSocket++／Asio、XML解析はTinyXML-2を使用する。依存の版・取得元は`deps/control-dependencies.json`、ライセンスは`deps/CONTROL-LICENSES.txt`を参照する。UUID生成等のOS固有処理はPlatformアダプターに分離している。プラグインからOSC／UDPは送信しない。従来のProbeは履歴・診断資料として保持し、CMakeの既定ビルドから外した。
