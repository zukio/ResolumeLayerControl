# 開発エージェント向け指示

このファイルはリポジトリのルートに置くAGENTS.mdである。本プロジェクトの実装・修正・調査を行うエージェントに適用する。

## 目的と参照順序

ResolumeのWebSocket Composition通知を契機に、ActiveなEffect Clipの設定をTarget Layerへ反映し、非Activeになったら管理開始前の値へ復元するC++／FFGLプラグインを保守する。Column再トリガーを一意に検出する設計ではなく、同じColumnで始まる映像にTransitionが間に合うかは別途評価する。

最初にdocs/SPECIFICATION.mdを読む。ユーザーの指示を優先し、仕様との相違や新しい判断は仕様書へ反映する。本ファイルと仕様書の不整合を黙って実装で解釈しない。

次にdocs/SETUP.md、docs/VALIDATION.md、必要に応じてdocs/ARCHITECTURE.mdとdocs/PROTOCOL_MAP.mdを読む。過去の文書や古いログより、現行コード・最新の実機結果・ユーザーの最新報告を優先し、古い状況記述をそのまま引き継がない。

## 守る要求

- 要求にない機能禁止や併用制限を、未検証という理由だけで追加しない。未検証は未検証として報告する。エラーの影響は関係する対象・要求に限定し、新しい正常な操作まで継続して拒否しない。制限が必要なら、具体的な故障例・影響範囲・復帰条件を示す。「誤動作を防ぐ」だけでなく「要求された操作を成立させる」ことまで含めて設計する。
- 制御用Clipは説明用の仮称であり、正式な機能名はEffect Clipである。
- Layer ControlはLayer TransitionのBlend ModeとDurationを制御する。Layer Mask ControlはLayer Mask Modeを制御する別プラグインであり、両者のTarget Layerは独立している。
- Transition Blend ModeとLayer Blend Modeを混同しない。
- MaskのTarget Layerは、マスクされる側ではなくMask Modeを変更する側である。
- 利用者はLayer番号と意味のある選択肢を編集する。WebSocketのパス・パラメータIDを通常操作で要求しない。プラグインのOSC互換送信経路は作らない。
- Resolume本体は改造しない。
- Effect Clipの配置LayerとTarget Layerを独立して扱う。
- Effect ClipがActiveになった時のLayer設定反映と、非Activeになった時のベースライン復元を維持する。別の同種Control Clipが同じ項目を管理するときは、復元せず新しい指定値へ切り替える。
- 同じColumnで始まる映像にTransitionが適用されるかを必ず検証する。Layer値の変更だけでは要求達成としない。
- 初版は1インスタンス1対象Layerとする。Enabledはプラグインごとの設定に従い、Apply Transition／Apply MaskのようなApplyスイッチを再導入しない。

## 調査・報告の原則

- 「要求」「資料確認済み」「設計案」「実機確認済み」「未確認」を区別する。
- WebSocketのパス／パラメータID、列挙値、ポート、SDKの通知仕様、ビルドコマンドを推測で確定しない。
- ffgl-effectテンプレートの実体・版・ライセンスは未確定である。名前からURLやファイルを創作しない。
- 公式資料と対象環境から取得した情報を優先する。参考情報には出典・対象版を残す。
- コンパイル成功、WebSocket接続・送信、Resolumeの値の変更、読戻し、同一Columnの映像への反映を別々に判定する。
- 実機がない場合は実行可能な範囲を進め、未実施の試験と再現手順を残す。実機成功を報告しない。
- 不可能または未成立と判明した項目は理由と影響を記録する。代替方式を元の仕様達成と呼ばない。
- 既に認められた範囲の調査・実装・修正は進める。操作体系や構成を変える必要がある場合に、その差分を具体化して示す。

## 保守作業の進め方

1. 仕様書と検証記録を読み、今回変更に関係する要求・既知の制約・未確認項目を特定する。
2. 既存のソース、定義JSON、生成物、プロジェクト設定、テストを確認し、既存パターンを優先する。
3. 変更に関係する最小のテストを追加・実行する。ホスト依存の条件は実機試験と分けて記録する。
4. 実機試験を行った場合はArenaの製品・版、構成、操作、観測結果をdocs/VALIDATION.mdへ追記する。未実施なら、再現手順と未確認事項を残す。

現行のビルド・導入手順はdocs/SETUP.mdを参照する。AT-01〜AT-17の合格条件は仕様書7節に従う。部分的な成立を全体の合格として報告しない。

LayerControl／LayerMaskControlのビルド設定はCMakeへ集約する。既存のbuild/windowsの対象vcxprojはCMakeへの委譲用であり、独自のソース一覧・依存・コンパイル設定を追加しない。Windowsの配布用DLLはbinaries/x64/<構成>/へ出力する。build-control-plugins／build-control-testsは生成物で手編集しない。

## Composition通知・Active判定・状態同期

- WebSocketのComposition状態通知を契機にする。定期ポーリングや再生位置の巻き戻りをトリガー判定に使わない。
- `parameter_update`の`path`で再生位置などの更新対象を判別し、位置更新だけで同期処理を開始しない。
- FFGLインスタンスをComposition内のClip IDへ一意に対応付ける方法を確立する。複製や複数配置で曖昧なら送信しない。
- 現行実装は`connected.value`の`Connected`／`Connected & previewing`をActiveとして扱い、`Disconnected`／`Previewing`では制御しない。既知の実機確認結果を維持し、状態判定を変える場合は状態別に再試験する。
- ActiveかつEnabledなら指定値とTarget Layerの**最新の実値**を比較する。異なる制御項目だけ`set`し、一致項目は送信しない。
- **以前に送信した値の記録を現在値の代わりにしない。** 別操作でTarget Layerが変わった場合は、次のComposition通知で差分を検出し指定値へ戻す。
- 同一Column再トリガーと再生中編集を区別する必要はない。Active中編集は次の通知で反映する。
- 未再生Clipの選択・プレビュー・編集は送信しない。初期化そのものによる無条件送信もしない。
- Active Clipがなくなった場合は、保存したベースラインへ戻す。復元対象値の読戻しを確認し、単なる送信成功を復元成功と扱わない。
- WebSocketの`set`による通知が再入力されても、最新値の比較と未完了要求の管理で無限再送させない。
- FFGLの`Connect()`／`SetTime()`を再トリガー検出の前提にしない。
- Auto Pilot、Fader Start、Bypass、Opacity 0、Solo、読込後Active状態を個別に検証する。

## 実装構成

現行の責務分割は`source/control/`（Runtime、Composition、StateReconciler、WebSocket、パススルー等）、`source/plugins/`（FFGLアダプターと機能別ポリシー）、`definitions/`（JSON定義）、`tools/generate_plugin.py`（ビルド時生成）である。既存構成があれば最小変更を優先し、同じ責務を重複実装しない。

- JSON定義を変更したら生成処理と定義テストを実行する。生成ヘッダーは原則として生成コマンドで更新し、手編集しない。
- `build/windows/FFGLPlugins.sln`の現行対象は`LayerControl`と`LayerMaskControl`。旧OSC Probeはソリューションから外れているが、ソース、CMake、テストから参照されているため、削除前に依存を調べる。
- Enabledの初期値は定義ごとに異なる。現行JSONではLayer ControlがON、Layer Mask ControlがOFF。変更時は意図と関連テストを確認する。
- 起動時のパラメータ復元と、Composition通知での状態同期を区別する。
- 描画処理中にネットワーク応答待ち・ディスク書込み待ちを行わない。
- 毎フレーム送信しない。通知を受けても一致する値は送信しない。
- 未完了の同一差分要求を多重送信しない。キューは有限とする。
- Active Clip切替・復元と未完了要求の世代を管理し、古いClipの応答や通知が新Clipの値を上書きしないようにする。
- 終了時にWebSocket・ワーカー・GL資源を解放する。例外をホスト境界へ漏らさない。

## プロトコルと失敗処理

- ローカルResolume WebserverへのWebSocketを受信・送信の統一路線とする。実ポートは現場で確認する。プラグインからOSC／UDPへ送らない。
- Layer番号／index、パラメータID、型、Duration単位、Blend ModeとMask Modeの列挙値を一か所で解決する。
- 絶対値設定を用い、Toggleや相対操作を使わない。
- 不明なBlend Mode、Duration、Layer番号、Clip対応付けを黙って補正しない。
- 実際のTarget Layer現在値を取得できなければ、前回送信値だけを根拠に一致と判定しない。
- WebSocketの送信成功・応答・読戻し・映像適用を分けて報告する。複数項目の原子的更新は保証しない。
- 部分反映時の自動ロールバックはしない。再送は明示的な設計と検証がある場合のみ行う。
- 同Target Layerの同項目を複数のActive Clipが競合して変更する構成は初版の保証対象外とする。

## 映像とMask

- 入力テクスチャの色・Alpha・向き・サイズを保つパススルーを第一候補とし、GL状態を復元する。
- 透明出力だけでEffect Clipを無害化できると考えない。
- 配置Layerで既存Clipが置き換わることを考慮し、最初は制御用の配置Layerを確保する。
- Opacity 0やBypassでもプラグインが処理されると仮定しない。
- One Belowの作用先、All Belowの範囲、Group境界を実機で確かめる。
- Mask Mode変更は、マスク用Clipの選択・再生を代行する機能ではない。
- 通常映像と独立したMask動画を同一Layerで同時に扱う機能を追加したことにしない。
- Mask Mode切替と映像切替の前後関係を観測し、一瞬の露出も評価する。
- LayerやGroupの削減は構成上の効果として評価する。削減を無条件に保証しない。

## 保存と互換性

- FFGLパラメータの番号・型・列挙値の意味を、公開後に不用意に変更しない。
- Compositionの保存・再読込、Clip複製、Preset保存・再適用をそれぞれ確認する。
- 接続状態や送信済みフラグを永続化しない。Composition状態から取得するClip IDは再起動後も同一と仮定しない。
- ホスト版で候補一覧が変わる項目は対応範囲と互換性を明示する。

## 検証と記録

仕様書のAT-01〜AT-17を受入試験として使う。現在、利用者の実機報告ではLayer ControlのActive時のTransition Blend変更と、別ClipをActiveにした直後のベースライン復元が確認済み。Transitionが同じColumnで始まる映像に適用されること、Layer Mask Controlの動作など、残る条件は個別に確認する。ホスト動作が関係する項目をモックだけで合格にしない。

ユニットテストを追加する場合は、値の変換、Active判定、Clip ID対応、現在値との差分、自己通知ループ抑制、古い要求の破棄等、故障の影響が大きいロジックを対象とする。文書だけの変更や実装をそのまま写したテストは不要。

自動テストはdocs/SETUP.mdに記載されたCMake／CTest手順を基本とする。通信ランタイムを変更した場合は`tests/test_websocket_runtime.py`も実行する。テスト成功は実機試験の代わりにならない。

検証資料は以下のパスを使用する。実ファイルの有無と内容はリポジトリで確認する。

| パス案               | 内容                                                        |
| -------------------- | ----------------------------------------------------------- |
| docs/VALIDATION.md   | 対象環境、試験手順、期待結果、実結果、制限                  |
| docs/PROTOCOL_MAP.md | 実測済みのWebSocketパス・Clip状態・ID対応・型・単位・列挙値 |
| docs/SETUP.md        | 確認済みのビルド、導入、接続、復旧手順                      |

ビルド手順は実際の構成を読んでから確定する。実行していないコマンドや存在しないターゲットを「確認済み」として書かない。

## 完了時の報告

変更内容、目的、実施した検証、その結果、未実施項目、残る制約を短く報告する。実機未検証なら明記する。次の担当者が同じ環境と手順で再現できる状態で引き継ぐ。
