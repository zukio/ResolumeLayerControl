# ビルドと導入

## プラグイン

- **Layer Control**：Target Layer、Transition Blend、Durationを設定する。
- **Layer Mask Control**：Target Layer、Mask Modeを設定する。Transition用とは独立したTarget Layerを持つ。

どちらもWebSocketを使う別個のFFGL DLLです。旧OSC Probeは現行の操作手順には使いません。

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

DLLは`binaries\x64\Release\`に出力されます。プラグインを追加するには、DLLに加えて同名の設定JSONも配置します。

## 導入と設定

1. Resolume Arenaを終了する。
2. 使用するDLLと、同じプラグイン名のJSONをArenaの`Extra Effects`フォルダーへコピーする。通常は`%USERPROFILE%\Documents\Resolume Arena\Extra Effects`です。
3. JSONの`port`をArenaのWebserver設定で確認したポートに合わせる。例：

   ```json
   { "port": 8080, "log": "C:/path/to/layercontrol-live.jsonl" }
   ```

   `LayerControl.dll`には`LayerControl.json`、`LayerMaskControl.dll`には`LayerMaskControl.json`を使います。8080は今回の検証環境で確認した値で、他の環境のポートを保証しません。ログ出力が不要なら`log`は省略できます。

4. ArenaのWebserverを有効にし、ポートを確認してからArenaを再起動する。
5. 専用のEffect Clipに目的のプラグインを追加する。`Enabled`は初期状態でONです。配置Layerと`Target Layer`は別々に指定します。`Instance Key`は編集しません。

## 確認

利用者の実機確認では、Layer ControlのTransition Blend Mode変更と、別ClipをActiveにしたときの復元を確認済みです。同じColumnで始まる映像へのTransition反映は未確認です。Layer Mask ControlのMask Mode変更も実機で未確認です。

送信ログの`set_sent`は送信成功を示すだけで、Resolumeでの反映や映像への効果を保証しません。ログと再現手順は[VALIDATION.md](VALIDATION.md)を参照してください。

自動テストはリポジトリのルートで実行します：

```powershell
cmake -S tests -B build-control-tests -G "Visual Studio 17 2022" -A x64
cmake --build build-control-tests --config Release
ctest --test-dir build-control-tests -C Release --output-on-failure
```
