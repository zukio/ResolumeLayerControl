# 検証記録

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
