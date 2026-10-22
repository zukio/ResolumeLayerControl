# Column操作を取得する別の方法の調査

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
