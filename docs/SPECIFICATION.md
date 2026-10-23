# Resolume Layer設定制御プラグイン 仕様・設計書

文書版：0.4 ／ 初版：2026-10-22 ／ 改訂日：2026-10-23 ／ 状態：状態同期方式の検証用実装あり（実機受入未完了）

### 0. 範囲の整理（0.4版で改訂）

依頼の本題は、**Layer TransitionのBlend ModeとDurationをColumnのトリガーで切り替えること**（Target Layer、Transition Blend Mode、Durationの3項目）である。Mask Modeは「ついでのご相談」であり、実現可否が不明な別機能として扱う。

- **初版の対象は`Layer Control`のみとする。** 項目はEnabled、Target Layer、Duration、Transition Blend（表示名。FFGLの名称制限16文字のため「Mode」を省く）に限る。Instance Keyは内部識別用で編集しない。
- **Maskは別プラグイン`Layer Mask Control`として分離し、初版の合否に含めない。** 項目はEnabled、Target Layer、Mask Modeとする。
- TransitionとMaskはTarget Layerを共用しない。対象Layerが異なり得るため、各プラグインが独自のTarget Layerを持つ。
- 旧案の`Apply Transition`／`Apply Mask`は廃止する。使わない機能はそのプラグインを置かないことで表す。`Enabled`は各プラグインに残す。
- Resolumeが全Effectに付ける共通項目（Blend Mode、Opacity）は本機能と無関係である。
- 以下、4.2（Mask）、AT-09〜AT-11、UNK-03、段階4は**第2段階（Layer Mask Control）**の内容であり、初版の合否や完成報告に含めない。

### 2026-10-23 実装状況と追加した設計判断

WebSocket専用の`Layer Control`を旧OSC診断Probeとは別DLLとして追加した。共通の通信・Composition解析・Clip Resolver・差分同期・診断・映像パススルーと、今回固有のTransition／Maskポリシー・FFGL UIを分離した。構造と拡張方法は[ARCHITECTURE.md](ARCHITECTURE.md)、実測対応は[PROTOCOL_MAP.md](PROTOCOL_MAP.md)、ビルドと試験は[SETUP.md](SETUP.md)を参照。

- Clip対応付けは実行時GUIDを`Instance Key`テキストパラメータとして公開し、Composition内で一意に観測できた場合だけ成立とする設計案。復元／複製された旧キーは採用しない。FFGL VALUEイベントによるArena側への伝播は実機未検証。曖昧・不在なら送信しない。
- 記録に存在する50種類のTransition選択肢とDuration 0〜10秒を検証用UIへ採用。送信時は最新Compositionの型・options・min/max・IDも検査する。未知値を補正しない。
- Group入りCompositionは番号対応の実機確認まで送信しない。Target Layer UI上限1024は実装の上限であり、ホストの最大Layer数を意味しない。
- Webserverポートは本体の保存設定`server.xml`から取得し、不明・不正・無効なら接続しない。DLL内で接続を共有し、受信メッセージ／インスタンス／未完了要求を有限にする。自動再接続・自動再送は初版では行わない。
- 同一項目を複数のActive Sourceが要求した場合は送信を抑止する。新しい通知をまだ受信していない間のホスト側変更、および送信済み要求の取消は保証しない。
- Windows Release DLLのビルド、合成状態・保存ログを使うC++試験、ローカルWebSocketサーバーとの通信試験を実施。これは段階2のClip対応付けやAT-02の映像反映の合格ではない。段階2の未検証条件を残し、独立に試験できる基盤を実装した段階である。

利用者が任意のコマンドを入力するUIは追加しない。ひな型としての拡張はコード側のポリシー交換で行い、追加項目も実値の取得・差分・Active条件を検証する。

### 2026-10-23 設計方針（この版の基準）

- Windows／Mac共通化：制御ロジック・WebSocket++／Asio通信・TinyXML-2によるXML解析・ファイルログを共用し、設定場所取得等だけをOSアダプターへ分離する。同じソースからDLL／bundleをビルドする。Windows側もWinHTTPを廃止する。MacのDocuments配置は公式資料確認済みだがserver.xml形式・実行名・ビルド・ホスト動作は実機未検証。[ADR-001](ADR-001-CROSS-PLATFORM.md)を参照。以下のArena.exeの確認はWindows側に適用し、Mac側はArena／Resolume Arenaという実行名候補を検査する。
- 共通通信の送信成功ログは有限キューへの受理を示す。実送信完了・応答・読戻し・映像適用と区別する。受信は1メッセージ16 MiB、待機32件／32 MiB、送信待機1 MiBを上限とし、超過で制御を停止する。
- 接続設定：本体Webserver設定読取りを標準とし、DLL隣接JSONと`RLC_WS_PORT`を廃止する。WindowsのDocuments既知フォルダー配下の`Resolume Arena/Preferences/server.xml`をRuntime開始時にワーカーで一度だけ読む。実測したArena.exe／Arena 7.27.1の形式を対象とし、他ホストは接続しない。`enabled`は0/1、`port`は1〜65535、`address`は0.0.0.0／127.0.0.1のみ受理し、接続先は127.0.0.1固定。ログは任意の`RLC_WS_LOG`だけで指定する。XML不明・不正時の既定ポートへのフォールバックはしない。保存時期・起動順序は実機未検証。ポート変更の監視・自動再接続は追加しない。[VALIDATION.md](VALIDATION.md)を参照。

- **適用契機はResolume WebSocketのComposition状態通知とする。** 定期ポーリングやFFGLの`Connect()`・`SetTime()`によるトリガー推定を基本方式にしない。
- 通知を受けたら、**対象の制御用Effect Clipが本番出力でActiveか**、そのClipの**指定値とTarget Layerの実際の現在値が異なるか**を照合する。Activeかつ不一致の項目だけWebSocketの`set`で同期する。
- **再トリガーと再生中のパラメータ編集を識別する必要はない。** どちらもActive中の状態同期として扱う。元仕様の「1トリガーにつき1回送信」は廃止する。
- **一致項目は送信しない。** 比較に使うのは「最後に送った値」ではなく、Resolumeから取得したTarget Layerの現在値である。外部からの手動変更を見落とさない。
- 未再生・プレビュー専用のClipはLayer設定を変更しない。**Clipが非Activeになった（または無効化・削除された）ら、適用前にTarget Layerが持っていた値（ベースライン）へ戻す。** 同じ項目を別のLayer Control ClipがActiveで管理している間は復元せず、そのClipの値へ切り替えるだけとする。何も管理しなくなった時だけ復元する（0.4版で変更。旧：自動復元しない）。
- PluginとResolume間の受信・送信はWebSocketに統一する。既存の外部OSC運用には介入せず、プラグインからOSCを送信する互換経路は実装しない。

### この版が根拠にする既存の実測

- 対象環境：Resolume Arena 7.27.1（利用者の2026-10-22〜23試験）。Node-REDからTransition Blend Mode、Duration、Mask Modeの設定変更に成功。Transition Blend Mode／Mask Modeは取得したパラメータIDを利用し、Durationは`/composition/layers/3/transition/duration`への`set`で成功。パラメータIDは固定しない。
- Composition状態データには、Clipごとに`id`と`connected.value`があり、状態選択肢として`Disconnected`、`Previewing`、`Connected`、`Connected & previewing`等が現れる。**このデータを個々のFFGLインスタンスへ確実に対応付ける方法は未検証**。
- 個別の再生位置通知例は`type: parameter_update`、`path: /composition/layers/1/clips/7/transport/position`、`value: 189.28…`。`path`で更新対象を識別できる。再生位置の巻き戻りだけでは再トリガーを識別できない。
- 再トリガーとBlend Mode編集で同種のComposition通知を受信した。通常ループのたびにComposition通知が発生するわけではない。ただし、通知に含まれる状態がActive判定へ利用できるか、対象Clipの同定と対象値の読み取りを含め、実機統合検証が必要。
- 過去のFFGL検証では、同じColumnの5回の再トリガーで`SetTime()`の逆行と`Connect()`再通知は観測されなかった。今回の設計はこれらに依存しない。

本書は要求、実測、設計上の選択、未検証事項を区別する。進捗と試験ログの実体は[VALIDATION.md](VALIDATION.md)および[TRIGGER_RESEARCH.md](TRIGGER_RESEARCH.md)を参照する。

## 1. 目的と完成条件

ResolumeのColumnに置いた制御用Effect Clipの設定に従い、指定LayerのTransition設定（初版）を自動調整する。Mask Modeの調整は第2段階の別プラグインで扱う。設定内容はResolumeのFFGLパラメータUIから編集・保存する。

**完成条件はトリガー回数の検出ではなく、Activeな制御用Effect Clipの指定値とTarget Layerの実値が同期すること。** Composition通知で照合し、一致している間は送信せず、不一致なら設定を更新する。Active中のパラメータ編集も適用対象とし、未再生・プレビュー専用Clipの設定は本番へ適用しない。

ただしColumnトリガーで始まる映像切替については、**その回のTransitionに新しい方式・時間が使われること**を完成条件として維持する。通知後の変更がTransition開始に間に合うかは未検証であり、状態同期の成功だけでこの条件の達成を主張しない。

（第2段階）Mask制御は、同じLayerを通常映像とマスク用で切り替えることで、役割切替だけのLayerやGroupを減らすことを目指す。削減数は保証しない。

## 2. 用語と区別

| 用語 | 本書での意味 |
| --- | --- |
| 制御用Clip | 今回の説明用の仮称。Resolumeの正式機能名ではない |
| Effect Clip | 既存の正式機能。空のClipへエフェクトを追加して作成する [S1] |
| 制御用Effect Clip | 本プラグインを配置し、Layer設定の変更指示を保持するEffect Clip |
| 配置Layer | 制御用Effect Clipを置くLayer |
| Target Layer | 設定を変更する対象Layer。配置Layerとは独立 |
| Transition Blend Mode | 同じLayerでClipを切り替えるときのTransition方式 |
| Layer Blend Mode | 上下のLayerを合成する方式。今回の変更対象ではない |
| Duration | 対象LayerのTransition Time。Effect Clip自体の再生時間とは別 |
| Active | この仕様では本番出力で再生中の状態。`connected.value`が`Connected`または`Connected & previewing`であることを判定候補とする（要実機確認） |
| 状態同期 | Composition通知時に指定値とTarget Layerの現在値を比較し、差分がある項目だけ`set`する処理 |
| Composition通知 | WebSocket購読により受信するComposition状態の通知。位置などの個別`parameter_update`とは区別する |

## 3. 要求・確認済み事項・未確定事項

### 3.1 依頼者の要求

| ID | 要求 |
| --- | --- |
| REQ-01 | Columnごとに異なるLayer設定を持たせる |
| REQ-02 | ResolumeのUIで対象Layerと変更内容を編集・保存する |
| REQ-03 | Target Layer、Transition Blend Mode、Durationを指定する（**初版の対象**） |
| REQ-04 | （第2段階・別プラグイン`Layer Mask Control`）独自のTarget LayerのMask ModeをDisabled／One Below／All Belowから指定する。初版の合否に含めない |
| REQ-05 | Composition通知を契機にActiveな制御用Effect ClipとTarget Layerの状態を照合し、差分だけをWebSocketで適用する。再トリガー単位の送信回数は数えない |
| REQ-06 | 制御目的のClipが意図しない映像変化や既存Clipの停止を起こさない |
| REQ-07 | 通常操作ではWebSocketのパスや生コマンドを管理させず、Layer番号と意味のある選択肢で設定する |
| REQ-08 | Resolume本体を改造しない。既存の外部OSC運用を妨げない |
| REQ-09 | 未再生・プレビュー専用Clipでは設定を変えず、Active中はパラメータ編集の結果を反映する |
| REQ-10 | 現在値の比較によりWebSocket通知の自己再帰・無意味な再送を抑える。ただし別操作で実値が変わった場合は次の同期契機で指定値へ戻す |

### 3.2 公式資料で確認できた事実

「資料確認済み」は「本プラグインで実機確認済み」を意味しない。

| ID | 事実 | 根拠 |
| --- | --- | --- |
| FACT-01 | Effect Clipのエフェクトは、その下で再生中のLayer群の映像へ作用する | [S1] |
| FACT-02 | Layer Transitionには時間とTransition方式の設定がある | [S2] |
| FACT-03 | Mask Layerには通常表示へ戻す状態、直下1Layerへ適用する状態、下位Layer群へ適用する状態がある | [S2] |
| FACT-04 | Maskは再生内容の明暗を利用し、Alphaは黒として扱われる | [S2] |
| FACT-05 | OSC設定画面・OSC編集モードで、操作対象のアドレス・型・範囲を確認する方法がある | [S3] |
| FACT-06 | ResolumeにはREST APIおよびWebSocketによる外部連携機能がある | [S4][S5] |
| FACT-07 | 公式FFGL SDKにはConnect、Disconnect、ProcessOpenGL等のメソッドがある | [S6][S7] |

元構想の「Disable」は要求上の呼称として残す。公式英語マニュアルでは通常表示へ戻す状態を「Disabled」と記載している。製品UIの表記と送信値は対象バージョンで確認する。

### 3.3 暫定設計として置く前提

| ID | 暫定方針 | 確定に必要な事項 |
| --- | --- | --- |
| PRE-01 | C++／FFGL Effectプラグインとする | 対象OS、SDK版、ビルド条件 |
| PRE-02 | 同一PCのResolumeへWebSocket接続する（例：127.0.0.1） | Webserverのポート、起動順、再接続手順 |
| PRE-03 | WebSocketのComposition購読を同期の契機にし、差分を`set`で書き込む | 通知形式、状態の鮮度、送信後通知、エラー応答 |
| PRE-04 | 1インスタンスは1つのTarget Layerを制御する | 複数Layer指定は初版の範囲外 |
| PRE-05 | TransitionとMaskは別プラグインとし、Target Layerを共用しない。Apply項目は持たない | 4.3、4.5 |
| PRE-06 | 非Active化後に、適用直前の値（ベースライン）へ復元する。ベースラインは最初の変更前の実値で、Runtime内メモリのみ（保存しない）。復元は送信後に破棄し再送しない | 実機でのActive外れ検出、Runtime終了時（最後のインスタンス削除）は復元されない |
| PRE-07 | 検証時は制御用Effect Clipの配置Layerを確保する | 既存Clipとの競合、映像影響 |
| PRE-08 | Active判定の候補はClipの`connected.value` | `Connected`・`Previewing`の状態遷移と再生実態を実測 |
| PRE-09 | 対象値の読戻しは通知の最新状態またはWebSocketからの取得で行う | 実際の値を古いスナップショットと取り違えない方法 |

これらは設計方針であり、未検証項目の実機成功を意味しない。

### 3.4 未検証事項と成立条件

| ID | 未検証事項・状態 | 成立と判断する条件 |
| --- | --- | --- |
| UNK-01 | FFGLプラグイン内からWebSocketの購読・取得・`set`を行えるか | Resolume実ホスト上で受信・送信・終了が安定して動作する |
| UNK-02 | Transition Blend Mode／Durationの設定 | **Node-REDからの`set`成功は報告済み**。プラグイン経由の反映と同じColumnのTransitionへの適用順序は未検証 |
| UNK-03 | Mask Modeの3状態 | **Node-REDからの変更成功は報告済み**。プラグイン統合後のマスク表示・動画・Group境界は未検証 |
| UNK-04 | Composition通知時にClipの本番Active状態を正しく判定できるか | 未再生・Previewing・Connected・Connected & previewing・停止・切替で誤判定しない |
| UNK-05 | 同じColumnの映像Transition開始前に設定が有効になるか | 明確に異なる旧値から起動し、今回の切替で新設定が使われる |
| UNK-06 | FFGLの編集・保存・復元ができるか | 再起動・再読込後に値と意味が維持される |
| UNK-07 | 制御用Effect Clipが映像を意図せず変えないか | 色・Alpha・出力・既存Clipの再生が保持される |
| UNK-08 | ffgl-effectテンプレートの実体／版／ライセンス | 上流commitとビルド手順を特定する |
| UNK-09 | **FFGLインスタンスとComposition内Clipの一意な対応付け** | 複製したClipや複数同時起動でも、自分のClipの`connected`と設定値を正しく参照できる |
| UNK-10 | 値の読戻しと通知／`set`の競合 | 手動変更が反映された実値で比較でき、送信による通知ループが発生しない |
| UNK-11 | 旧新Effect Clipの同時Active・通知順序 | 古いClipの指示が後から上書きしない条件を説明できる |
| UNK-12 | Composition通知の発火条件 | 本番トリガー、Active中編集、Target Layer手動変更、初期ロードで同期契機を取りこぼさない |

ffgl-effectという名称だけから配布元を推定しない。SDK同梱AddSubtractとローカルテンプレートの上流版対応は未確認。

## 4. 機能仕様

### 4.1 Transition制御

Composition通知を契機に対象Effect ClipがActiveであることを確認し、その時点の指定値とTarget Layerの現在値を比較する。不一致の項目がある場合のみTransition Blend ModeとDurationを変更する。Layer同士を合成するBlend Mode、LayerのOpacity、対象Clipの速度等は変更しない。

Blend Mode候補は対象版で確認した一覧を使う。表示名と外部制御値の対応を明示し、不明な値をAlpha等へ黙って置き換えない。Randomや追加Mixerへの対応は一覧と保存互換性を確認してから対応範囲へ加える。

DurationはUI・内部モデルとも秒として扱う。送信時に必要な単位変換や正規化は通信側で行う。0秒、上限、精度を対象版で確認する。マニュアルのスライダー例だけからAPIの有効範囲を決定しない。

### 4.2 Mask Mode制御（第2段階・`Layer Mask Control`。初版の範囲外）

| 要求上の指定 | 意図する動作 |
| --- | --- |
| Disable | Target Layerを通常の映像Layerとして扱う |
| One Below | Target Layerの内容を、その直下のLayerのマスクとして使う |
| All Below | Target Layerの内容を、下位Layer群のマスクとして使う |

制御対象は、マスクされる映像Layerではなく、Mask Modeを持つLayerである。Mask Mode変更だけでマスク用素材の選択・配置・再生は行わない。

#### 目的と制約

- 同じLayerを場面Aでは通常映像、場面BではMaskとして使う場合に、役割切替用の重複構成を減らす。
- 同じLayerに、独立した通常映像と独立したMask動画を同時再生する機能は追加しない。
- One Belowは直下を対象とする。離れた任意のLayerを選ぶ機能にはしない。
- Group境界、入れ子、Layerの移動後の適用範囲は実機で確認する。Groupを不要にできると断定しない。
- Layerの順番を変えると、Maskの作用先も変わり得る。番号の割当と下位関係を再確認する。
- Mask切替の瞬間とClip切替の瞬間の前後関係によって、旧映像が一時的に通常表示される等の可能性を検証する。
- 動画のマスク結果は、黒・白・中間調・Alphaを含む素材で確認する。
- 動画対応のOne Below専用MASKエフェクトは別の代替案として扱う。本プラグインのMask Mode切替と同等とはみなさない。
- Maskの外部制御が実現しなくても、成立済みのTransition機能は独立して評価できる。ただしMask対応済みとは報告しない。

### 4.3 パラメータ案

TransitionとMaskは別プラグインとし、Target Layerを共用しない。FFGLパラメータをResolumeで編集・保存する。

**Layer Control（初版）**

| 表示名 | 型・意味 | 初期状態 |
| --- | --- | --- |
| Enabled | 状態同期の有効／無効 | **有効**（0.4版で変更。初期化・読込・配置だけでは送信せず、Activeな間だけ反映する） |
| Target Layer | 利用者が認識するLayer番号 | 1 |
| Duration | Transitionの秒数 | 1.000秒（範囲0〜10。精度は実測で確定） |
| Transition Blend | 検証済み選択肢（50種）から選ぶTransition方式。Layer Blend Modeではない | Alpha |
| Instance Key | Clip識別用の内部値。編集しない | 自動生成 |

**Layer Mask Control（第2段階）**

| 表示名 | 型・意味 | 初期状態 |
| --- | --- | --- |
| Enabled | 状態同期の有効／無効 | 無効 |
| Target Layer | Mask Modeを変更する側のLayer番号（Layer Controlとは独立） | 1 |
| Mask Mode | Disabled／One Below／All Below。「変更しない」状態は持たず、使わないときはEnabledをOffにする | Disabled |
| Instance Key | 内部値 | 自動生成 |

ResolumeがEffectごとに付けるBlend Mode／Opacityは本プラグインの項目ではない。

プラグイン内部の接続設定はWebSocket用に統一し、OSCアドレスの編集機能は実装しない。ポートはResolume Webserverの設定と合わせる。変更対象のLayerやパラメータIDはCompositionのデータから解決し、実行間で同一IDが維持されると仮定しない。

Target Layerは初版では番号指定とする。Layerの移動・挿入・削除に自動追従するID指定とは別であり、Groupを含む番号とAPI indexの対応、表示順、1始まり／0始まりは実機で確認する。変換は一か所に集約する。

Activeな制御用Effect Clipのパラメータ編集は、次に受信したComposition通知を契機として同期する。**編集イベントを独自のトリガーとして推測しない。** 通知が発生しない変更経路があるかは試験する。

### 4.4 Composition通知を契機とする状態同期

| 状態・操作 | 期待する動作 |
| --- | --- |
| 対象Clipが`Disconnected` | 送信しない |
| 対象Clipが`Previewing`のみ | 送信しない |
| 対象Clipが`Connected` | 指定値とTarget Layer実値の差分を同期する |
| 対象Clipが`Connected & previewing` | 同じく差分を同期する（本番側でConnected） |
| 別Columnへのトリガー／対象Clip直接トリガー | Composition通知後にActiveとなった対象Clipの差分を同期する |
| 同一Columnの再トリガー | 特別な再トリガー検出を要求しない。通知時点で差分があれば同期し、一致なら送らない |
| **Active中の制御用Effect Clipパラメータ編集** | 次のComposition通知で変更後の指定値を同期する |
| 未再生Clipの編集・選択・プレビュー | Target Layerを変更しない |
| Target Layerの設定を手動変更 | Active中の次のComposition通知で実値との差分を照合し、必要なら指定値へ戻す（通知が届くか未検証） |
| 通常ループ、位置更新、フレーム描画 | これら単独では設定を送信しない |
| Clip停止・Eject・別Clipへの切替 | ActiveでなくなったClipは同期しない。同じ項目を管理するActiveなClipが他になければ、次のComposition通知でベースラインへ復元する。他にActiveなClipがあればそのClipの値へ切り替える（復元しない） |
| 再初期化・読込 | それだけで無条件送信しない。ただしActiveなClipについてComposition通知があれば同期対象となる |

Active判定はClipの`connected.value`を候補とする。`selected`やColumnのUI選択状態では判定しない。**`connected`が本番再生を正確に表すこと、FFGLインスタンスとClip IDが対応付くことは未検証**である。

**送信するかは「前回送信した値」ではなくTarget Layerの実際の現在値で決める。** そのため手動変更後に一致していなければ改めて送る。既に一致している項目は、同じColumnを再トリガーしても送らない。これは旧AT-04の「操作ごとに必ず1回送信」からの仕様変更である。

WebSocketの`set`で発生した通知も入力になり得る。再同期の前には取得できる新しい実値を比較し、一致したら送信を止める。更新未反映・応答待ちの間に同じ差分要求を多重送信しないよう、保留中要求を管理する。通知が古い場合の読戻し・確定判定は検証する。

**Composition状態通知と再生位置の`parameter_update`を区別する。** 再生位置通知を契機とした連続`set`や、位置の巻き戻りからの再トリガー推定は行わない。

### 4.5 機能定義JSONと共通基盤

**共通のC++基盤と、JSONで記述する機能定義を分ける。** 通信、Clip識別、Active判定、実値比較、重複送信防止、映像パススルーは共通C++が担当し、プラグインごとの項目と対応はJSON（`definitions/*.json`）で定義する。ビルド時にJSONを検査し、FFGLパラメータ定義と制御設定を生成する（`tools/generate_plugin.py`）。実行中にUI構成が変わらないため、保存済みCompositionとの互換性を管理しやすい。

JSONで管理する情報：

- プラグイン名・固有ID
- UI項目の名前・型・初期値・選択肢・範囲
- Target Layerとの対応（`role: target`）
- Compositionから読む項目と設定する項目（`property`）
- 単位変換・比較精度（`tolerance`）・適用条件

例（Duration。設計例であり実ファイルが正）：

```json
{ "id": 2, "key": "duration", "label": "Duration", "type": "number",
  "default": 1, "min": 0, "max": 10,
  "property": "transition.duration", "tolerance": 0.000001 }
```

- `property`は開発者向けの論理名である。実際のWebSocketパラメータIDは、その都度Compositionから解決する。
- 項目の`id`はFFGLパラメータ番号で、削除した番号は再利用せず予約として残す（生成時に検査する）。公開後に番号・型・選択肢の意味を変えない。
- 既存の型（boolean／integer／number／choice／text）と制御方式に収まる追加は、JSONの編集と再ビルドで行える。新しい動作方式はC++側の追加が必要。
- FFGLの表示名は16文字以内。

## 5. 設計案

### 5.1 責務の分離

| 要素 | 責務 |
| --- | --- |
| FFGL Adapter | パラメータの保持、映像のパススルー、インスタンス寿命の管理 |
| Composition Subscriber | WebSocketでComposition通知を受け取り、対象パス・状態を解析する |
| Clip Resolver / Active Gate | FFGLインスタンスとComposition内Clipの対応を取り、本番Activeか判定する |
| State Reconciler | Active Clipの指定値とTarget Layer実値を比較し、変更が必要な項目だけ要求する |
| Protocol Mapping | Layer番号とWebSocketのパス・ID・値・単位を対応付ける |
| WebSocket Transport | `set`の送信、必要な取得、応答・再接続、未完了要求の制御 |
| Diagnostics | 通知、Active判定、差分、送信、読戻し、失敗を分けて記録する |

従来の`Trigger Detector`は必須構成から除外する。ネットワーク待ちやファイル書込みを描画スレッドで行わない。複数インスタンスからの購読方式と状態共有方法は重複接続を避ける設計にする。

### 5.2 処理手順

1. 起動／復元時に設定値を読み込む。初期化それ自体を送信契機にしない。
2. WebSocketでComposition状態通知を購読する。通常の再生位置`parameter_update`とは区別する。
3. 通知から対象Clipを同定し、`connected.value`などで本番Active状態を判定する。対応が不確実なら送信せずログに記録する。
4. Activeかつ`Enabled`の場合、Target Layer、Apply指定、FFGL設定値を取得する。
5. Target Layerの**実際の現在値**を、取得した最新Composition状態または有効なWebSocket照会で確認する。古いキャッシュ／最後の送信値で代用しない。
6. 値とLayer対応を検査し、指定値と一致しない項目だけ`set`要求を生成する。全部一致なら何もしない。
7. WebSocketで差分を送信し、応答と読戻しを区別して記録する。未完了の同じ差分要求を重ねない。
8. 次のComposition通知でも同じ照合を行う。管理するActiveなClipがなくなった項目は、取得した実値がベースラインと異なる場合だけ`set`で復元する（Clip評価に失敗した通知では復元しない）。

対象Layerが存在しない、Clip対応付けが曖昧、状態が不明、値が不正な場合は別Layerへ代用せず送信しない。

### 5.3 Composition通知とActive判定の成立判定

再トリガー／通常ループ／編集を直接分類することは**今回の設計では要件にしない**。確認すべきなのは、通知による状態同期が操作上の要求を満たすかである。

- Composition通知が同一Column再トリガー、別Columnへの切替、Active中の編集で届くこと。
- 当該Clipの`connected`をComposition状態から取得でき、`Previewing`と本番`Connected`を区別できること。
- 同一のFFGLエフェクトを複製・複数配置した場合も、各インスタンスを正しいClip ID／pathへ対応付けられること。
- Target Layerの最新値が取得でき、外部から手動で値を変更しても差分照合が追従すること。
- WebSocketの`set`後に通知が再び届いても無限送信しないこと。
- Active中の編集で同期が実行され、未再生Clipの編集・プレビューでは実行されないこと。
- 旧Clipと新ClipがTransition中に両方`Connected`になる場合、後着要求で以前の値を上書きしないこと。
- Auto Pilot、Fader Start、Opacity 0、Bypass、Solo、初回ロード・再接続で想定外の送信を起こさないこと。

**実機のComposition JSONにある`connected`の候補状態は観測済みだが、インスタンスへの対応付けや全操作での状態推移は未確認。** 対応付けが確立できない段階では製品動作としてActive判定の成立を宣言しない。

過去の`SetTime()`、`Connect()`や再生位置巻き戻りを使う再トリガー検出は研究記録として残し、今回の主要実装には組み込まない。

### 5.4 同一Columnでの反映順序

望ましい順序は「設定有効化 → 対象映像のTransition開始」である。一方、実装候補は「Column起動 → Composition通知 → Active判定／実値比較 → WebSocket `set` → Resolume反映」となり、同じColumnで開始するTransitionに間に合わない可能性がある。これは未検証の懸念であり、今回の状態同期方式でも解消したとみなさない。

最初のTransition試験で、新旧の時間と方式を明確に変えて比較する。値のUI表示・WebSocket応答だけで合格にしない。WebSocketで複数項目を順次`set`しても、その時点のTransitionへの反映や原子的更新は保証されない。

間に合わない場合は、事前設定用のColumn、設定後に映像を起動する制御、別の連携方式を代替案として整理する。いずれも操作または構成の変更を伴うため、本来の完成条件とは分けて記載する。固定時間待ちの追加だけで成立済みとしない。

### 5.5 映像出力と配置

本プラグインは映像加工を目的としない。Effect Clipは下位Layerの映像へ作用するため、FFGL Effectとして受け取ったテクスチャを色・Alpha・向き・解像度を維持して返す方式を第一候補とする。

透明画素を出すだけで映像への影響をなくせるとは仮定しない。Effect Clipでは映像を置き換えてしまう可能性も評価する。OpenGL状態を適切に復元し、入力なしの扱いはSDK契約に従って検証する。

配置Layerでは元から再生中のClipを置き換える可能性がある。初期試験は専用の配置Layerを用意する。既存映像Layerへ置いても停止しないとは記載しない。配置LayerのOpacity 0やBypassによってFFGL処理が省略される可能性があるため、非表示化をトリガー成立の前提にしない。

### 5.6 通信・失敗時の扱い

- PluginとResolume間はWebSocketを用い、絶対値の`set`を行う。OSC・UDP送信経路は持たない。既存の外部OSC運用には介入しない。
- 列挙値、パス、パラメータID、Durationの単位・精度は対象版の実測結果から解決する。IDやLayer indexを固定しない。
- `set`の送信・サーバ応答・値の読戻し・今回の映像への反映を別の成功段階として記録する。
- WebSocketの`set`が通知を誘発しても、最新実値との一致を確認して再送を止める。更新処理中は同一差分の重複送信を抑える。
- 旧Clipの送信要求が新Clipの状態を上書きしないよう、古い保留要求と世代／Active状態を送信直前に再評価する。既送信分の取消しは保証しない。
- 複数項目の一部だけが反映される可能性を扱い、原子的更新・自動ロールバックを保証しない。
- 自動再送は初版の既定機能としない。再送が必要なら回数と期限、再照合、古い要求の破棄を定める。
- 同じTarget Layerの同項目を複数のActiveな制御Clipから同時に変更する構成は初版の保証対象外とする。
- 切断、不正値、対応付け不能、タイムアウトは記録し、Resolumeの描画を止めない。

## 6. 保存と互換性

FFGLパラメータをResolumeに保持させる方式を第一候補とする。Compositionの保存・再読込、Effect Clipの複製、Presetへの保存・再適用について個別に検証する。

公開後はパラメータの番号・型・既存列挙値の意味を不用意に変えない。Blend Mode一覧がResolumeの版や導入Mixerで変わる場合は、対応表と保存互換性の方針を定める。未知の値は送信不可として明示する。

実行中フラグ、接続状態、実行世代、送信済み判定は保存値に含めない。

## 7. 開発段階と判定

| 段階 | 作業 | 次へ進む条件 |
| --- | --- | --- |
| 0：環境確定 | OS、Arena版、FFGL SDK、Webserver設定、テストCompositionを記録 | 試験を再現できる |
| 1：WebSocket変更確認 | Transition方式・時間・Maskの`set`と読戻し、反映順序を確認 | Node-REDでの変更成功報告はあり、最新値の取得・今回のTransitionへの反映は別に判定 |
| 2：通知・Clip同定 | Composition購読、`connected`の状態遷移、FFGLインスタンスとClip IDの対応を観測 | UNK-04・09・12の成立可否が明確 |
| 3：状態同期実装 | FFGLパラメータ、Active Gate、差分比較、WebSocket `set`、重複抑止を統合 | AT-01〜AT-06、AT-12の関連項目を満たす |
| 4（第2段階・初版外）：Mask・映像影響 | `Layer Mask Control`でMask各状態、動画、Group、パススルー、切替タイミングを試験 | 試験で保証できる構成を特定 |
| 5：運用整理 | 接続・エラー・負荷・ログ・制限を記録 | 未確認項目が明示される |

次の優先作業は**Composition通知からの対象ClipのActive判定と、Target Layerの最新実値の取得・差分照合**である。再トリガー専用イベント探しは進行条件にしない。

## 8. 受入試験

以下は実装の合格条件であり、記載だけで試験実施済みとはしない。**初版（Layer Control）の対象はAT-09〜AT-11（Mask）を除く項目。** AT-09〜AT-11は第2段階の`Layer Mask Control`の試験とする。Node-RED単体のWebSocket設定成功とプラグインの受入試験を区別する。

| ID | 試験 | 合格条件 |
| --- | --- | --- |
| AT-01 | 別LayerのTransition変更 | 指定Target Layerの方式・時間だけ同期し、他LayerとLayer Blend Modeを変えない |
| AT-02 | 同じColumnで映像と設定を切替 | **今回開始するTransition**に新方式・時間が使用される |
| AT-03 | Active中の繰返しComposition通知／フレーム描画 | Target Layerが指定値と一致する間は追加`set`を送らない。描画・位置更新では送らない |
| AT-04 | 同一Column再トリガー・手動変更後の照合 | 再トリガー検出を要求しない。Target Layerが手動変更で不一致なら**次のComposition通知時に**指定値へ戻り、一致なら送らない |
| AT-05 | 未再生Clipの編集、選択、プレビュー、再初期化 | ClipがActiveでなければTarget Layerを変更しない。Active中のパラメータ編集は通知後に同期する |
| AT-06 | 保存・再起動・読込・複製・Preset | 保存値が復元され、インスタンスとClipの対応が混線しない |
| AT-07 | 映像パススルー | 色・Alpha・向き・解像度、既存映像への影響が許容範囲 |
| AT-08 | Clip停止・Eject・別Clip切替・空セルColumn | 停止後に適用前の値へ復元される。別のLayer Control Clipへ切り替える場合は復元を挟まず新Clipの値になる。空セル挙動を記録 |
| AT-09 | Mask 3状態の同期 | Disabled・One Below・All Belowで指定通りのマスク適用 |
| AT-10 | 動画Maskと同Column映像 | 動画が更新され、意図しない一瞬の露出がない |
| AT-11 | Group境界・Layer移動・追加・削除 | マスク作用先とTarget Layer番号の扱いが説明可能 |
| AT-12 | 不正値・切断・対象Clip不明・競合 | 別Layerへ誤送信せず、失敗と未確認を記録し、描画を継続 |
| AT-13 | 高速Column切替・旧新Clipの同時Active | 古い保留要求で新設定を上書きせず、限界を実測 |
| AT-14 | Previewing／Connected & previewing／Bypass／Opacity 0／Auto Pilot等 | Activeとプレビューを判定し、成立・非対応の条件を記録 |
| AT-15 | 繰返し・長時間・終了 | 通知ループ・保留要求の蓄積・資源リークがなく終了できる |
| AT-16 | 自己`set`が誘発する通知 | 再照合で収束し、同一差分を無限再送しない |
| AT-17 | Active中の制御用Effect Clip編集 | 通知を契機として変更した指定値がTarget Layerへ同期される |

比較に用いたTarget Layer実値、Active判定の根拠、通知の`type`・`path`、要求値、送信前後の値、反映映像をログへ残す。Duration精度や許容遅延等の数値条件は運用環境で確定する。

## 9. 検証記録の書式

各試験には、試験ID、日時、OS、Resolume製品・版、SDK／プラグインのcommit、配置と対象Layer、操作手順、期待結果、実結果、送信先、型・値、ログ・映像の場所、判定、制限を記録する。

WebSocket対応表には、対象版、Composition通知形式、Clip状態・ID／path、`set`先のパス／パラメータID、型、単位、列挙値、最新値の読戻し方法、実機確認日を残す。未確認の欄は「未確認」とし、推測値で埋めない。

## 10. 初版の範囲外

Mask Mode制御（別プラグイン`Layer Mask Control`として第2段階）、Resolume本体改造、本プラグインの制御項目以外へ任意のWebSocketコマンドを追加するUI、ネットワーク越しの複数PC制御、1インスタンスでの複数Target Layer指定、独立した通常映像とMask動画の同一Layer同時再生、専用動画MASKエフェクト、未検証バージョンへの互換性保証は初版に含めない。

## 11. 参照資料

公式資料の確認日は2026-10-22。利用者のWebSocket実測は2026-10-22〜23。WebSocket通知内容と個々の実装成立条件は実機で確定する。

- [S1：公式 Effects／Effect Clips](https://resolume.com/support/en/effects)
- [S2：公式 Layers／Layer Transition・Mask Layer](https://resolume.com/support/en/layers)
- [S3：公式 OSC](https://resolume.com/support/en/osc)
- [S4：公式 REST API & Webserver](https://resolume.com/support/en/restapi)
- [S5：公式 Arena／Avenue REST API Reference](https://resolume.com/docs/restapi/)
- [S6：公式 FFGL SDKリポジトリ](https://github.com/resolume/ffgl)
- [S7：公式 FFGLPluginSDK.h](https://github.com/resolume/ffgl/blob/master/source/lib/ffgl/FFGLPluginSDK.h)

