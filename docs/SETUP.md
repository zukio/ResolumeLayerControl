# WebSocket版のビルド・導入・検証

2026-10-23。`Layer Control`（仮ID RLCW、0.3）は検証用DLL。実機受入未完了。旧`Layer Ctrl Probe`とは別のプラグインであり、既存CompositionのProbeを自動変換しない。

## 今回確認した環境とビルド

- Windows x64、OSVersion 10.0.26300.0。
- Visual Studio 2022 Community、v143、MSVC 19.44.35226.0、Windows SDK 10.0.28000.0。
- 同梱SDK：CMake上2.2。リポジトリ開始時HEAD `290d8f0`。SDK上流commit／ffgl-effectテンプレートとの対応は未確定。
- 同梱GLEW 2.1.0、nlohmann/json 3.11.3、WinHTTP。

リポジトリのルートで実行したコマンド：

```powershell
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' build/windows/LayerControl.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
```

出力：`binaries/x64/Release/LayerControl.dll`。Releaseは同梱GLEWに合わせて静的CRTを使用する。GLEWのPDB欠落警告は残る（GLEWのデバッグ情報がない）。Debug構成のビルドと実行は今回未確認。

Visual Studioでは`build/windows/LayerControl.vcxproj`を直接開ける。`build/windows/FFGLPlugins.sln`の`effects`にも`LayerControl`を追加済み（Release|x64を`MSBuild FFGLPlugins.sln "/t:effects\LayerControl"`でビルド確認）。slnを使う場合は`LayerControl`を選んでビルドする。CMake側にもターゲットを追加したが、プラグイン全体のCMakeビルドは今回未検証。確認済みのDLLビルド経路は上記vcxproj。

## 自動試験

実行済み：

```powershell
cmake -S tests -B build-control-tests -G 'Visual Studio 17 2022' -A x64
cmake --build build-control-tests --config Release
ctest --test-dir build-control-tests -C Release --output-on-failure
& 'C:/Users/A-SUZUKI/.pyenv/pyenv-win/versions/3.12.0/python.exe' tests/test_websocket_runtime.py build-control-tests/Release/runtime_smoke.exe
```

Pythonパスは今回の環境で実際に動作したもの。他の環境ではPython 3の実体を指定する。PATH上のpythonは、この環境ではpyenvのバージョン未設定で動かなかった。

C++テストは合成状態と保存済み実受信ログを検査する。通信試験は空きポートのローカルテストサーバーを使い、Resolumeの状態を変更しない。結果をAT-01〜AT-17の実機合格に代用しない。

## 接続設定と導入手順

DLLと同じ場所の`LayerControl.json`でも接続設定を指定できる。環境変数があればそちらを優先し、どちらにもportがなければ接続しない。ファイルは最大4 KiB。今回の実機ではserver.xmlの8080を確認して、次の内容を配置した。

```json
{ "port": 8080, "log": "N:/data/C++/ResolumeLayerControl/docs/evidence/layercontrol-live-20261023.jsonl" }
```

配置先は`C:/Users/A-SUZUKI/Documents/Resolume Arena/Extra Effects`。利用者によるArena再起動後に初回DLLの認識・接続・キー公開を確認した。active_clip参照重複修正と表示名変更後のDLLは差替え待ち。隣接JSON経路は通信試験コマンドへ`--config`を追加して確認済み。未確認の受入項目は[VALIDATION.md](VALIDATION.md)参照。

1. Arenaを終了する。既存ProbeのDLLは別名のまま保存し、新DLLをArenaで設定しているFFGLプラグインフォルダーへ配置する。実際の配置先はPreferencesで確認する。
2. ArenaのWebserverを有効にし、表示される実ポートを確認する。OSC設定は変更しない。
3. `LayerControl.json`に実ポートとログの保存先を設定する。代わりに起動用PowerShellへ`RLC_WS_PORT`と`RLC_WS_LOG`を設定し、そのPowerShellからArenaを起動してもよい。今回の実ポートは8080だが、他の環境の既定値とはみなさない。
4. 専用の配置LayerにEffect Clipを作り、`Layer Control`を追加する。EnabledはOffが初期値。Target Layerは配置Layerと独立して指定する。
5. `Instance Key`を編集せず、ログのgate.evidence.clip_id／connectedが当該Clipと一致することを先に確認する。キーがCompositionに存在しない場合は送信されない。
6. 複製したClipも含めて一意な対応が確認できてから、テスト用CompositionでEnabledをOnにする。Apply Transition／Apply Maskを必要な項目だけOnにする。
7. 以下の順で受入試験を進め、旧値・指定値・読戻し・実映像を別々に記録する。

`Instance Key`は実行時の自動識別情報。利用者がClip IDやWebSocket pathを手入力する仕組みではない。保存／複製された値は新インスタンスで再生成される。ホストへの更新伝播が成立しなければgateは通らない。

## 優先する実機試験

1. **段階2／UNK-04・09・12**：未再生、Previewing、Connected、Connected & previewingの各状態でClip IDとキーを照合。Clip複製、複数配置、Preset、再読込で取り違えがないことを確認。Layer／Compositionへの配置では送信しないことも確認する。
2. **AT-01・03・04・05・16・17**：Enabled On、Target Layerを別Layerにして差分だけset。一致中の再通知で送らない。手動でTarget実値を変更して次のComposition通知で戻る。Active中編集は次の通知で適用、Previewing中編集では適用しない。
3. **AT-02**：旧Durationを0、新Durationを5秒など明確な差にし、同じColumnで映像とEffect Clipを開始。録画から今回の切替が5秒になったか判断する。値表示が5秒になっても映像が即時切替なら不合格。方式も別に確認する。
4. **AT-09・10**：Apply Mask Off、Disabled、One Below、All Belowを個別に試す。独立したMask動画・白黒・中間調・Alphaを使い、切替直後の一瞬の露出も録画する。
5. **AT-06〜08・11〜15**：保存・再読込・複製・Preset、色／Alpha／向き／サイズ、停止・Eject、Layer挿入移動削除、競合、高速切替、Bypass、Opacity 0、Solo、Auto Pilot、Fader Start、長時間と終了を確認する。Group入りCompositionは現在の実装では送信を止める。

段階2が未成立なら後工程の実装量を理由に合格としない。同じColumnの映像に間に合わない場合、通知後同期の制約として記録し、外部トリガー方式をこの仕様の達成と呼ばない。

## 診断と復旧

`RLC_WS_LOG`指定時はワーカースレッドがJSONLを追記する（1 Runtimeにつき最大16 MiB追記）。未指定ならWindowsのDebug出力のみ。描画中にログ書込みを待たない。

| event                                | 意味                                                  |
| ------------------------------------ | ----------------------------------------------------- |
| connected                            | WebSocket接続が成立                                   |
| gate                                 | ownerキー、状態判定、Clip ID／connected、Target Layer |
| compare                              | 実値と指定値の比較。一致項目も記録                    |
| set_sent                             | WinHTTP送信成功。サーバー反映の保証ではない           |
| server_response                      | parameter_set応答。読戻し・映像適用とは別             |
| readback                             | get／update／後続Compositionで指定値と一致            |
| readback_timeout                     | 5秒超の未完了要求を次の受信時に検出。自動再送しない   |
| invalid_state／server_error／failure | 未対応構成、不正値、サーバーエラー、通信失敗等        |
| disconnected                         | 接続終了と未完了件数                                  |

切断後は自動再接続しない。設定を確認し、すべてのインスタンスを解放して再生成するか、Arenaを再起動する。EnabledのOff/Onだけでは通信を再開しない。タイムアウトしたpathの再送も同じ接続内では行わない。Clip停止時は既に変更したLayer設定を復元しないので、必要な値は利用者が戻す。

## 汎用化

**現状の仕組み**

- JSON(`definitions/LayerMaskControl.json`)から、ビルド時に `source/generated/LayerMaskControl.h` が生成されます(`tools/generate_plugin.py`)。
- そのヘッダを、`source/plugins/LayerMaskControl/LayerMaskControl.cpp`(約20行)が読み込み、共通のC++基盤(`source/control/`)と組み合わせます。
- 出力は `binaries\x64\Release\LayerMaskControl.dll` です。Layer Control とは別のDLLで、別のEffectとして Resolume に出ます。

**ビルドの状態**

- `build\windows\LayerMaskControl.vcxproj` は存在し、初回に単体でビルドして成功しました。
- ただし `FFGLPlugins.sln` には未追加です(LayerControl は追加済み)。追加が必要です。
- Mask の実機確認はしていません。さきほど出力先から Mask の DLL を削除したので、今は存在しません。必要ならビルドし直します。

**新しいプラグインを増やす手順**

1. `definitions/` に新しいJSONを書く。
2. `source/plugins/` に、20行ほどの `.cpp` と `CMakeLists.txt` を作る。
3. `build\windows\` に、既存の vcxproj を複製して、名前とGUIDを変える。

つまり、JSONだけでは新しいDLLは増えず、ビルド定義が1つずつ必要です。
