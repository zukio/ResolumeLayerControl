# 検証記録

## APISender post body入力補助（2026-10-28）

- 利用者報告：swapのbodyを配列で送ると失敗し、JSON文字列として送ると動作。ログ`logs/control-20261028-171832-890.jsonl`には`error reading field "body": value is not std::string`。要求によりpostのbodyが配列／オブジェクトの場合だけ送信用に文字列化し、文字列・他型・他action・保存テキストは保持する。
- Windowsで`cmake --build build-control-tests --config Release --target api_sender_smoke runtime_smoke`成功。初回はWindows SDK参照先へのサンドボックスアクセス拒否となり、昇格実行で成功。
- `ctest --test-dir build-control-tests -C Release -R "^(api_sender|websocket_runtime)$" --output-on-failure`は2/2成功。模擬WebSocket相手でActive／Inactive双方の配列・入れ子オブジェクトの文字列化、引用符・改行保持、文字列の二重変換なし、bodyなし・null・数値・他actionの非変換を確認。入力テキスト保持も確認。
- `cmake --build build-control-plugins --config Release --target ffgl-plugin-api-sender`成功。出力`binaries/x64/Release/APISender.dll`。稼働中Arenaへの導入・再起動・修正版DLLでの実機試験は未実施。
- 実機再現：Arena終了後に修正版DLLを導入し再起動。Active CommandへAPISender.mdの配列bodyのswapを入力、Inactiveは空欄、非ActiveからActiveにしてClip 1と3の配置交換およびエラーログ不在を確認する。既存の文字列body形式でも同じ操作を別途確認する。

## APISender Column起動の利用者報告（2026-10-28）

- 対象：利用者のWindows／Resolume Arena環境。今回の製品版番号・DLLコミットは未照合。APISenderはColumn 7に配置との申告。
- `logs/control-20261028-171832-890.jsonl`ではActive／Inactive各1件のキュー受理と、それぞれ直後の`Route not matched`を確認。`docs/evidence/ws-position-01.jsonl`内のCompositionではActive Commandが`{"action":"post","path":"/composition/columns/1/connect"}`、Inactive CommandがColumn 3への同形式。受信ログ7行目にもルート不一致あり。ただし受信ログ単独では送信との対応は確定できない。
- 再現案内：Active Commandを`{"action":"trigger","parameter":"/composition/columns/1/connect"}`に変更、Inactive Commandを空欄にし、一度非ActiveにしてからColumn 7を起動する。
- この案内後、利用者から「動作したみたい」との報告あり。Column起動の暫定的な実機報告として記録する。成功時の送受信ログ・最終入力・映像は未照合。Inactive側のColumn起動、連鎖、保存互換性などの全面合格にはしない。
- 説明書にtrigger形式とGET結果の表示・転送がない点を追記。ソース変更なし、文書のみのため自動テストは未実施。

## APISender実装・Cloud検証（2026-10-28）

- GitHub連携でmasterとDevが同じ`88fa376df23590eb210a8205a486dc03796f82eb`であることを確認し、同コミットからローカルDevへ切替。既存Layer系と独立したAPISender（FFGL ID RAPI）を追加。仕様と実機再現は[APISender.md](APISender.md)。
- 環境：Linux Cloud、g++ C++17、Python 3。Windows／Mac用Platformをテスト専用アダプターに置き換え、製品と同じWebSocket.cpp／TinyXML-2／RuntimeをローカルRFC6455サーバーに接続。Linuxの製品対応を追加したものではない。サンドボックスではsocket生成が拒否されたため、通信テストは許可された昇格実行でループバックを使用した。
- APISender専用試験：`python3 tests/test_api_sender.py work/bin/api_sender_smoke`成功。起動時Active／非Active、配列順序、同じ通知・再トリガー相当で無再送、Preview、Bypass、Enabled、キー不在・重複保留、編集中は再送しないこと、空コマンド、サーバーエラー後に再送しないこと、再接続後のComposition待機とActive送信を確認。模擬triggerは任意JSONの転送を試すもので、そのAPI actionのResolume実機対応を検証したものではない。
- 入力試験：不正JSON、非オブジェクト、action不正を拒否し旧設定を保持。64 KiBテキスト上限、256コマンド上限、空白・空配列を確認。
- 回帰試験：control_tests 1,860項目成功、test_definitions.py 5件成功、server_settings_testsは失敗0。test_websocket_transport.py／test_websocket_runtime.py／test_websocket_recovery.py成功。既存の差分set・読戻し・自己通知・復元・ログ失敗・再接続・通知集約・大容量通知・位置通知抑止を確認。
- FFGLアダプターは同梱SDK／GLEWヘッダーを用いたg++の構文チェックに成功。追加の`-Wall -Wextra -Werror`では既存RuntimeのIntent集約初期化とSDKの未使用引数の警告がエラーとなった。標準の構文チェックは成功しており、この警告のために既存SDKは変更していない。
- Windowsソリューション・vcxproj（XML解析成功）、CMakeターゲット、Visual Studio委譲先許可リスト、macOS CIの対象を追加。この環境にcmakeがないためCMake／CTestの実行は未実施。g++で各実行ファイルを直接ビルドし、上記テストを実行した。Windows DLL／Mac bundle生成、Arenaロード、FFGLテキスト編集・保存／再読込・Preset・複製、映像パススルー、API実反映は未実施。

直接ビルドの再現（リポジトリルート、Linuxテスト専用）：

```sh
mkdir -p work/bin
g++ -std=c++17 -O1 -pthread -DASIO_STANDALONE -D_WEBSOCKETPP_CPP11_STL_ -Ideps -Ideps/tinyxml2 -Ideps/asio/asio/include -Ideps/websocketpp -c source/control/WebSocket.cpp -o work/websocket.o
g++ -std=c++17 -O1 -pthread -DRLC_RUNTIME_TESTING -DASIO_STANDALONE -D_WEBSOCKETPP_CPP11_STL_ -Ideps -Ideps/tinyxml2 -Ideps/asio/asio/include -Ideps/websocketpp tests/api_sender_smoke.cpp tests/platform_testing.cpp work/websocket.o deps/tinyxml2/tinyxml2.cpp -o work/bin/api_sender_smoke
python3 tests/test_api_sender.py work/bin/api_sender_smoke
```


## 終了遅延調査・保存時の元値の扱い（2026-10-27）

- 要求：直近のRuntime／WebSocket変更後にResolumeの終了が遅いと感じたため、プラグインの関与を確認する。あわせて、Resolume再起動でベースライン（メモリのみ）が失われた際に、前回の設定を元の値と誤認する可能性を調べる。
- 実装：`Runtime.h`へ終了計測ログを追加（`runtime_shutdown`：終了時のワーカー段階`phase_at_stop`＝connect／receive／process／socket_shutdown／reconnect_wait、`cancel_us`、`join_us`。`socket_shutdown`：`socket.Shutdown()`の所要時間）。制御仕様・終了処理は変更していない。`RLC_WS_LOG`指定時だけファイルへ出力。
- 環境：Windows、Arena（インストール先`C:\Program Files\Resolume Arena\Arena.exe`）。ログ付き起動は`tools/start-arena-with-log.ps1`。Release DLLを`Extra Effects`へ導入。Arena製品版の再確認はしていない。
- 終了遅延の結果：接続前（connect）終了の`join_us`は259〜536 µs、接続済み（receive）終了は786〜4,347 µs。いずれも1回の実機終了での計測値で、体感できる遅延ではない。`Extra Effects`のDLLを削除した状態でも終了が遅く、プラグインのログは出なかった（利用者報告）。したがって終了遅延はプラグイン起因ではない。
- 補足：ArenaはDLLを未使用のプロジェクトでも、起動時にインスタンスを一時生成するとみられる。生成された共有Runtimeはconnect段階で即座に終了する（LayerControl／LayerMaskControlで各1件。コード・ログからの推定であり、Resolume側の動作は未確認）。
- 保存と元の値（利用者の実機確認）：①保存せず終了すると、元のLayer設定が残る。②非Activeへの復元完了後に保存すると、元のLayer設定が保存される。③Effect ClipがActiveの間に保存すると、プラグインが変更した値が保存される。したがって、問題の本質はベースライン消失ではなく、一時的な設定変更がCompositionへ保存されることである。ベースラインはRuntime内メモリのみ（PRE-06）。Active中の保存値が次回読込時にLayerの現在値となり、その値と指定値が一致すれば差分がなく、ベースラインは作られない。
- 運用上の注意：Compositionを保存する前に、Effect Clipを非Activeにして復元を完了させる（復帰用Columnへ切り替え、読戻しで元の値を確認してから保存する）。最後のインスタンス削除時（Runtime破棄）は復元されない。Active中の自動保存・終了時保存の挙動は未検証。
- 終了時の復元は成立しない（実機結果）：`~Runtime`時に保持中のベースラインへ`set`／`get`を送る試験用実装（環境変数`RLC_SHUTDOWN_PROBE_MS`）を作成し、Arena通常終了（Quit!ダイアログから終了）で試した。Effect ClipをActiveにしLayer値の反映と読戻しを確認した状態で終了したが、`runtime_shutdown`・`probe_*`・`disconnected`は出力されなかった。したがってArenaの通常終了では`~Runtime`が実行されていないか、実行前にプロセスが終了している。終了時にWebserverが応答するかは、プラグインが実行されないため確認できていない。ArenaがFFGLインスタンス・DLLを解放しない詳細な原因は未調査。この結果を受け、プローブ実装は削除した（終了計測ログ`runtime_shutdown`／`socket_shutdown`は残す）。`disconnect-all`などの終了時送信も同じ理由で成立しない。タスクマネージャー終了、メニューExit、保存して終了の各経路は未試験。
- 決定（利用者）：保存前の運用を仕様の前提とする。Compositionを保存する前に、復帰用ColumnへEffect Clipを非Activeにして切り替え、読戻しで元の値を確認してから保存する。終了時の自動復元は実装しない。
- 残る確認事項（未実施）：(1)通常のColumn切替で、ベースラインを確実に復元すること。特にA→Bの高速切替と、通信障害時の復元。(2)Composition保存前に、復元の読戻しを確認する運用の有効性。(3)大容量Composition処理中（process）と再接続待機中（reconnect_wait）の終了時間。(4)Active中の自動保存・Arena終了時の保存確認時の保存内容。

## 通知集約・自動再接続（2026-10-27）

- 要求：30 Layer × 30 Column程度の実用構成を考慮し、通知集中で永久停止しない。利用者が対策1〜5（速度、最新状態集約、再接続、自動復帰、容量検証）を指示。
- 実装：I/Oスレッドで受信を継続し、待機中の完全なCompositionは最新1件へ集約。読戻し・エラー等の順序は維持。JSON展開時にGroupのlayers／Layerのactive_clip複写を省き、Clip対応を1通知1索引へ変更。gate／compareは変化時だけログ出力。Runtime ID、解析／評価時間、受信・集約数・ピーク待機量を記録。
- 復帰：接続失敗・切断・受信待機超過後は250 ms〜最大5秒のバックオフで再接続し、設定を再読取り。旧pending／期限／待機を破棄し、新Compositionまではsetしない。ベースラインは保持し、正規Layerの同じIDを確認できる時だけ復元。終了は条件変数を解除し、I/Oと評価の両スレッドをjoinする。
- 容量：128 Sourceでは900配置を受けられないため1024へ変更。登録1024件、1025件目の拒否、解放後の再登録を模擬試験。通信は最新Composition最大64 MiB、その他待機32件／32 MiB、送信待機1 MiB。未完了384件は維持。送信受付は実送信／読戻しの成功とは区別する。
- 30×30索引試験：900個の一意キー・30 Active・Group複写を合成。全Clip解決とActive評価、重複キー拒否を確認。代表測定で新索引＋900 Source評価4,327 µs、従来の900回ResolveClipのみ598,323 µs。この速度比較は小さな合成Fixtureであり、実機の総処理時間ではない。
- 通信負荷：保存済みArena 7.27.1のClipメタデータを複製し30×30＋Group複写を作成。1通知24,429,485 bytes（約23.3 MiB）。旧16 MiB制限では入らないため64 MiBへ変更した根拠とした。4通知（末尾の新指定を含む）送信から最新指定のset/get受信まで代表4.625秒、模擬RuntimeプロセスのピークWorking Set 174.5 MiB。代表最終通知のJSON解析797,364 µs、評価94,040 µs。JSON解析はなお重く、映像Transitionへ間に合う結果とは扱わない。テストは実キャプチャの構造を利用した合成であり、実機900 Clipを再生したものではない。
- 回帰試験：`websocket_recovery`に起動時503拒否から復帰、切断時pending破棄、新しい指定の反映、接続をまたぐ元値への復元、受信待機超過から再接続、評価停止中の150件の古い非Active通知の集約、応答／エラー保持、大容量通知後の新しい指定への反映を追加。既存`tests/test_websocket_runtime.py`（CTest経由）の読戻し・自己通知収束・項目別復帰・ログ失敗試験も継続。
- 再生位置通知対策：エラーのない`parameter_update`の`/transport/position`を受信段階で除外。評価停止中に500件を集中させても応答待機を圧迫せず、最新状態を処理できることを回帰試験へ追加した。エラー付き通知は除外しない。
- ビルド／試験：Windows、VS2022 MSBuild 17.14.40、SDK 10.0.28000.0。`cmake --build build-control-tests --config Release`、`ctest --test-dir build-control-tests -C Release -V`、両プラグインReleaseビルドを実施。最新の最終確認ログは`evidence/recovery-tests-20261027.txt`へ保存する。通常サンドボックスではSDK参照が拒否されるためビルドのみ昇格実行。
- 未実施：新DLLをArenaへ導入した試験、実機30×30・両DLL同時負荷・長時間運用、切断前後の映像とMaskの露出、AT-02、Mac。稼働中Arenaの再起動・DLL置換・Composition変更は今回行っていない。試験用サーバーのget/set成功を実機成功としない。
- 実機再現手順：保存・終了後にbinaries/x64/Releaseの両DLLを導入してログ付き起動。両PluginのActive適用→Layer本体を手動変更→次通知で再適用→非Activeで元値へ復元を確認。試験用CompositionでWebserverを一時無効／再有効化し、reconnect_wait→connected→新通知→差分set→readbackを確認。30×30では実通知bytes、parse_us、evaluation_us、peak_bytes、replaced、メモリを記録し、映像適用は別判定。恒常的に64 MiB超過する通知や負荷による長い遅延は未解決となり得るため、ログを根拠に追加評価する。

## ログ付き再起動後、Maskのみ停止（2026-10-27）

- 利用者報告：再起動前は両方が動かず、Layer本体の手動変更が契機の可能性。ログ付き再起動後はControlが動作しMaskのみ不良。今回の取得は読取りのみで、再生・値変更は利用者操作。
- Arena PID 45392、20:29:56起動。保存設定の製品情報はArena 7.27.1 rev15990。ログ`logs/control-20261027-202956-420.jsonl`の調査時点のコピーを`evidence/control-mask-stall-20261027.jsonl`へ保存。
- 20:30:05.286 JSTに`WebSocket connection failed or cancelled`、20:30:08.637に`WebSocket receive queue limit`を記録。Maskのgateは20:30:08.311のdisabledが最後で、その後はLayer Controlのgate／set_sent／restore_sent／readbackが継続。Controlの値適用と復元の読戻しは記録あり。映像への反映は未判定。
- 現在のMaskはLayer 1／Column 8、Connected、Enabled=true、Effect Bypass=false、Target=1、One Below。対象Layer 1のmaskmodeはDisabled。現在のInstance Keyはeb4ac378-f295-46a7-8c17-8ca1206ad5f7で、起動時ログにあるMask ownerとは異なる。現在キーのgate記録は取得ログにない。
- 現行WebSocket.cppは受信待機32件または32 MiB超過でfailureを設定して終了し、Runtimeは再接続しない。ログ上、この停止経路の発生を確認。Mask停止との関連は、その後のMaskログ消失とControl継続から強く疑われるが、failure行にはDLL／Runtime識別子がなく直接の帰属は未確定。件数・バイト数どちらの超過か、通知の内容・集中原因も現行ログでは特定できない。手動変更が直接の原因とはまだ断定しない。
- 次の実装修正候補：failureにRuntime識別と受信件数・バイト数・メッセージ種別を残し、通知集中時の処理と停止後の復帰を検証する。無条件の上限拡大や通知破棄を修正済み扱いにしない。本調査では製品コード・DLLを変更していない。

## 使用中の停止調査（2026-10-27、19:05起動セッション）

- 読取りのみ実施。Arena PID 45252、起動時刻19:05:24。Composition Example、4 Layer／10 Column／1 Group。再生・設定変更・保存・再起動は行っていない。
- Layer 3／Column 5のLayer Controlが再生中、Enabled=true、Target Layer=1、Transition Blend=Cube、Duration=0。一方、Target Layer 1はAlpha／1秒。不一致を観測したが、通知前後の時系列とEffect Bypassの実値は未取得で、原因は未確定。Layer 1／Column 8のLayer Mask ControlはEnabled=false。
- Arenaがロードした両DLLのパスを確認。配置ファイルとbinaries/x64/ReleaseのSHA-256は一致。LayerControl: E477ED04001E57CAFEA081AEB43FE69EE53E44966D8006F3ADD5B3FD0141D6CF、LayerMaskControl: 4DFFC1DF660CE1909AA84505B5A9B21BA2AD83D23E51FA1B18916A144CB53B26。メモリ上のバイナリハッシュは未取得。
- User／Machineおよび調査シェルのRLC_WS_LOGは未設定。起動済みArena固有の環境変数は未確認。docs/evidenceで確認できたプラグインログは10月23日のみで、今回の停止に対応する内部ログは取得できていない。
- 現行Runtimeは切断／接続失敗後に自動再接続しない。停止原因の候補であり、今回これが発生した証拠はない。
- 再現用にtools/start-arena-with-log.ps1を追加。利用者が保存・終了した後に実行すると、logs/control-日時.jsonlを指定してArenaを起動する。稼働中は起動を拒否する。スクリプト構文のみ確認、実起動は未実施。成功した操作から停止までを短時間記録し、failure／disconnected／gate／compare／readbackを照合する。ファイルログはRuntimeごとに16 MiBで追記停止するため、無期限記録には使わない。映像適用・AT試験は未実施。

## 制御を止める照合・制限の棚卸し（2026-10-27）

利用者依頼により共通コード・定義・既存テスト・履歴を静的調査した。[棚卸し詳細](VALIDATION_GATES_AUDIT.md)に優先見直し9項目、互換範囲・容量等の追加条件、維持すべき確認、復元上の注意を記録。Groupの全内容一致、Source単位エラーの全復元への波及、失敗pathの継続隔離、読戻しより先の期限判定、任意ログ失敗による停止を優先候補とした。設計案であり、今回はソース・DLL・実機を変更していない。実機原因の確定や修正後の合格を意味しない。

## 手動変更後の再適用停止・読戻しID照合修正（2026-10-27）

- 最新報告：Layer Control／Layer Mask ControlでPlugin適用後、Resolume側で通常操作により値を変更するとPluginが再適用しない。先の「フォーカスが外れたときの復元」だけの問題としては扱わない。
- 実機再現：Windows／Arena 7.27.1 rev15990、Example、4 Layer／10 Column／1 Group、port 8080。Layer 1・Column 8のLayer Mask ControlはEnabled、Target 1、All Below、Connected、bypassed=false。Layer 4・Column 8のLayer ControlもEnabled、Target 1、Cube、Connected、bypassed=false。対象Layer 1はDisabled／Addのままで、両Clipを再トリガーしても指定値にならなかった。MaskをOne Belowへ手動設定して再トリガーしてもOne Belowのまま。試験後は元のDisabledへ戻した。Compositionの保存・再起動・構造変更は行っていない。
- 証跡：`evidence/ws-manual-before-20261027.json`、`evidence/ws-manual-triggered-20261027.json`。`ws-manual-retrigger-20261027.json`は実トリガー前の追加スナップショットであり、成功後の証跡ではない。使用中のExtra Effects DLLは2026-10-26の版で、今回ビルドした修正版ではない。
- プロトコル実測：`evidence/ws-readback-wire-20261027.json`。`/parameter/by-id/1793073084789`へのgetは`id:1793073084789`と`path:/composition/layers/1/maskmode`を返した。Transition BlendとDurationも同様にby-id以外のpathで返る。従来Runtimeはpath完全一致で未完了要求を管理していたため、この正しい読戻しを無視する。後続Compositionで一致を確認できないまま5秒を超えるとpathが隔離され、以後の適用・復元を抑止する。
- 修正：共通Runtimeで応答の正の整数idを`/parameter/by-id/<id>`に正規化して読戻し・エラーを対応付ける。idのない従来のby-id応答も受け付ける。set応答だけでは成功にせず、get／updateの値一致を確認する。無条件再送・ポーリング・タイムアウト解除は追加しない。
- 回帰検証：実測と同じid＋Composition path形式へテストを変更すると、修正前は非Active復元待ちでタイムアウト。修正後は`ctest --test-dir build-control-tests -C Release --output-on-failure`で5/5合格（`tests/test_websocket_runtime.py`を含む）。手動変更相当の最新実値との差分再適用、異なるIDの応答拒否、get読戻し、非Active復元、updateでの復元読戻し、自己通知時の無送信を確認した。
- 判定の限界：応答の取りこぼしは実測と回帰試験で確定したコード不具合。ただし既に起動していたDLLの内部ログは未取得で、その停止がreadback_timeout由来かは断定しない。修正DLLの再読込み後の実機再試験は未実施。Solo／31 Layer・複数Group／Arena 7.28.1／同Column映像へのTransition適用は未確認。
- ビルド：`cmake --build build-control-tests --config Release`、`cmake --build build-control-plugins --config Release --target ffgl-plugin-layer-control ffgl-plugin-layer-mask-control`成功。修正DLLは`binaries/x64/Release/LayerControl.dll`と`LayerMaskControl.dll`。Extra Effectsへの置換は未実施。
- 再試験手順：修正DLLをSETUP.mdに従い導入し、ログ付きでArenaを起動。両PluginについてActive時の指定値適用→Target Layer本体を別値へ変更→ClipまたはColumn再トリガー→指定値の読戻し→非Active化→管理開始前の値の読戻しを確認する。5秒以上経過してから再度手動変更して再適用を確認し、readback_timeoutがないことをログで照合する。映像適用は値の合否と分ける。

## Solo調査の引継ぎ・大規模CompositionでのMask固定報告（2026-10-27）

- 利用者提供の調査テキスト：他Layer（1・3）がSolo、Layer Control配置Layer 4は非Soloでも、Effect ClipはConnected、Effect bypassed=false、一意キーの条件を満たしていたとの報告。これは別調査の観測報告であり、今回のエージェントによる再現ではない。Solo前後の設定変更・読戻し・通知列は未確認。コードはsoloを直接参照せず、同期は描画コールバックから独立している。ただしホストがFFGLインスタンスを維持し、通知を継続することまで保証した結果ではない。
- 利用者提供の別環境の写真：下部にResolume Arena 7.28.1、Layer 31までの表示、OUTPUT／MAIN MIX／CAM LAYOUT／INPUTのGroup見出しを確認。Mask ModeをOne Below／All Belowへ変更後に他へ切り替わらないとの第三者報告。Layer本体の数値／選択値が固定なのか、映像結果だけなのか、操作対象・実際のDLL版・Composition JSON・ログは未取得。Solo事象との共通原因は未確定。
- コード確認：Runtimeは送信から5秒経過した未確認pathを次の受信時にFailへ移す。StateReconcilerのfailed集合は接続中保持され、同じpathへの新しい指定値・復元もPlanから除外される。遅れて正しい読戻しが届いてもpendingが消えているため解除されない。タイムアウト判定が受信メッセージの読戻し処理より先なので、5秒後の一致通知そのものでも隔離が先行する。これは停止し得る実装上の経路であり、利用者の発生原因と断定しない。
- その他の停止経路：同じpathを複数Active Sourceが要求すると同値でも競合として抑止。Instance Keyの不在／重複でも抑止。Group側LayerとトップレベルLayerのJSON全体（動的値を含む）の不一致でinvalid_stateとなり、その通知で復元も抑止され得る。受信1メッセージ16 MiB・受信待機32件／32 MiB・送信待機1 MiB超過や切断は停止し、自動再接続しない。31 Layerという数だけで拒否する条件はない。128はLayer数でなく登録FFGLインスタンス数の上限。
- 検証：既存の`ctest --test-dir build-control-tests -C Release --output-on-failure`を実行し5/5合格。失敗path隔離・競合・Group不整合拒否を含む既存ロジック試験であり、31 Layer／複数Group環境の再現試験ではない。今回は実装・DLLを変更していない。
- 次の再現記録：対象環境で使用中DLLのSHA-256、Arena版、再現操作（同一Effect ClipのMask Mode編集／別Clip再生／Target変更）、配置Layer・Target Layer・Group所属、変更前後のLayer本体Mask Modeを記録する。RLC_WS_LOGを設定して起動し、最初に成功した操作から固定までのログと各時点のWebSocket Compositionを取得する。readback_timeout／server_error／invalid_state／failure／disconnected、およびgate・compare・set_sent・restore_sent・readbackを照合する。競合には複数ownerが同一pathを要求していないかも見る。ログは16 MiBで追記停止するため、再現用の新しいログで短時間の記録とする。
- 設計上の注意：確認前にSolo／Effect BypassゲートやGroup照合を一律撤廃したり、失敗pathへ無条件再送したりしない。失敗した旧要求の自動再送と、新しいユーザー指定を区別する世代管理・最新実値照合・復元の検証が必要。AT-02、Mask映像とGroup境界、7.28.1実機は未検証。

## ビルド設定と成果物の統一（2026-10-26）

- 対象：LayerControl／LayerMaskControl。ソースは共通のまま、既存vcxprojをMakefileプロジェクトへ変更し、CMakeへBuild／Rebuild／Cleanを委譲。コンパイル設定・依存・JSON生成の二重定義を除去。SDKサンプル・旧Probeは対象外。
- Windowsの全構成を`cmake/BuildOptions.cmake`で`/MT`・`/utf-8`に統一。同梱GLEWはRelease静的CRTなのでDebugも同じCRTを使用し、最適化OFFとデバッグ情報を維持。デバッグ版CRTのヒープ診断は対象外。
- Windows DLLの出力を`binaries/x64/<構成>/`に統一。旧`build-control-plugins/source/plugins/{LayerControl,LayerMaskControl}/Release/`のDLLは、新しい出力の存在を確認して削除した。中間ライブラリ・生成プロジェクトは作業フォルダーに残す。Macのbundle出力先は変更していない。
- 検証環境：Windows、VS2022 MSBuild 17.14.40、Windows SDK 10.0.28000.0。既存`FFGLPlugins.sln`から対象2プロジェクトをRelease／x64、`/m`でビルド成功。共有ビルドツリーの処理はラッパー内のファイルロックで直列化。CMake直接実行でも両プラグインのRelease／Debugビルド成功。
- `LayerControl.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64`成功。`LayerMaskControl.vcxproj /t:Clean /p:Configuration=Debug /p:Platform=x64`で対象Debug DLLだけが削除され、LayerControl Debugと両Release DLLが残ることを確認。手順はSETUP.md。GUIのメニュー操作自体は未実施で、同じプロジェクトをMSBuildで検証した。
- テスト：`cmake -S tests -B build-control-tests -G "Visual Studio 17 2022" -A x64`、Releaseビルド、`ctest --test-dir build-control-tests -C Release --output-on-failure`成功（5/5、通信ランタイム試験を含む）。生成vcxprojでも/MT・/utf-8と出力先を確認。
- 残る制約：Debugリンクで同梱GLEWのPDB欠損警告LNK4099あり。GLEW内部のデバッグ情報は利用不可。CRT競合警告LNK4098は今回なし。Macビルド、Arenaへの導入・映像・同Column Transition反映は本整理では未実施。

## 固定IPv4・Group対応（2026-10-26）

- 変更：server.xmlの特定IPv4を接続先へ渡す。0.0.0.0は127.0.0.1へ変換。ローカルbindでそのPCで利用可能か確認し、IPv6・ホスト名・不正アドレスを拒否。Group全面拒否を削除し、Group側LayerのID・内容を正規layers配列と照合して識別の二重計数を防ぐ。
- Target Layerは現在の番号を指定し、並び替え前のLayerを追跡しない。復元先は従来どおりパラメータID。関連する併用条件・上限をSPECIFICATION.mdの2026-10-26節に整理。
- 実機読取り：Windows、Resolume Arena 7.27.1 rev15990、Composition Example、4 Layer／10 Column／1 Group。ws_snapshotで2回取得。最初はGroup内がLayer 4（ID 1792997542341）、後のUI照合ではGroup内がLayer 2・3（ID 1792997542341、1499337861306）。後者はUIのLayer 1 Galactucity、2 空Layer、3 Window Capture、4 Layer Controlとトップレベルlayersの順番が一致。取得中の構成変更は利用者側の操作で、エージェントが操作手順を管理した並び替え試験ではない。生データはPROTOCOL_MAP.md参照。保存・再生操作・構成変更は行っていない。
- 自動試験：SETUP.md記載のCMake構成・Releaseビルド、`ctest --test-dir build-control-tests -C Release --output-on-failure`を実行し5/5合格。Group内外、Group側の複写、不整合拒否、並び替え後の番号とパラメータID、Group除去、対象削除、実機キャプチャのキー解決を確認。これらは映像の受入試験ではない。
- 通信試験：`tests/test_websocket_runtime.py build-control-tests/Release/runtime_smoke.exe`に続けて、第2引数に`0.0.0.0`、`192.168.105.55`を指定して実行し合格。後者はこのPCのLAN IPv4で待ち受けるローカル模擬サーバーを使用。接続、差分set、読戻し、復元、自己通知収束、終了を確認。非ローカル203.0.113.1の拒否も確認。初めに候補192.168.0.111を使用した試験はbind時点でWinError 10049となり未成立。実際にbind可能な192.168.105.55で再試験した。DHCP／固定の割当方式自体は検査していない。
- ビルド：`cmake --build build-control-plugins --config Release --target ffgl-plugin-layer-control ffgl-plugin-layer-mask-control`成功。成果物は各`build-control-plugins/source/plugins/<プラグイン名>/Release/`配下。Windows SDK参照が通常サンドボックス内で拒否されたため、ビルドは昇格実行。新DLLのArena導入は未実施。
- 未実施／再現：新DLL導入後、本体Listen AddressをこのPCの固定IPv4にして設定保存・再起動し、実接続と値読戻しを確認。Group内外にEffect Clip／Target Layerを組み合わせ、Active時適用・非Active時復元を確認。Group追加・削除・移動の各操作前後にUI番号・Layer ID・パラメータID・実値を記録し、Target=Nが現在のN番を指すこと、旧対象の復元がそのIDにだけ行われることを確認。存在しない番号は送信しない。同Columnの映像Transition（AT-02）、MaskのGroup境界・一瞬の露出、全AT試験、Macは未実施。

## Windows／Mac共通コードへの移行（2026-10-23）

- Windows専用のWinHTTP／XmlLiteを削除し、両OSでWebSocket++ 0.8.2＋standalone Asio 1.28.0、TinyXML-2 10.0.0を使用。Runtime・差分同期・復元・ログファイル処理を共用。OSアダプターはDocuments取得、ホスト名検査、環境変数の文字列変換、デバッグ出力、UUID生成のみ。依存の取得元・ハッシュは`deps/control-dependencies.json`、ライセンスは`deps/CONTROL-LICENSES.txt`。
- Windows 10.0.26300、VS2022／MSVC 19.44.35226、SDK 10.0.28000.0で共通Runtimeとテストをビルド。`ctest --test-dir build-control-tests -C Release --output-on-failure`は5件全合格（control_logic、definition_validation、websocket_transport、websocket_runtime、server_settings）。
- 通信試験：分割Composition受信、差分送信、応答と読戻しの区別、復元、自己通知収束、位置通知抑止、待受中キャンセルに加え、接続ハンドシェイク中のキャンセル、HTTP拒否、ping/pong、バイナリ拒否、16 MiB超過フレームの拒否、peer closeを確認。実サーバーと同じRFC6455を使うローカルテストであり、Arena受入試験ではない。
- 設定試験：欠損、不正XML、無効状態、ポート境界、非対応アドレス、DTD、複数ルート、属性重複、深すぎるXML、非対応名前空間を拒否。旧JSON・RLC_WS_PORTを無視し、日本語パスのログを作成する既存試験も共通実装で合格。
- Windows用Visual Studioソリューションで両DLLのビルドに成功。WebSocket++の整数縮小警告C4267と既存GLEWのPDB警告LNK4099あり。新しい共用CMake構成でも`ffgl-plugin-layer-control`／`ffgl-plugin-layer-mask-control`のRelease DLL生成に成功。静的GLEWに合わせCRTを統一し、初回に出たLNK4098は修正後のCMakeビルドで解消。
- Mac用CMakeはFoundation／OpenGLをリンクし、MODULEを`.bundle`として生成する設定とInfo.plistを追加。同じテストをMacでも実行できる構成にした。ただしこの環境にはMac実機・Xcodeがなく、Macビルド・bundleのロード・Mac実行名・server.xml形式は未検証。Documents配置だけは[公式Directory List](https://resolume.com/support/en/directory-list)で確認した。
- 再現：Windows／Macのコマンドは[SETUP.md](SETUP.md)。Macではまず共通テストとbundleビルド、ホスト実行名・本体設定との照合を行う。両OSで新プラグインの接続、読戻し、終了、パススルー映像、AT-02同Column反映を別途実機試験する。署名・公証・配布試験も未実施。今回はホストへ導入していない。
- `set_sent`／`restore_sent`はライブラリの送信キュー受理を示し、実送信完了・ホスト反映とは区別する。読戻し確認を維持し、自動再接続・再送・ポーリングは追加していない。設計判断は[ADR-001](ADR-001-CROSS-PLATFORM.md)。

## 本体設定読取りへの移行実装（2026-10-23）

- 利用者の追記：前項調査時の接続失敗はArena未起動によるもの。保存設定の読取り不良とは扱わない。
- 変更：両プラグイン共通Runtimeが、WindowsのDocuments既知フォルダーとホスト実行ファイル名（Arena.exe）から本体の`Resolume Arena/Preferences/server.xml`を解決する。XmlLiteでXML全体を検査し、64 KiB上限・DTD禁止・深さ上限を設定。enabled=1、port=1〜65535、address=0.0.0.0／127.0.0.1のときだけ接続する。設定ファイルは変更しない。
- DLL隣接JSONと`RLC_WS_PORT`は廃止。ログは`RLC_WS_LOG`のみ（Windows Unicodeパス）。共有Runtime開始時のワーカー処理であり、描画中のファイル読取りや設定ポーリングは追加していない。
- 検証：`cmake -S tests -B build-control-tests -G "Visual Studio 17 2022" -A x64`、`cmake --build build-control-tests --config Release`、`ctest --test-dir build-control-tests -C Release --output-on-failure`を実行。4件全合格（control_logic、definition_validation、websocket_runtime、server_settings）。Windows SDK検出に通常サンドボックス外の読取りが必要なためビルドは昇格実行した。
- `tests/test_websocket_runtime.py`はCTestから実行。テスト専用コンパイルの注入口で一時XMLを指定し、本体設定は変更しない。旧JSONが不正／旧環境変数が不正でも新XMLで接続、日本語ログパス、無効・欠損・不正設定時の接続抑止、差分set・読戻し・復元・終了を確認。テスト用の設定パス指定は製品DLLに含めない。
- XMLテスト：コメント・属性順序、無効フラグ、ポート境界／不正文字／範囲外、属性欠損、非対応アドレス、別ルート、閉じタグ欠損、複数ルート、重複属性、DTD、サイズ超過、ファイル欠損、Documents移動後のパス構築、別ホスト拒否を確認。
- `build/windows/FFGLPlugins.sln`の`effects\LayerControl`／`effects\LayerMaskControl`をRelease／x64でビルド成功。GLEWのPDB欠損警告LNK4099あり。成果物は`binaries/x64/Release/LayerControl.dll`と`LayerMaskControl.dll`。
- 未実施：新DLLのArenaへの導入・実機接続、UI変更時のXML保存時期、Webserverとの起動順序、AT-02の同Column映像反映を含む実機受入。再現は[SETUP.md](SETUP.md)に従いDLLのみを配置し、旧JSON／旧環境変数に依存せず接続できるか、任意の本体ポート変更・再起動後も接続できるかを確認する。自動再接続は従来どおり行わない。

## 本体Webserver設定の自動取得調査（2026-10-23）

以下は移行実装前の調査記録。

- 要求：接続対象はプラグインをロードしているResolume本体とし、ポートを別JSONへ再入力する手間をなくす。
- 現行コード確認：`source/control/Runtime.h`はDLL隣接JSONを読み、`RLC_WS_PORT`をJSONの`port`より優先する。本体設定の読取りは未実装。同梱FFGL SDKの`SetHostinfoStruct`はname／versionのみで、Webserverポートを渡す項目はない。
- 対象環境の読取り確認：WindowsのDocuments既知フォルダーは`C:/Users/A-SUZUKI/Documents`。`Resolume Arena/Preferences/server.xml`のルートは`ServerController`で、`enabled="1" port="8080" address="0.0.0.0"`。子の`versionInfo`はResolume Arena 7.27.1 rev15990。XML自体の読み取り・ポートの整数取得に成功した。本体設定は変更していない。
- 接続確認：取得したポートを用い`GET http://127.0.0.1:8080/api/v1/product`を実施したが「リモート サーバーに接続できません」で失敗。今回の調査では保存設定と稼働中サーバーの一致を確認できていない。`Get-NetTCPConnection`もアクセス拒否で所有プロセスを検証できず、接続成功とは扱わない。
- 資料確認済み：[公式REST API & Webserver](https://resolume.com/support/en/restapi)はListen PortとListen Addressが変更可能であることを説明する。`server.xml`の形式・保存タイミングを公開互換APIとして保証する記載は今回確認できていない。
- 設計案：WindowsのDocuments既知フォルダーを解決し、ホスト製品を確認したうえで本体の`server.xml`をワーカーで読む。`enabled`・ポート範囲・待受アドレスを検査し、無効／不明／不正なら接続しない。127.0.0.1固定接続では、特定LANアドレスへのbindは対応可否の確認が必要。通常運用のポート指定は本体に集約し、診断ログ設定と分離する。自動取得は未実装。
- 未確認・再現手順：本体UIでポート／有効状態を変更した際のXML更新時期を観測し、再起動前後で保存値・待受ポート・製品応答を照合する。プラグインの起動がWebserver起動より早い場合、Documents移動、別製品／複数起動、XML欠損・破損も別途確認する。今回ホストの設定変更・再起動は行っていない。映像・AT試験は本調査の対象外。

## WebSocket版の実装・実機途中結果（2026-10-23）

現在の実装は`Layer Control`（初回試験時の表示名はLayer Control WS）。現行仕様はComposition通知による差分同期と、非Active時の復元である。

- Windows 10.0.26300.0 x64、VS2022 v143／MSVC 19.44.35226.0、Windows SDK 10.0.28000.0でRelease DLLをビルド。SDKは同梱2.2表記、上流commitは未確定。
- C++ロジック試験合格（最新件数はtests実行結果を参照）。合成状態と保存済み実受信ログを使用。Active判定、実値との差分、競合、重複キー、active_clip参照の除外、読戻し精度、有限要求管理を確認。
- ローカルWebSocketサーバーとWinHTTPの通信試験合格。分割受信、set→get、応答と読戻しの区別、未完了抑止、自己通知収束、位置通知抑止、無効化、受信待ち中終了を確認。環境変数／隣接JSON設定の両経路で実施。
- 利用者がArenaを起動・再起動後、Arena 7.27.1 rev15990、Composition Example、3 Layer／10 Column／Groupなしを取得。server.xmlでWebserver有効・port 8080を確認。
- 新DLLと`LayerControl.json`を`C:/Users/A-SUZUKI/Documents/Resolume Arena/Extra Effects`へ追加。旧Probeは上書きしていない。
- 新WinHTTPアダプターを使う読み取り専用ws_snapshotで実機Composition受信に成功。`docs/evidence/ws-native-20261023.json`に保存。
- Layer 1／Column 9の空きセルへ検証用Effect Clipを追加。API要求はタイムアウトしたため再実行せず読戻しで確認し、追加済み・Connected状態を確認した。FFGLパラメータとGUIDのInstance KeyがCompositionへ公開されることを確認。記録：`docs/evidence/ws-native-plugin-20261023.json`。
- DLL内の接続・通知受信も確認。起動直後のメタデータ照会用インスタンスで一度upgrade failureがあり、実Clip追加時の接続は成功。`docs/evidence/layercontrol-live-20261023.jsonl`に記録。
- **実機で判明した修正**：同じClipがlayer.clipsとlayer.active_clipに載る。初回版はGUID重複と誤判定して送信を抑止した。active_clipを参照表示として除外し、正規のclips配列で一意性を判断するよう修正、回帰試験を追加。修正DLLの実機適用はこの記録時点で待機中。
- 初回試験用ClipはTarget Layer=3、Duration=2.5、Blend Mode=Cutに設定（現行UIにApply Mask等はない）。Target Layerの元値はTransition Alpha／0秒。新DLLからのset成功・読戻し・映像適用はまだ未確認。

未実施：修正後のActive同期、複製／Preset／再読込のキー対応、Previewing、同じColumnの今回のTransition（AT-02）、Maskの実映像、パススルー画質、Group、全受入試験。コンパイル・接続・通知受信をこれらの合格としない。[SETUP.md](SETUP.md)に再現コマンドと試験手順を記載。

?OSC?Layer Ctrl Probe??????????????????????????Git???????????????[SETUP.md](SETUP.md)????
