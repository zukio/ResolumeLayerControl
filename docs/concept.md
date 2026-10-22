# Resolume Layer設定制御プラグイン 開発構想

## 1. 依頼者の希望

ResolumeのColumnをトリガーした際に、映像ClipやEffect Clipと同様の操作で、指定したLayerの設定を変更したい。

**目的**

- Columnごとに異なるLayer設定を適用したい。
- ResolumeのUI上で設定内容を編集・保存したい。
- 本番中に手動でLayer設定を変更する操作を減らしたい。
- MASK用の専用LayerやGroupを減らし、構成変更を容易にしたい。

## 2. 実装項目

### ① Layer Transition設定の変更

以下の3項目を指定できる機能。

| 項目         | 内容                   |
| ------------ | ---------------------- |
| Target Layer | 変更対象のLayer番号    |
| Blend Mode   | TransitionのBlend Mode |
| Duration     | Transition Time        |

Columnのトリガーに連動して、指定LayerのTransition設定を変更する。

### ② Layer Mask Modeの切り替え

指定LayerのMask Modeを以下の3種類から選択して切り替える。

- `Disable`
- `One Below`
- `All Below`

現在はLayerの右クリックメニューから手動で変更できるが、Columnのトリガーに連動させたい。

これにより、同じLayerを通常映像用とMASK用で兼用し、専用Layerの削減を目指す。

※動画対応のOne Below専用MASKエフェクトは、代替案として別途検討する。

## 3. 制御用Clipの考え方

「制御用Clip」は今回の説明のための仮称であり、Resolumeの正式な機能名ではない。

Resolumeには既存機能として `Effect Clip` がある。

**Effect Clipとは**

エフェクトを映像Clipと同じようにセルに配置し、Columnからトリガーできる機能。

**今回の実装案**

この仕組みを利用し、映像加工ではなくLayer設定の変更を実行する独自FFGLプラグインを開発する。

例えば、Column 3に配置した制御用Effect Clipに以下を設定する。

- Target Layer：5
- Blend Mode：Alpha
- Duration：2秒

Column 3をトリガーすると、Layer 5のTransition設定が変更される。

各Columnに異なる設定のEffect Clipを配置すれば、Column単位で設定内容を管理できる。

## 4. 実装方法（案）

既存の `ffgl-effect` テンプレートを利用したC++／FFGLプラグインを想定する。

**処理の流れ**

1. Resolumeのセルに独自Effect Clipを配置する。
2. FFGLのパラメータUIで対象Layerと変更内容を設定する。
3. ColumnのトリガーによってEffect Clipを起動する。
4. プラグインからローカルのResolumeへOSCコマンドを送信する。
5. Resolumeが対象Layerの設定を変更する。

OSCはプラグイン内部の制御手段として利用し、通常操作では利用者がOSCアドレスやコマンドを直接管理する必要のない設計を目指す。上級者向けには、本プラグインが扱う制御項目のOSCアドレスや送信値の対応を変更できる詳細設定を設ける。具体的な範囲と検証方針は仕様書4.3節に記載する。

なお、これは実装候補であり、動作確認済みの構成ではない。

## 5. 要検証項目

### FFGL関連

- FFGLプラグインからOSC（UDP）を送信できること。
- Effect Clipがトリガーされたことを検出できること。
- トリガー1回につき、設定変更を確実に1回だけ実行できること。
- パラメータUIでLayer番号、Blend Mode、Duration、Mask Modeを設定・保存できること。

### Resolume関連

- OSCでLayer TransitionのBlend Modeを変更できるか。
- OSCでLayer TransitionのDurationを変更できるか。
- OSCでMask Modeの3種類を切り替えられるか。
- OSCで対応できない場合、REST APIなどで変更可能か。
- 制御用Effect Clipの起動が意図しない映像表示や既存Clipの停止を引き起こさないか。

## 6. 開発の進め方

**第1段階：Transition制御の検証**

まず、OSCによってLayer TransitionのBlend ModeとDurationを変更できるか確認する。

**第2段階：FFGLプラグインの試作**

`ffgl-effect` をベースに3項目の設定UIとOSC送信を実装し、Columnトリガーとの連動を確認する。

**第3段階：Mask Mode制御**

Mask Modeの外部制御可否を調査し、実現可能なら同様の方式で実装する。

---

**開発方針：** Resolume本体を改造するのではなく、既存のEffect Clipの操作体系を活用し、Column単位でLayer設定を変更できる仕組みを目指す。

前提と未確定事項を分けて示すMask制御の目的と制約を整理する
