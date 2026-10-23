# 検証記録

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
