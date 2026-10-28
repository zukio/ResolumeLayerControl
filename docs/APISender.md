# APISender

APISenderはEffect ClipのActive／非Active切替時に、入力したJSONを同じResolume本体のWebSocketへ送る独立したFFGL Effectです。FFGL IDは`RAPI`。Layer ControlとLayer Mask Controlの公開パラメータ・IDは変更しません。

## パラメータ

| 番号 | 名前 | 内容 |
| --- | --- | --- |
| 0 | Enabled | 初期値ON。OFFへの切替は非Activeとして扱う |
| 1 | Active Command | Active時に送るJSON。初期値は空欄 |
| 2 | Inactive Command | 非Active時に送るJSON。初期値は空欄 |
| 3 | Instance Key | Clip対応用。通常は編集しない |

単一のJSONオブジェクト、またはオブジェクトの配列を入力します。空欄・空白・空配列は送信なしです。各オブジェクトは空でない文字列の`action`が必要です。後述のpostのbody入力補助を除き、フィールドはResolumeへそのまま渡し、Target Layerへの変換や対象制限は行いません。例えば実測済みのDurationパスへの設定形式は次です。Layer番号と値は実際の構成に合わせて変更してください。

```json
{"action":"set","parameter":"/composition/layers/3/transition/duration","value":2.5}
```

Column 1を起動する場合は次をActive Commandに入力します。HTTPヘッダーやmethodは入力しません。2026-10-28、利用者からこの形式で「動作したみたい」との実機報告あり。先に案内した`post`＋`path`のColumn connect形式では対象環境で`Route not matched`が記録されました。

```json
{"action":"trigger","parameter":"/composition/columns/1/connect"}
```

まずInactive Commandを空欄にし、Effect Clipを非ActiveからActiveへ切り替えて確認します。Active中の編集だけでは再送しません。Column起動で自身が非Activeになる場合、設定済みのInactive Commandも送信対象になるため、連鎖動作は構成ごとに確認してください。

`get`の取得結果を表示・転送・後続コマンドへ渡す機能はありません。`unsubscribe`はパラメータ変更通知の購読解除であり、Clip停止やColumn起動ではありません。形式の参照：[公式WebSocket API](https://resolume.com/support/en/websocket-api)。

複数コマンドは次の形式です。配列順で個別のWebSocketメッセージとしてキューへ渡します。対象Layer番号は実際の構成に合わせます。

```json
[
  {"action":"set","parameter":"/composition/layers/3/transition/duration","value":2.5},
  {"action":"set","parameter":"/composition/layers/4/transition/duration","value":1.0}
]
```

Inactive Commandも同じ形式で、明示的に戻したい値を入力します。変更前の値の保存・自動復元はしません。by-id指定も入力どおりに送るため、Compositionを変更・再読込した際のIDの有効性は利用者側で確認します。

入力は1プロパティにつきUTF-8で64 KiB、配列は256コマンドまでです。不正JSON・配列内の非オブジェクト・不正action・上限超過は編集全体を拒否し、直前の有効な設定を保ちます。FFGLには失敗を返しますが、ホスト上のエラー表示は未検証です。APIとしてのパス・action・値の適否はサーバーが判断します。

## postのbody入力補助

`action`が`post`で、`body`が配列またはオブジェクトなら、送信用に内部でJSON文字列へ変換します。文字列は二重変換せずそのまま送ります。bodyなし・数値・boolean・nullや他actionのフィールドは変更しません。入力・保存テキストは元の形を保持し、Active／Inactive双方とコマンド配列に適用します。

```json
{"action":"post","id":"swap-test","path":"/composition/clips/swap","body":["/composition/layers/1/clips/1","/composition/layers/1/clips/3"]}
```

この入力はbodyだけをJSON文字列化して送ります。利用者環境では直接配列を送ると`value is not std::string`となり、手動で文字列化したswapは動作したとの報告あり。入力補助を追加したDLLでの実機確認は別途必要です。

## 送信条件

- Composition通知内のClipの`Connected`／`Connected & previewing`をActiveとし、Effect BypassがfalseかつEnabledがONの場合にActive Commandを1回送信します。
- Activeから非Active、Enabled OFF、Effect Bypass ONへの切替時にInactive Commandを1回送信します。
- 初回通知・再接続後の最初の通知で既にActiveならActive Commandを1回送信。最初から非Activeなら送信しません。初回通知前にパラメータが未復元の場合は、その後の観測された状態変化に従います。
- Active中のコマンド編集・同じClipの再トリガー・自己通知・手動での対象値変更だけでは再送しません。次の状態切替から新しいコマンドが使われます。
- 対応キーが不在／重複の場合は状態を確定せず保留し、再び一意に解決できる通知を待ちます。
- Composition通知は既存方式で最新1件へ集約します。途中の全遷移を保証しません。個別のparameter_updateや再生位置通知だけでは送信しません。
- 複数インスタンスはそれぞれの状態変化で送信します。コマンド同士の対象競合の調停・値の同期は行いません。

キュー受理はサーバー実行成功を意味しません。コマンドの自動get・実値比較・自動復元・エラー後の自動再送は行いません。バッチ途中のキュー拒否・終了・設定revision変更では残りを送らず、受理済み分の取消やバッチ全体の再送はしません。接続が切れた場合は既存基盤で再接続し、新しいCompositionを待ってActive Commandを送るため、非冪等コマンドの重複実行に注意してください。

`RLC_WS_LOG`の`api_commands_queued`は、owner・active・accepted・totalを記録します。acceptedがtotalより小さい場合は全件が受理されていません。コマンド本文はこのログへ追加しません。サーバーエラーは既存の`server_error`ログに記録します。接続設定・接続数は既存と同じDLL単位の共有Runtimeで、別PCへは接続しません。

Arena終了や最後のインスタンス削除時のInactive Commandは保証しません。明示的にClipを非Activeにして送信を確認してから保存・終了してください。Composition保存・再読込・Preset・Clip複製でのテキスト復元とキー対応は実機試験が必要です。

## ビルドと確認

Windows／macOSは既存のCMake設定で`ffgl-plugin-api-sender`をビルドします。Windowsの出力は`binaries/x64/Release/APISender.dll`、macOSは`build-control-plugins/source/plugins/APISender/APISender.bundle`です。Visual StudioのeffectsにもAPISenderを追加しました。導入はDLLまたはbundleのみで、隣接JSONは不要です。

```sh
cmake --build build-control-plugins --config Release --target ffgl-plugin-api-sender
```

APISenderはLayerのpropertyを定義する生成スキーマの対象にせず、専用の小さなFFGLアダプターと`APISenderSource.h`にテキスト・実行方針を実装しています。共通の通信・Clip解決・パススルーは既存を使用し、既存2プラグインの定義・生成ヘッダーは変更しません。

CMakeテストには`api_sender`を追加しました。Linux Cloudでは本体のWindows／macOSアダプターを使えないため、テスト専用アダプターを明示して共通通信を検証できます。これはLinux版Pluginの追加ではありません。

```sh
cmake -S tests -B work/cloud-tests -DRLC_CLOUD_TEST_PLATFORM=ON
cmake --build work/cloud-tests
ctest --test-dir work/cloud-tests --output-on-failure
```

実機では次を確認してください。

1. APISenderを専用Effect Clipに配置し、有効なActive／Inactive JSONを入力する。空欄・不正入力時の表示も確認する。
2. 非Active起動で無送信、Active化で1回、再トリガー・対象値の手動変更では無送信、非Active化で1回を確認する。
3. Enabled・Effect Bypass・Preview切替と、Active中に再接続した場合を確認する。
4. 配列の実行順序とサーバーエラーを確認する。別のAPI対象でも送信と反映を個別に確認する。
5. Composition保存・再読込、Preset再適用、Clip複製、映像パススルーを確認する。Plugin削除・Arena終了での送信は受入条件にしない。

Cloudの模擬通信試験は、Resolumeによる実行・UI保存互換性・Windows DLL／Mac bundleのロード成功を証明するものではありません。
