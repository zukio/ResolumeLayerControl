# ビルドと導入

## プラグイン

- **Layer Control**：Target Layer、Transition Blend、Durationを設定する。
- **Layer Mask Control**：Target Layer、Mask Modeを設定する。Transition用とは独立したTarget Layerを持つ。

どちらも同じ共通基盤を使うFFGLプラグインです。WindowsはDLL、Macはbundleを生成する構成です。Macビルド・実機動作は未検証です。旧OSC Probeは現行の操作手順には使いません。

## ビルド

Visual Studio 2022とC++デスクトップ開発ツールを使用します。

1. `build\windows\FFGLPlugins.sln`を開く。
2. 構成を`Release`、プラットフォームを`x64`にする。
3. `effects`から`LayerControl`または`LayerMaskControl`を選んでビルドする。

Developer PowerShellからビルドする場合：

```powershell
msbuild build\windows\FFGLPlugins.sln "/t:effects\LayerControl" /p:Configuration=Release /p:Platform=x64
msbuild build\windows\FFGLPlugins.sln "/t:effects\LayerMaskControl" /p:Configuration=Release /p:Platform=x64
```

DLLは`binaries\x64\Release\`に出力されます。配置するのはDLLのみです。DLL隣接JSONは読み込みません。

## 両OS共用のCMakeビルド

WebSocket++／Asio／TinyXML-2の固定版を`deps/`に同梱しています。ネットワークからのビルド時ダウンロードはありません。C++17、CMake、Python 3が必要です。

Windows（VS2022／x64）：

```powershell
cmake -S . -B build-control-plugins -G "Visual Studio 17 2022" -A x64 -DFFGL_BUILD_EXAMPLE_PLUGINS=OFF -DRLC_BUILD_PLUGINS=ON
cmake --build build-control-plugins --config Release --target ffgl-plugin-layer-control ffgl-plugin-layer-mask-control
```

Mac用の実行予定手順（このWindows環境では未実行）：Xcode Command Line Toolsを用意し、以下を実行します。macOS 11以上をビルド対象とする例です。Intel／Apple Silicon両対応はSDK・ホストを含む実機検証が必要です。

```sh
cmake -S . -B build-control-plugins -DCMAKE_BUILD_TYPE=Release -DFFGL_BUILD_EXAMPLE_PLUGINS=OFF -DRLC_BUILD_PLUGINS=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-control-plugins --target ffgl-plugin-layer-control ffgl-plugin-layer-mask-control
cmake -S tests -B build-control-tests -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-control-tests
ctest --test-dir build-control-tests --output-on-failure
```

CMake出力は`build-control-plugins/source/plugins/LayerControl/`、`LayerMaskControl/`配下です。Windowsでは`Release/`下のDLL、Macでは`.bundle`全体を使います。Macの配置先はユーザーの`Documents/Resolume Arena/Extra Effects`です。Macの設定はFoundationから取得したDocuments配下の`Resolume Arena/Preferences/server.xml`を参照します。実行名候補・XML形式を確認できない場合は接続しません。

Mac上で本体の実行名、XML、接続、終了、パススルー映像を確認してから配布可否を判定してください。署名・公証・Gatekeeperを含む配布試験は未実施です。依存ライセンスは`deps/CONTROL-LICENSES.txt`を配布物へ添付します。

## 導入と設定

1. Resolume Arenaを終了する。
2. 使用するDLLをArenaの`Extra Effects`フォルダーへコピーする。通常は`%USERPROFILE%\Documents\Resolume Arena\Extra Effects`です。
3. Arena本体のPreferences → WebserverでWebserverを有効にする。Listen Addressは`0.0.0.0`または`127.0.0.1`にする。
4. 本体設定を保存してArenaを再起動する。プラグインはWindowsのDocuments既知フォルダー配下の`Resolume Arena/Preferences/server.xml`からポートを取得する。Documentsの移動にも追従する。対象はWindows／Arena（設定形式の実測は7.27.1 rev15990）。別ホストは接続しない。
5. 専用のEffect Clipに目的のプラグインを追加する。Enabledの初期値はLayer ControlがON、Layer Mask ControlがOFF。配置LayerとTarget Layerは別々に指定し、Instance Keyは編集しない。

`LayerControl.json`／`LayerMaskControl.json`と`RLC_WS_PORT`は廃止しました。残っていても参照されません。`definitions/*.json`はビルド用の機能定義なので削除しません。

ログが必要な場合だけ、Arenaを起動する環境に`RLC_WS_LOG`でログファイルの出力先を指定します。親フォルダーは事前に作成してください。日本語を含むパスに対応します。未指定ならファイルログは作成せず、デバッガ出力のみです。指定したログファイルを開けない場合は接続せず、デバッガ出力に理由を記録します。

設定は共有Runtime開始時にワーカーで一度だけ読みます。Webserver無効、XML欠損・破損、ポート不正、非対応の待受アドレスでは接続しません。ポート変更後の自動再接続・設定監視はありません。変更後や接続失敗時は本体設定を確認してArenaを再起動してください。Webserver準備前の接続失敗を含む起動順序は実機未検証です。

## 確認

利用者の実機確認では、Layer ControlのTransition Blend Mode変更と、別ClipをActiveにしたときの復元を確認済みです。同じColumnで始まる映像へのTransition反映は未確認です。Layer Mask ControlのMask Mode変更も実機で未確認です。

送信ログの`set_sent`は共通通信の送信キューへの受理を示します。実送信完了、Resolumeでの反映、映像への効果を保証しません。ログと再現手順は[VALIDATION.md](VALIDATION.md)を参照してください。

自動テストはリポジトリのルートで実行します：

```powershell
cmake -S tests -B build-control-tests -G "Visual Studio 17 2022" -A x64
cmake --build build-control-tests --config Release
ctest --test-dir build-control-tests -C Release --output-on-failure
```
