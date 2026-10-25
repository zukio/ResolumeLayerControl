# メンテナンスガイド

今後の編集箇所は、**設定項目なら`definitions/`、動作の変更なら`source/control/`**が中心です。現在のコードでは、両プラグインの共通処理をまとめています。

| 変更したい内容                                             | 主な編集ファイル                                                                                        |
| ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| Layer Controlの表示名・初期値・Duration範囲・Blend選択肢   | [definitions/LayerControl.json](N:/data/C++/ResolumeLayerControl/definitions/LayerControl.json)         |
| Layer Mask Controlの設定項目・初期値・選択肢               | [definitions/LayerMaskControl.json](N:/data/C++/ResolumeLayerControl/definitions/LayerMaskControl.json) |
| 設定値の検査、Target Layerの解決、変更する項目と実値の対応 | [DefinitionPolicy.h](N:/data/C++/ResolumeLayerControl/source/control/DefinitionPolicy.h)                |
| Resolumeに表示するFFGLパラメータ、値の保持、Instance Key   | [DefinedPlugin.h](N:/data/C++/ResolumeLayerControl/source/control/DefinedPlugin.h)                      |
| Active判定、Effect Clipの識別、Composition解析             | [Composition.h](N:/data/C++/ResolumeLayerControl/source/control/Composition.h)                          |
| 通知から設定を反映する流れ、非Active時の復元、読戻し、ログ | [Runtime.h](N:/data/C++/ResolumeLayerControl/source/control/Runtime.h)                                  |
| 現在値との差分判定、競合・重複送信の抑止                   | [StateReconciler.h](N:/data/C++/ResolumeLayerControl/source/control/StateReconciler.h)                  |
| WebSocket通信・接続・終了処理                              | [WebSocket.cpp](N:/data/C++/ResolumeLayerControl/source/control/WebSocket.cpp)                          |
| 本体の`server.xml`読取り・接続設定の検査                   | [ServerSettings.h](N:/data/C++/ResolumeLayerControl/source/control/ServerSettings.h)                    |
| 入力映像のパススルー、OpenGL状態の復元                     | [PassThrough.h](N:/data/C++/ResolumeLayerControl/source/control/PassThrough.h)                          |

保守時は次の点に注意してください。

- **`source/generated/`は直接編集しません。** JSONを変更し、[generate_plugin.py](N:/data/C++/ResolumeLayerControl/tools/generate_plugin.py)で再生成します。
- パラメータの`id`や選択肢の順番は保存済みCompositionに関係するため、安易に変更しません。
- `source/control/`の変更は、Layer ControlとLayer Mask Controlの両方に影響します。
- ビルド・テスト手順は[SETUP.md](N:/data/C++/ResolumeLayerControl/docs/SETUP.md)を参照してください。通信ランタイム変更時は`tests/test_websocket_runtime.py`も対象です。
