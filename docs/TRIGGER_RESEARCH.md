# Column操作を取得する別の方法の調査

## 2026-10-23 WebSocket実測（旧候補比較より新しい結果）

### 再生位置の直接購読（11:28〜11:29台）

最初の試行はInjectのみだったと利用者から訂正され、上書き後の `ws-position-01.jsonl` を解析した。Layer 1／Column 7のBasicEffect、position ID 1792718126172を購読。JST 11:28:52.976〜11:29:53.072、6037件：parameter_subscribed 1、parameter_update 6027、Composition 8、parameter_unsubscribed 1。証跡は `docs/evidence/ws-position-20261023-1129.jsonl`。

Column見出し5回クリックのログ（実機観測、JST 11:32:28〜11:32:50、2290件、証跡 `docs/evidence/ws-position-column-20261023-1132.jsonl`）：購読は同じ position ID 1792718126172。position が約1600〜3700から0へ戻った5回すべてで、約57〜66ms後に clip 7 オブジェクト全体の通知が1件届いた（クリック間隔は約1.6〜1.8秒）。Clip再トリガーと同じ「位置の巻き戻り＋Composition通知」の組合せがColumn操作でも観測された。別に、通常ループの折返し2回（4995→4、4992→3）は通知を伴わなかった。ただし通知の遅延は約60msで、先に位置の巻き戻りが来る。クリックと各巻き戻りの対応は時刻順で読んだもので、操作時刻の独立記録はない。Column操作とClip操作の区別、ループ直後の編集・シーク・Auto Pilot・Bypass等での誤検出は未確認。FFGL側で実際にこの情報を取得できるかも未確認。

通常ループに対応する約5000→0付近の巻戻りが10回（5秒周期、再トリガー後の1回を含む）。これらに対応するComposition通知は記録されていない。Layer 3のTransition Blend Mode編集は11:29:15.096（Alpha）、19.236（Burn）、23.675（Alpha）の3件。Composition内の対象Effect Clip位置は約2756、1900、1337で、編集に対応する位置の巻戻りは見られない。

その後のClipセル5回の再トリガー手順に対応して、11:29:41.505、42.746、44.049、45.356、46.668に位置がそれぞれ約4167、1231、1292、1298、1303から正確な0へ戻った。各50、57、48、55、54ms後にCompositionを受信し、そこでの位置は約39〜47。数値はNode-RED側受信時刻差であり、ホスト内部の保証遅延ではない。

判定：今回のログでは通常ループ・編集・再トリガー5回に異なる観測パターンがあった。ただし0との完全一致、末尾近傍判定、固定時間窓を製品用の保証条件にしない。シーク、ループ境界直前の再トリガー、ループ直後の編集、負荷・切断、再生範囲・速度変更は未検証。位置購読の更新頻度は今回約100件/秒であり、製品では複数インスタンスの重複購読とキュー負荷を考慮する必要がある。今回はClip直接再トリガーで、位置購読を併用したColumn見出し操作は別途確認が必要。

### 別Effect Clipの比較（11:14〜11:15台）

利用者の操作申告：Column見出しを5回、その後Clipセルを5回。上書きされたログは10件すべてComposition情報。Column 7、Layer 1のactive_clipはBasicEffect（ID 1772084178611、エフェクト名BasicEffectとTransform）。BasicEffectの配布元・実装はこのログから確定しない。

前半5件（JST 11:14:43.641〜48.968）はLayer 1〜3の再生位置が同時に0または約41〜46になった。後半5件（11:15:03.519〜08.501）はLayer 1だけ約36〜52となり、Layer 2・3は通常の再生位置の進行を示した。今回の操作順・件数と整合し、Column起動とClip直接起動の作用範囲の違いを観測できた。

利用者追加情報：エフェクトの中身は空。効果内部に再トリガー検出処理があると仮定しない。

同一操作継続中の差分は主にtransport.position.value。3行目には旧ClipのconnectedとColumn 8のclipconnectedの変化、6行目にはClip選択の変化もあった。専用の再トリガー番号・イベント種別は観測されない。前半最後と後半最初の受信間隔は約14.55秒で、その間の通知は記録されていない。無操作期間の独立した開始・終了マーカーはない。

LayerControl入りClipと同様に再操作時の位置が先頭付近になるが、これはホストのClip Transport情報であり、FFGL SetTimeに同じ値が渡る証拠ではない。ループ・シーク等との区別、再接続初期情報の除外、映像開始前の設定反映は未確認。証跡：`docs/evidence/ws-effectclip-20261023-1115.jsonl`。

訂正：以下の10:56台の17回は、利用者確認によりColumn見出しではなくLayer 3のClipセルのクリックだった。Clip直接再トリガー試験として扱う。Column操作試験は下記の11:00台の再試験と区別する。

### 編集との対照試験（11:04〜11:05台）

利用者はBlend ModeをBurnに変更したと報告。`ws-receive-02.jsonl`ではなく、既存`ws-receive-01.jsonl`の18〜24行に追記されていた。18行（JST 11:04:56.439）はAlpha、19行（11:05:31.115）はLayer 3 transition.blend_modeがBurn、active_clipのpositionは約4720.718。その後11:05:51.675、53.457、55.461、57.324、59.277の5件はBurnのままpositionが約42〜49であり、指定された再トリガー5回の手順と整合する。

重要：Transition Blend Mode編集でもComposition情報が届いた。Composition受信そのものを再トリガーと扱う方式は誤検出するため採用不可。これはプラグイン内部パラメータ編集ではなくLayer Transition設定の編集として記録する。記録内では19行と20行の間に通知はなく、無操作時に毎フレーム通知される挙動はこの区間では観測されない。positionの低値だけでループ・シーク等と区別できる保証はなく、製品用の検出方式は未確定。

### Column見出しでの再試験（11:00台）

利用者が操作箇所を訂正して再試験し、同名ファイルを上書きした。読み取った17件は全件Composition情報。接続Columnは行1〜5が8、6〜11が7、12〜17が8であり、前回のClipセル操作とは異なりColumn状態も切り替わった。受信時刻（JST）は順に11:00:21.203〜24.557、11:00:28.523〜33.845、11:00:36.096〜40.535。件数は直前に申告された5・6・6の手順と整合する。

連続する同一Columnの通知ではtransport.position.valueのみ変化するものが多い。切替後には旧Clipのconnected／clipconnectedの変化を含む通知もあり、通知受信自体や位置の閾値をトリガー識別の根拠に確定しない。独立したクリック時刻記録はなく、一対一の時刻照合と無操作期間の記録開始・終了確認は未実施。選択・プレビュー・編集・再接続の対照試験も未実施。

証跡：`docs/evidence/ws-column-20261023-1100.jsonl`（上書き前に作業ログから複製）。今回の限定操作で同一Column再操作中にもComposition通知を取得できたが、製品用Trigger DetectorやAT-04全体の合格とはしない。

方針はユーザー指定によりWebSocket双方向。対象Arena 7.27.1、Node-REDで保存した `ws-receive-01.jsonl` を解析。利用者の操作申告は、LayerControl配置Columnを5回、隣を6回、配置Columnを6回。ログは17行で全件Composition情報、個別parameter_update通知ではない。

| ログ行 | JST受信時刻 | Layer 3 active_clip ID | 操作申告との対応 |
| --- | --- | --- | --- |
| 1〜5 | 10:56:07.971〜10:56:12.284 | 1772084178630 | 配置Columnの5回 |
| 6〜11 | 10:56:13.907〜10:56:24.261 | 1772084178621 | 隣の6回 |
| 12〜17 | 10:56:28.909〜10:56:35.405 | 1772084178630 | 配置Columnの6回 |

件数・順序・active_clipの切替が申告と一致する。独立したクリック時刻の記録はないため、各操作との時刻照合や配送保証まで確認したものではない。全17行でcolumns[6].connectedはDisconnected、columns[7].connectedはConnectedのまま。Column見出しのクリックとClipセルの直接クリックの区別は未確認であり、Column状態から操作先を確定しない。

同一Clip継続中の主な差分はtransport.position.value。Layer 3の対象Clipは多くの通知で約39〜50だが、通常ループ・シーク等でも生じ得るため閾値判定を採用しない。FFGLのSetTimeとは別情報であり、過去のFFGL診断結果を覆すものではない。

判定：今回の限定操作では再操作を含めて通知数が一致した。Composition通知そのものを起動と見なす方式は未成立。選択・プレビュー・編集等の対照試験、Column見出しとClipセル操作の区別、再接続時の初期情報除外、同一Column映像への反映順序が未検証。元ログは上書きされ得る作業ファイルであり、恒久的な証跡の保管先は未確定。

調査日：2026-10-22。対象実機：Resolume Arena 7.27.1 15990。

## 現時点の結論

既存FFGL通知だけでは、同じColumnを押す操作のたびに実行する方法は成立していない。追加候補として、まず **Resolume本体のOSC出力を観測する**。これはプラグインからOSCを送る、これまでの試験とは逆方向である。

この調査用受信ツールはResolumeの設定を書き換えず、受信した情報だけを保存する。製品への統合や、追加の常駐アプリの採用を確定したものではない。

## 候補と根拠

| 方法 | 確認できたこと | 未確認・制約 |
| --- | --- | --- |
| FFGLの追加通知 | 同梱FFGL.cppの入口を確認。専用のColumn／Clip再トリガー通知は見つからなかった | 未知の独自拡張が絶対に存在しないとは断定しない |
| ResolumeのOSC出力 | 公式資料にClip trigger等の出力、項目ごとの出力設定、Bundle対応がある | 同一Columnの連打で毎回出るか、送信値、通知順序、初期状態出力との区別は実機未確認 |
| パラメータアニメーション | 公式資料にClip／Column起動に応じた開始条件がある | Effect Clipでの挙動、同一Column押し直し、ループ・編集・読込みとの区別、初期化中の抑止は未検証。追加の診断用パラメータと利用者による設定が必要 |
| WebSocket | パラメータを購読し、値の変更を通知できる | 同値への再操作の通知保証は資料から得られなかった。triggerは送信できる操作であり、全トリガーの受信購読を意味しない |
| 外部コントローラー経由 | 外部で起動操作を受けて設定→Column起動の順序を制御する構成を考えられる | 設計案。通常のResolume画面での操作から構成が変わる。元仕様を達成済みとは扱わない |

同梱SDKの `ParamTrigger` はイベント型パラメータで、名前がTriggerでもClip起動を自動通知するものではない。`FF_GET_PARAMETER_EVENTS` もプラグインが発行したパラメータ変更をホストが読む向きの仕組みで、Column操作の受信ではない（同梱FFGL.h、FFGLParamTrigger.cpp、FFGLParamEvent.cppを確認）。

## 次の実機試験：Resolume → PythonのOSC

**DLLの再ビルドは不要です。** Probe ModeはObserve onlyにしておく。既存のOSC出力を使用している場合は、宛先・ポート・プリセットの元の設定を記録し、試験後に戻す。

### 1. 受信ツールを起動する

同じWindowsのPowerShellで実行する。19091はこの試験のために選んだポート例で、Resolumeの既定ポートではない。使用済みなら別の空きポートに変え、送信側も合わせる。

```powershell
Set-Location 'N:\data\C++\ResolumeLayerControl'
python tools/receive_resolume_osc.py --port 19091 --output "$env:TEMP\rlc-resolume-osc-01.jsonl"
```

`Listening`が出たら準備完了。既存ファイルは上書きしない。既にあれば02など別の名前を指定する。

### 2. Resolume本体のOSC出力を設定する

1. ResolumeのPreferences（環境設定）→OSCを開く。
2. **OSC Output／出力**を有効にする。Input／入力の設定ではない。
3. 出力先をLocalhost、またはIPアドレス `127.0.0.1` にする。
4. Outgoing Port／出力ポートを `19091` にする。
5. Shortcuts → Edit OSCを開く。
6. 現在のプリセット名を記録してから、調査用に `Output All OSC Messages` を選ぶ。この試験では出力アドレスを推測して限定しない。
7. OSC編集モードを終了し、通常操作へ戻る。

これは現在の公式英語資料に基づく手順案。Arena 7.27.1の実画面で項目名や配置が異なる場合は、表示内容を確認して記録する。設定直後には現在値が出る可能性があり、その受信だけをトリガー成功と数えない。

### 3. 同じColumnを5回押す

1. 別のPowerShellでクリック記録も起動する。

```powershell
Set-Location 'N:\data\C++\ResolumeLayerControl'
python tools/mark_probe_clicks.py --output "$env:TEMP\rlc-clicks-04.csv"
```

2. Alt+TabでResolumeへ移動し、ProbeのColumnへ切り替える。
3. 3秒待ち、同じColumnを約2秒間隔で5回クリックする。他の場所はクリックしない。
4. 3秒待ってAlt+Tabで戻り、クリック記録側でCtrl+Cを押す。
5. OSC受信ツール側でもCtrl+Cを押す。

画面には最初の10パケットと以降100パケットごとの表示だけが出る。画面に行が増えなくても、保存ファイルには全受信パケットを残す。Output Allは情報量が多いため、試験を終えたら出力設定を元に戻す。

### 4. 2つの記録を共有する

```powershell
$desktop = [Environment]::GetFolderPath('Desktop')
Copy-Item "$env:TEMP\rlc-resolume-osc-01.jsonl" "$desktop\rlc-resolume-osc-01.jsonl"
Copy-Item "$env:TEMP\rlc-clicks-04.csv" "$desktop\rlc-clicks-04.csv"
```

JSONLは1行ごとに受信時刻、送信元、OSCの内容、受信バイト列を保存したテキスト。未対応の型もバイト列は保存する。Composition内の名前や素材に関する情報が含まれる可能性があるため、共有範囲は確認する。

## 判定方法

- 5回のクリック時刻に対応して、同じColumn／Clipの起動を示す通知が毎回あるか確認する。同じアドレス・同じ値でも捨てない。
- 通常再生中、選択・プレビュー・パラメータ編集、初期状態出力でも同じ通知が出ないか追加試験する。
- UDP受信の記録だけで配送保証・exactly-onceとは言わない。受信欠落と「Resolumeが通知しなかったこと」は区別する。
- Columnと所属Clipの両方の通知が届く可能性がある。それぞれを1要求に数える実装にはしない。通知の意味と組合せを実測してから決める。
- 本方式を採用する場合、FFGLパラメータと受信イベントの対象対応、有限キュー、停止中の通知、再起動や旧イベントの扱いを別途設計する。
- イベント受信が成立しても「同じColumnの映像へ新設定が間に合うか」は別問題。従来どおりUNK-05／AT-02で判定する。

## ローカルで確認した範囲

- `tools/receive_resolume_osc.py` を追加。Python標準ライブラリだけを使用し、127.0.0.1で受信のみ行う。重複メッセージを保持し、Bundle内の各メッセージも表示する。
- `python3 tests/test_resolume_osc.py`：4件合格（値の復号、同一メッセージ2件のBundle保持、未対応タグ、壊れたパケット）。
- Resolume本体のOSC出力による再トリガー検出は未実施。

## 資料

確認日：2026-10-22。公式Web資料は対象版固定の資料ではないため、実機7.27.1での確認を必要とする。

- [Resolume OSC：OSC Output、Custom OSC Messages、Destinations、Bundles](https://resolume.com/support/en/osc)
- [Resolume Parameter Animation：Parameter Start Settings](https://resolume.com/support/en/parameter-animation)
- [Resolume WebSocket API：parameter_update](https://resolume.com/support/en/websocket-api)
- [Resolume REST API & Webserver：対象版のローカルReferenceの開き方](https://resolume.com/support/en/restapi)
- リポジトリ内：`source/lib/ffgl/FFGL.cpp`、`FFGL.h`、`ffglquickstart/FFGLParamTrigger.cpp`、`FFGLParamEvent.cpp`

## 実機で見つかったColumn操作通知（2026-10-22）

利用者が共有したTEMP内 `rlc-clicks-09.csv` と、同じTEMP内で読み取れた `rlc-resolume-osc-04.jsonl` を照合した。対象はResolume Arena 7.27.1 15990。受信ログは53,213パケット、wall_ms範囲1792676681139〜1792676738583。クリック8〜15はこの受信ログより前の操作なので判定対象外。

**同じColumnを押し直した操作に対応するOSC出力を観測できた。** FFGLのConnectとは異なり、Resolume本体のOSC Outputが発行するメッセージである。

| クリック番号 | 観測した通知 | クリック検出から受信までの差 |
| --- | --- | --- |
| 34 | `/composition/columns/8/connect`、int32の1 | 0ms |
| 35 | 同上 | 1ms |
| 36 | 同上 | 3ms |
| 37 | 同上 | 4ms |
| 38 | 同上 | 3ms |
| 41〜45 | 同じColumn 8のconnect、1が各クリックに対応して再び出力 | 0〜5ms |

この差はPython側2ツールの観測時刻の差であり、ホスト内部の処理順・保証遅延を表すものではない。

上記の各操作では、後から同じアドレスのint32の0も出た（クリック34〜38では約216ms後）。**1と0をそれぞれ起動1回として数えてはいけない。** 0の意味は実機UIと追加照合が必要。クリック34ではColumn選択・connected状態も変化したが、35〜38では状態変化なしでもconnect=1が繰り返し届いた。したがって今回のログでは状態変化の監視より、操作に対応するconnect出力が有力である。

クリック40・46はColumn 7、48はColumn 6のconnect=1、その後41・47・49はColumn 8のconnect=1だった。これらはアドレスから読んだ操作先であり、補助ツールが画面座標からColumnを判定したものではない。

判定：同一Columnの再操作を含むイベント取得の候補として前進した。受信からFFGLインスタンスの設定を取り出して適用する方式は未実装。プレビュー・初期出力・自動再生・キーボード/MIDI等の誤検出と取りこぼし、同一Column映像へのTransition反映順序も未検証。元仕様全体の達成とはしない。

## 内部受信と既存の外部OSC連携の共存（2026-10-22）

要求：利用者は既存の外部制御を妨げずに、内部でもColumn操作通知を受けたい。二重送信・中継・別経路を比較中であり、方式は未採用。

資料確認済み：[公式OSC資料のDestinations節](https://resolume.com/support/en/osc)には、Bonjourで見つかった宛先、Localhost、Broadcast、手動IP指定とOutgoing portが記載されている。内部と外部に別々のIP・ポートを登録して同時送信する機能は、今回の資料調査では確認できなかった。資料は版固定ではなく、対象Arena 7.27.1の実画面で複数宛先の可否を確定した結果ではない。

| 候補 | 評価・未確認事項 |
| --- | --- |
| Localhostと外部IPへ個別送信 | 公式資料から設定方法を確認できず。利用可能と仮定して実装しない |
| Broadcast | 公式にある複数機器向け出力。内部受信も成立するかはWindows実機で未検証。外部受信側のポート・Broadcast対応、送信対象の拡大、既存連携への影響を確認する必要がある |
| ローカル中継から内部処理と既存外部宛先へ分配 | 設計案。外部の宛先ポートを維持しやすいが、中継停止が外部出力へ影響する。元パケットの維持、送信元変更の影響、遅延・欠落・ループ防止を検証する |
| 別イベント経路 | OSC出力設定を維持できる候補だが、WebSocket等で同値の再操作を取得できるかは未確認 |

コード確認：現行の `tools/receive_resolume_osc.py` は127.0.0.1にbindする。Broadcast受信対応済みではなく、同ツールのままBroadcastへ切り替えて成功すると案内しない。

次の確認：対象版のOSC Output画面で宛先追加の有無と現在の外部宛先・ポート・出力プリセットを確認する。Broadcastを試す場合はLAN側を受信できる試験用受信器を別途用意し、内部・外部の双方で同一Column再操作を照合する。既存の出力プリセットをOutput Allへ無条件に置き換えない。外部からResolumeへの入力だけを使う構成と、Resolumeからのフィードバック出力も使う構成を区別する。

今回の調査は資料と既存コードの確認のみ。Resolumeの設定変更・中継実装・同時受信の実機試験は行っていない。
