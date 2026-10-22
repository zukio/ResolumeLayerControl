# Layer Ctrl Probeの使い方と確認結果

更新日：2026-10-22

## 1. 今どこまで分かったか

Layer Ctrl Probeは、Layer設定制御を作る前に、通信と起動の検出方法を調べるための試作品です。TransitionやMask Modeはまだ変更しません。

今回、利用者のResolume Arena上で次の結果が得られました。

| 確認したこと                                               | 結果                                               |
| ---------------------------------------------------------- | -------------------------------------------------- |
| プラグインからOSCを送り、同じWindowsのPythonで受け取れるか | **受信できた**。sequence 12、14、16、18の4件を確認 |
| ProbeのあるColumnへ切り替えると送られるか                  | 利用者の操作報告では送られた                       |
| 再生中の同じColumnを5回押すと、5回送られるか               | **送られなかった**                                 |
| 操作せず10秒待つと、余分に送られないか                     | 追加受信なし                                       |

**通信はできました。しかし、今の方式では同じColumnを押し直した操作を検出できません。** 「Columnを押すたびに確実に1回実行する」という要求は、まだ達成していません。

現在は、Resolumeがプラグインへ送る `Connect` という通知に合わせてOSCを送っています。今回の操作では、Columnへの切替時に受信があり、同じColumnの押し直しでは受信がありませんでした。Connectと実際の操作の対応を詳しく確認するには、CSVログとの照合が必要です。

FFGLから得られる他の通知で押し直しを検出できるかは未確認です。FFGL全体で不可能と判明したわけではありません。

### まだ記録が必要な情報

- Windowsのバージョン
- Resolume Arenaのバージョン
- 実際に使ったVisual Studioのバージョンとビルド設定
- DLLを配置したフォルダ
- 試験時のCSVログ（まだ内容を確認していない）

プラグインの読込みとOSC受信は利用者の実機で確認できました。ビルドに使った条件は、まだ記録できていません。

## 2. 試験で使う画面

同じWindows上で、次の3つを使います。

| 画面         | 役割                                               |
| ------------ | -------------------------------------------------- |
| PowerShell ① | Python受信ツールを動かす。OSCを受け取ると1行増える |
| PowerShell ② | ポートとログ保存先を設定して、Resolumeを起動する   |
| Resolume     | Effect Clipを配置し、Columnを操作する              |

以下のコマンドはすべて **PowerShell用** です。`PS N:\...>` などの先頭の表示は入力せず、コード枠の中だけを入力してください。

## 3. 最初に一度だけ行う準備

既にLayer Ctrl ProbeがResolumeで使える場合は、4節へ進んでください。

1. Visual Studioで `build\windows\FFGLPlugins.sln` を開く。
2. 構成を `Release`、プラットフォームを `x64` にする。
3. ソリューションエクスプローラーの `effects` 内にある `LayerControlProbe` を右クリックして「ビルド」を選ぶ。
4. 出力されたDLLを、ResolumeがFFGLプラグインを読み込むフォルダ(%USERPROFILE%%\Documents\Resolume Arena\Extra Effects など)へコピーする。配置先はResolumeの設定で確認する。

プロジェクト上の出力先は `binaries\x64\Release\` です。

Visual Studio 2022または2026を使えます。まずはプロジェクトに指定されている `v143` を維持してください。インストーラーには「C++によるデスクトップ開発」、Windows SDK、MSVC v143が必要です。VS 2026で従来のプロジェクトとツールセットを使えることは[Microsoft公式資料](https://devblogs.microsoft.com/cppblog/upgrading-c-projects-to-visual-studio-2026/)で確認しています。このプロジェクトのVS別のビルド結果は未記録です。

## 4. Python受信ツールを起動する

PowerShell ①を開き、次を実行します。

```powershell
Set-Location 'N:\data\C++\ResolumeLayerControl'
python tools/receive_probe.py --port 19090
```

次の表示が出たら準備できています。この画面は開いたままにします。

```text
Listening on ('127.0.0.1', 19090)
```

19090は今回の試験用ポートです。Resolume本体のOSC受信ポートを設定する手順ではありません。

## 5. 設定を渡してResolumeを起動する

まず、起動中のResolumeを終了します。続いて、**別のPowerShell ②**を開いて、次を1行ずつ実行します。

```powershell
$env:RLC_PROBE_PORT = '19090'
$env:RLC_PROBE_LOG = "$env:TEMP\rlc-probe.csv"
& 'C:\Program Files\Resolume Arena\Arena.exe'
```

最後のパスが実際のインストール先と異なる場合は、Resolumeのショートカットの「プロパティ」→「リンク先」を確認して置き換えてください。

最初の2行は、これから起動するプラグインに次の情報を渡します。

- OSCを送るポート：19090
- 操作記録を保存するファイル：Windowsの一時フォルダ内の `rlc-probe.csv`

**この2行を実行したPowerShellからResolumeを起動することが必要です。** 通常のショートカットから起動したResolumeや、既に起動していたResolumeには、この設定は渡りません。

起動時にPowerShellへResolumeのログが流れても、そのままにしてください。OSCの受信結果を見る画面はPowerShell ①です。

## 6. Resolumeで試す

最初は、Probeを配置する専用Layerを1つ用意してください。普段の映像Clipがあるセルを置き換えないようにします。Probeは1つだけにすると記録を追いやすくなります。

### A. 記録だけを行う

1. 専用LayerにEffect Clipを作り、`Layer Ctrl Probe` を追加する。
2. `Probe Mode` を初期値の `Observe only` にする。
3. 別Columnへの切替や、同じColumnの押し直しを行う。

このモードではOSCを送らないため、Python画面に行が増えなくても正常です。通知はCSVへ記録します。

### B. OSCが届くか確認する

1. `Probe Mode` を `OSC on Connect TEST` にする。
2. 別のColumnへ切り替える。
3. Probeを置いたColumnへ戻る。
4. PowerShell ①に受信の行が増えるか確認する。

表示例：

```text
1792671062375194100 ('127.0.0.1', 56062) sequence 12
```

この1行は試験メッセージを1件受信したという意味です。先頭は受信時刻、56062は送信側が使ったポートです。19090と一致する必要はありません。

`sequence` は診断イベントの通し番号です。送信以外のイベントにも番号が付くため、12→14→16のように飛んでも異常ではありません。

### C. 同じColumnの押し直しを確認する

1. ProbeのColumnを再生したまま、同じColumnを間隔を空けて5回押す。
2. 受信行が何件増えたか記録する。
3. 操作せず10秒待ち、さらに行が増えないか確認する。

今回の結果は「押し直しでは増えない」「待っていても増えない」でした。現在のConnect方式では、押し直しを検出する要求を満たしません。同じ試験を繰り返しても、この結果だけで検出成功とはしません。

ProbeのないColumnで送信がないこと自体は、現在の実装では自然な動作です。

## 7. CSVログを開く・共有する

別のPowerShellから次を実行します。

```powershell
notepad "$env:TEMP\rlc-probe.csv"
```

エクスプローラーで探す場合は、**エクスプローラーのアドレス欄**に次を入力します。

```text
%TEMP%
```

開いたフォルダ内の `rlc-probe.csv` を探してください。`%TEMP%` はPowerShellへ入力する書き方ではありません。

ファイルが見つからない場合は、5節の方法でResolumeを起動したか、Layer Ctrl Probeを読み込ませたかを確認します。

ログには以前の記録も追記されます。試験ごとに分けたい場合は、Resolumeを終了した後で既存ファイルの名前を変更し、5節から起動し直してください。

主な項目の意味：

| 表示                                 | 意味                                                               |
| ------------------------------------ | ------------------------------------------------------------------ |
| `create`                             | プラグインが作られた                                               |
| `init_gl` / `deinit_gl`              | 描画の準備／終了                                                   |
| `connect` / `disconnect`             | ホストから接続／切断の通知が来た。Column操作と同じ意味とは限らない |
| `first_process`                      | 描画準備後の最初の描画処理が呼ばれた                               |
| `mode_observe` / `mode_connect_test` | モードの値が設定された。保存値の復元でも記録され得る               |
| `osc_requested=1`                    | 試験OSCの送信が要求された                                          |
| `send_bytes=20`                      | 20バイトの送信処理が成功した。受信確認はPython画面で別に行う       |
| `send_bytes=-1`                      | 未送信または送信失敗。この値だけでは原因を判別できない             |
| `dropped`                            | 記録を取りこぼした累積数。増えていれば、回数判定には使わない       |

結果を共有するときは、「何を何回押したか」「受信行が何件増えたか」とCSVの該当部分をセットで残してください。可能なら操作画面も録画します。

## 8. 次に調べること

次は、同じColumnを押し直したときに、Connect以外の通知や情報を使えるか調べます。通知記録を拡張した診断版を追加しました。以下の手順で確認します（新しいDLLでの実機結果はまだありません）。

### 通知記録を拡張した版で試す手順

1. Resolumeを終了する。Visual StudioでLayerControlProbeを再ビルドし、以前と同じ配置先のDLLを新しいDLLで上書きする。Debugでビルドした場合は `binaries\x64\Debug\LayerControlProbe.dll` を使う。
2. 以前のCSVを別名で保管する。5節のPowerShell手順でResolumeを起動する。
3. Probe Modeを **Observe only** にする。今回はCSVを見るため、Python受信画面に何も出なくても正常。
4. 別のPowerShellを開き、次を実行する。ファイル名は試験ごとに変える（既存ファイルは上書きしない）。

```powershell
Set-Location 'N:\data\C++\ResolumeLayerControl'
python tools/mark_probe_clicks.py --output "$env:TEMP\rlc-clicks-01.csv"
```

5. Resolumeに戻り、ProbeのColumnへ切り替え、3秒待つ。同じColumnを約2秒間隔で5回クリックする。最後に3秒待つ。この間は他の場所をクリックしない。可能なら録画する。
6. 補助ツールのPowerShellへ戻り、Ctrl+Cで記録を終了する。画面を戻すためのクリックなども記録されるので、試験と無関係な行があれば共有時に伝える。ColumnをキーボードやMIDIで操作した場合、このツールでは記録できない。
7. Probeを停止して数秒待つ。次の2ファイルをデスクトップへコピーし、両方共有する。

```powershell
$desktop = [Environment]::GetFolderPath('Desktop')
Copy-Item "$env:TEMP\rlc-probe.csv" "$desktop\rlc-probe-result.csv"
Copy-Item "$env:TEMP\rlc-clicks-01.csv" "$desktop\rlc-clicks-01.csv"
```

クリック記録の `before_wall_ms`〜`after_wall_ms` が、ボタンが押されたことを観測した時刻の範囲。Resolumeがその操作を処理した時刻そのものではない。2ミリ秒ごとの確認を目標にしているが、Windowsの負荷で間隔が広がったり、非常に短いクリックを見逃したりする可能性がある。5回の操作に対応する行があるか確認する。`foreground_title` はそのとき手前にあった画面の名前で、Column番号は記録しない。画面名に共有したくない情報がある場合は共有前に伏せてよい。


新しい版ではCSVに `create_trace_v3` が記録される。これがなければ、以前のDLLや以前のログを見ている可能性がある。

追加した主なイベント：

| イベント | 記録内容 |
| --- | --- |
| `set_time` | a：ホストから渡された時刻。その意味・単位と再トリガーとの関係は未検証 |
| `set_beat` | a：BPM、b：拍の位相 |
| `set_sample_rate` | a：サンプルレート |
| `resize` | a：幅、b：高さ |
| `set_float` | a：パラメータ番号、b：指定された値。不正値も記録 |
| `get_float` / `get_parameter_display` / `get_text` | a：ホストが照会したパラメータ番号 |
| `set_text` | a：パラメータ番号、text：渡された文字列（最大159バイト） |
| `get_input_status` | a：入力番号 |
| `process` | a：入力テクスチャ数。描画呼出しごとに記録 |
| `host_name` / `host_version` | text：ホストから通知された名前／版（最大159バイト） |

CSVの `wall_ms` は日時と照合するための時刻（Unix時刻のミリ秒）。先頭側の従来の時刻は処理順序を見るための単調増加時計。Windowsの時計が補正されるとwall_msは前後する場合がある。

毎フレームの呼出しも、同じ値の繰返しも記録するため、短時間でも多数の行が出る。これらを削除せず共有する。通常再生中と押し直し前後を比較し、通知の発生・値・順序に差があるか調べる。拍の周回や時刻の変化だけをトリガーと断定しない。

キューは4096件で、書込み側とのロック競合で破棄しない方式に変更した。ただし満杯や通知側同士の競合が続いた場合は破棄するため、`dropped` が増えた場合は取りこぼしがある。その試験から「通知が来なかった」とは判定しない。終了時は残ったログを書き終える。OSCの未送信分は終了時に送信しない。終了行の `discarded=0` はログの未処理分が残らなかったことを示す。新しい通知の記録はOSC送信を増やさず、OSC試験モードの送信条件はConnectのまま。

今回記録するのは同梱SDKでオーバーライド可能なインスタンスの呼出し。SDK内部で処理される問い合わせを含むすべてのFFGL API呼出しを捕捉するものではない。

その後、次の操作を個別に確認します。現時点では未検証です。

- Clipの直接起動、停止、Eject、空セルのColumn
- 選択、プレビュー、サムネイル生成、パラメータ編集
- 保存・再読込み、Clip複製、Preset再適用、描画の再初期化
- Bypass、Opacity 0、Solo、Auto Pilot、Fader Start、読込時自動再生
- 高速なColumn切替、旧Clipと新ClipがTransition中に重なる場合
- 入力映像の色・Alpha・向き・サイズが維持されるか

本番の起動とそれ以外を区別できなければ、FFGL単独の方式を成立済みとはしません。追加の連携が必要なら、構成変更案として検討します。

**同じColumnで始まる映像に新しいTransition設定が間に合うか**は、別の重要な試験です。今回の試験OSCではLayer設定を変えないため、まだ確認できていません。仕様書のAT-01〜AT-15も一括で合格したとは扱いません。

## 9. 開発者向けの補足

普段の試験では、この節の操作は不要です。

### 実装と環境

- ベースは `source/plugins/AddSubtract`。元のサンプルは保持し、別プラグインを追加した。表示名は `Layer Ctrl Probe`、仮IDは `RLCP`。
- 同梱SDKのCMake表記は2.2。上流commitは未確認。実装時点では本リポジトリにもcommitがなかった。
- ローカルの `/mnt/n/data/C++/ffgl-effect` を確認したが、由来・版の対応は未確定。ルートの `LICENSE.md` はBSD 3条項。
- `N:\data\WPF\WindowCapture\WindowCapture2ffgl\WindowCapture2ffgl` のFFGLHostAdapter.cpp、WindowCaptureSource.cpp、vcxprojを参照。選択肢の値、処理の保留、v143/x64構成を確認した。動的候補更新とホストへのパラメータ変更通知は、今回の診断版には追加していない。
- エージェントの作業環境はWSL2 Linux x86_64。CMake・GLEWヘッダーがなく、FFGL全体のビルドは未実施。

### 診断版の制約

`OSC on Connect TEST` は実験用であり、製品用のトリガー検出器ではない。プレビューや保存値の復元・再初期化でもConnectが来れば送信し得る。本番のLayer設定制御には使用しない。

通信・ディスク書込みは専用ワーカーで行う。通知側は最大4096件の複数書込み元に対応するリングキューへ追加する。書込み側との共有mutexは使わず、通知側のCAS試行は64回に制限する。満杯または試行上限到達時は破棄してdroppedを加算する。終了時はログを排出するが未送信OSCは送らない。ホストはインスタンスを破棄する前に通知呼出しを終える必要がある。ログのflushは各行ではなく、約100ミリ秒ごとまたはキューが空になったときに行う。複数インスタンスでは同じログへ書き込むため、最初の回数試験は1インスタンスで行う。従来のCSV時刻は単調増加時計のマイクロ秒。追加のwall_msは日時照合用。

### 実施済みの通信単体試験

Linuxで次を実行し、通信ワーカーをコンパイルした。

```sh
g++ -std=c++11 -pthread -Wall -Wextra -Werror tests/probe_worker_smoke.cpp -o /tmp/rlc-probe-smoke
python3 tests/test_probe_transport.py /tmp/rlc-probe-smoke
```

OSCアドレス `/rlc/probe`、型タグ `,i`、int32値1、20バイト、追加パケットなし、ログ出力、ワーカー終了を確認した。ソケット生成の制限があったため、通信試験は承認されたサンドボックス外実行で行った。

この単体試験と、1節の利用者によるResolume実機での受信結果は、別の検証結果として扱う。

通知記録拡張後の確認：g++で通信ワーカーを再コンパイルし、OSC単体試験に合格。同一値の通知を2回とも残すこと、引数値と文字列のスナップショットを保持することも確認した。プラグインは同梱GLEWヘッダーと `-DGLEW_NO_GLU` を使ったLinux上の構文検査に合格（GLUヘッダーがないため同定義を使用）。Windows DLLの再ビルド・追加通知の実機確認は未実施。

### 通知記録v3の確認

- `create_trace_v3` が更新確認の目印。
- 共有されたCSV（利用者のデスクトップのrlc-probe-result.csv）を調査した。65,928行、新版の実行インスタンスでConnect 7回、Disconnect 6回、dropped最大72。ホスト通知からResolume Arena 7.27.1 15990を確認した。操作時刻の記録がなく取りこぼしもあるため、再トリガー時に通知がなかったとは断定しない。
- Linuxで4スレッドから各800件、合計3,200件を記録する試験を10回実施。件数・引数の一意性・sequence順序・dropped=0・終了時のログ排出を確認。
- プラグインのLinux構文検査とOSC／値保持の単体試験に合格。Windows用クリック記録ツールはPythonの構文検査済み。新DLLとクリック記録ツールのWindows実機動作は未確認。

再現コマンド：

```sh
g++ -std=c++11 -pthread -Wall -Wextra -Werror tests/probe_worker_concurrent.cpp -o /tmp/rlc-probe-concurrent
python3 tests/test_probe_concurrent.py /tmp/rlc-probe-concurrent
```

### 利用者のv3実機ログを照合した結果（2026-10-22）

対象資料：デスクトップの `rlc-probe-result.csv` と `rlc-clicks-02.csv`。旧v2の追記分とv3を分けて集計した。

- v3の実行インスタンス `000001AC951000F0` は97,336行、droppedは全行0。SetTime・拍情報・描画は各32,436件。Connect 6回、Disconnect 5回。今回の範囲では記録方式改善後の取りこぼしは観測されなかった。
- Resolume画面でのクリック7〜12の前後（各クリックの100ms前〜500ms後）には、SetTime・拍情報・描画だけが継続し、Connectやパラメータ設定等の追加イベントはなかった。クリック8〜12を含む時刻範囲でSetTimeの逆行は観測されなかった。
- クリック13・15・17の後にDisconnect、14・16の後にConnectを記録。クリック18〜20の前後には描画等も記録されていない（17の後に切断された状態）。
- 途中でOSC試験モードへ変更されており、Connect時にはOSCも送信されていた。
- クリックの対象セル／Column番号は補助ツールでは分からない。8〜12が同一Columnの再トリガー操作かは利用者への確認待ち。この対応が確認できるまでは、再トリガー専用通知の有無を確定しない。

この結果は同梱SDKで記録した通知に限る。FFGL全APIで検出不能と証明したものではない。

### 操作を絞った5回の再トリガー試験（2026-10-22）

利用者は同じColumnだけを連続して5回押したと報告。今回のクリックはTEMP内 `rlc-clicks-03.csv` の2〜6番で、after_wall_msは1792674298848、1792674299992、1792674300996、1792674301933、1792674302782。チャットの出力とも一致した。Desktopの同名CSVには以前の02相当の内容が入っていたため、解析には使用しなかった。Desktopの通知ログも古かったため、TEMP内 `rlc-probe.csv` を読み取って照合した。

各クリックの100ms前〜500ms後にはSetTime・拍情報・描画だけを記録し、各通知件数は順に67、65、69、70、70件。各範囲のdroppedはすべて0。クリック5件を含む範囲でSetTimeの逆行はなかった。Connect、Disconnect、パラメータ設定等の追加通知は記録されていない。

判定：今回の環境・操作・計測範囲では、記録した通知から同一Column再トリガーを識別する手掛かりが得られなかった。現在のConnect方式はREQ-05／AT-04を満たさない。全FFGL API・全ホスト構成での不可能性を証明した結果ではない。タイマーや拍の周回を代わりのトリガーとは扱わず、追加情報の調査、または外部イベント連携などの構成変更案が必要。

## 次の調査手順

FFGL以外の情報源も比較した。次はDLLを変更せずにResolume本体のOSC出力を観測する。[別情報の調査と実機手順（TRIGGER_RESEARCH.md）](TRIGGER_RESEARCH.md)を参照。採用方式はまだ確定していない。

### Resolume本体OSC出力での前進

クリック09とTEMP内のOSC受信04を照合し、クリック34〜38、41〜45で同じColumn 8の `/composition/columns/8/connect`（型`,i`、値1）が繰り返し届くことを確認した。後から値0も届くため、両方を起動として数えない。詳細と未検証事項は[TRIGGER_RESEARCH.md](TRIGGER_RESEARCH.md)の実機結果を参照。これはFFGLのConnectとは別のイベント経路であり、製品用の適用処理は未実装。
