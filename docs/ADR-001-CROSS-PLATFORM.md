# ADR-001: WindowsとmacOSで制御コードを共用する

**Status:** Accepted（Mac実機検証は未完了）  
**Date:** 2026-10-23  
**Deciders:** 利用者が両OSで共通実装とする方針を指定。ライブラリ選定は実装時判断。

## Context

Active判定・差分同期・復元を別々に保守せず、同じソースからWindows DLLとMac bundleを生成する。描画処理に通信待ちを持ち込まず、有限キューと終了時キャンセルを維持する。

## Decision

- WebSocket++ 0.8.2＋standalone Asio 1.28.0を両OSで共用する。WinHTTP経路は削除。依存ヘッダーは`WebSocket.cpp`に隔離し、FFGL SDKのWindowsヘッダー定義との干渉を防ぐ。
- XMLはTinyXML-2 10.0.0、ファイルログはC++標準ライブラリで共用。`PlatformWindows.cpp`／`PlatformMac.mm`のみがDocuments取得、ホスト識別、環境変数のOS文字列変換、デバッグ出力、UUID生成を担当する。
- 制御用の通信は従来どおり127.0.0.1のWebSocketのみ。再接続・再送・ポーリングは追加しない。
- 依存ソースは版固定で同梱する。配布物に`deps/CONTROL-LICENSES.txt`を添付する。取得URLとSHA-256は`deps/control-dependencies.json`に記録。

## Options Considered

| 案 | 利点 | 負担・判断 |
| --- | --- | --- |
| WebSocket++／Asio共用 | メッセージ上限、RFC6455処理、イベント待ち、キャンセルを利用可能 | 依存ヘッダーが増える。固定版の更新・互換性確認が必要。採用 |
| WinHTTPを維持しMac側を別実装 | Windows既存経路の変更が小さい | 通信処理を二重に保守し、利用者の共通化方針に合わない |
| Sokol | 描画API等の共通化に使える | 本件のWebSocket・XML移植を解決しないため採用しない |

## Consequences

RuntimeのワーカーがAsioイベントを駆動する。受信は1メッセージ16 MiB、待機キュー32件／32 MiB、送信待機量1 MiBに制限する。`set_sent`は送信キューへの受理を示し、OS送信完了・ホスト反映とは区別する。ライブラリに渡した要求の取消しは保証しない。読戻しによる成功判定は維持する。

MacのDocuments配下の配置は[Resolume公式Directory List](https://resolume.com/support/en/directory-list)で確認した。Macのserver.xml形式、ホスト実行名、bundleロード、起動順序は実機未確認。候補が一致しない場合は接続せずログに残す。Mac対応済みの製品としての配布判定には実機試験が必要。

## Action Items

1. [x] 共通通信・XML・ログと薄いOSアダプターを実装
2. [x] Windows／Macを対象にしたCMakeビルド構成・共通試験を整備
3. [ ] Macでビルド、server.xml・実行名確認、実ホストでロード・通信・終了試験
4. [ ] 両OSでAT-02を含む未完了の実機受入試験

資料：[WebSocket++](https://github.com/zaphoyd/websocketpp/tree/0.8.2)、[Asio](https://github.com/chriskohlhoff/asio/tree/asio-1-28-0)、[TinyXML-2](https://github.com/leethomason/tinyxml2/tree/10.0.0)。確認日2026-10-23。
