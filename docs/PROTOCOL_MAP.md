# WebSocket対応表

対象記録：Arena 7.27.1（15990）、利用者の2026-10-22〜23試験。今回の実装・解析日：2026-10-23。

## 根拠

- `docs/evidence/ws-native-plugin-20261023.json`に保存した実受信Composition。Clip内のEffectとparams、Layerのtransition／maskmodeを解析した。
- Node-REDによるTransition／Mask設定成功は仕様書冒頭の利用者報告。新DLLによる実機成功とは区別する。
- [Resolume公式WebSocket API](https://resolume.com/support/en/websocket-api)でendpoint、接続直後のComposition、`get`／`set`、by-id、応答typeを確認（2026-10-23、版固定資料ではない）。

| 項目 | 確認内容 | 実装／残る確認 |
| --- | --- | --- |
| endpoint | 公式資料：`ws://address:port/api/v1` | hostは127.0.0.1固定、portは本体の`Documents/Resolume Arena/Preferences/server.xml`から取得。実測保存値8080（Arena 7.27.1）、既定値へのフォールバックなし。enabled=1かつaddress=0.0.0.0／127.0.0.1のみ接続 |
| Composition | 記録：トップレベル`layers`配列 | このメッセージだけが新しい差分同期の契機 |
| Layer番号 | 記録のlayers配列、既存inspect_websocket.cjsは番号-1を使用 | 初版は番号-1。Group入りCompositionは送信停止、移動後は新しい順番を使用。実機番号対応AT-11未確認 |
| Clip ID | `layers[].clips[].id` | int64。起動間で固定しない |
| Active候補 | `connected.value`にDisconnected／Previewing／Connected／Connected & previewing | 後2つだけ通過。実機の全状態遷移は未確認 |
| Effect | Clipの`video.effects[].params` | 既存Probeの構造は実測済み。新しいInstance Keyの伝播は未確認 |
| Bypass | `video.effects[].bypassed.value` | true・不明の場合送信停止 |
| Transition方式 | `layer.transition.blend_mode`、ParamChoice、valueは文字列、options配列、id | optionsにdesired／actualが一意に存在することを確認して文字列set。Layer Blend Modeは参照しない |
| Duration | `layer.transition.duration`、ParamRange、min=0、max=10、valueは実数（保存ログ） | UI／内部は秒、0〜10。実行時min/maxも検査。値は正規化せず送る。読戻しの精度・同Columnへの適用は未確認 |
| Mask | `layer.maskmode`、ParamChoice、Disabled／One Below／All Below | Apply Mask OffとDisabledを分離 |
| parameter ID | 各パラメータの`id` | 最新Compositionから`/parameter/by-id/<id>`へ解決。ID固定なし |
| 送信 | `{"action":"set","parameter":"/parameter/by-id/<id>","value":...}` | 絶対値のみ。新DLLの実機確認は未実施 |
| 読戻し | `{"action":"get","parameter":"/parameter/by-id/<id>"}` | set送信後に同一WebSocketで送信 |
| 応答 | 公式：parameter_set／parameter_get／parameter_updateとpath | set応答を読戻し成功と混同しない。get／updateまたは後続Compositionの値一致を確認 |
| 位置通知 | 記録：`parameter_update`と`.../transport/position` | 同期を開始しない |

対応する50種類のTransition選択肢は`source/plugins/LayerControl/LayerPolicy.h`の`BlendModes()`に、取得ログと同じ順序で固定した。送信時はホストのoptionsを検証し、文字列を送るためホスト側indexの並びに依存しない。未知の選択肢へ置き換えない。

ローカルテストサーバーで同形式の通信が通ったことは、Resolumeのプロトコル受入を証明しない。Composition通知の発火条件、設定後の通知順序、Duration精度、同じColumnの映像へ間に合うかは実機試験が必要。
