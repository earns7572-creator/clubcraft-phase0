# Club Craft — AI開発引き継ぎ資料（製品仕様の正本）

**Repository:** `https://github.com/earns7572-creator/clubcraft-phase0`

**Branch:** `main`

**現行実装:** 0.5.0系プロトタイプ
**重要:** この文書は、以前の「4 Speaker固定・Stereo空間シミュレーター中心」のロードマップを置き換える。今後の開発はこの文書を正本とする。

> **最重要の製品概念:** Club Craftは、仮想クラブのサウンドシステムそのものを音楽制作・ミキシング・マスタリングのインターフェースにするDAWプラグインである。
>
> 通常のEQで周波数を直接ブースト／カットするのではなく、Speakerの種類・数・配置・出力バランスと、SOURCEから各Speakerへのルーティングを操作した結果として、最終的な音色・空間・マスターが決まる。

---

## 1. 製品の中心思想

Club Craftは「PA設計ソフト」でも「4 Speaker固定のStereo空間シミュレーター」でもない。

中心は次の4点。

1. **仮想クラブを作る**
   - Speakerを自由に追加・削除・配置する
   - `SUB / WOOFER / FULL RANGE / MID / HIGH / CUSTOM` を選ぶ
   - 同じ種類を何台置いてもよい
   - 例: SUBを5台置いて極端な低域を作ることも許可する

2. **DAWの各SOURCEを、好きなSpeakerへ直接つなぐ**
   - 1 SOURCE → 複数Speaker
   - 複数SOURCE → 同じSpeaker
   - 接続数は固定しない
   - UIではSOURCEをSpeakerへドラッグして接続する

3. **FULL SIGNAL / BAND ROUTING**
   - 例: `Kick Full Signal → FULL L/R`
   - 同時に `Kick 20–100 Hz → SUB 1/2`
   - 周波数分割は「EQの代わり」ではなく、Speakerへの送り方として扱う

4. **Speaker Balanceによる“逆EQ”**
   - EQカーブを直接編集しない
   - Speakerの種類・台数・Levelの結果として最終周波数バランスが変わる
   - RTA / Spectrumは編集画面ではなく「結果を見る画面」

---

## 2. 現行0.5.0の扱い

現行コードには以下がある。

- C++20 / JUCE 9.0.1
- 単一 `Club Craft.vst3`
- 各インスタンスでSOURCE / CLUB role切替
- `SessionRegistry`
- APVTS state
- realtime-safeなprimitive atomics + seqlock
- Generic Speaker Type
- Speaker Level
- Speaker位置 / Listener位置
- StereoSpatialRenderer

### 残すもの

以下は基盤として維持する。

- single-binary SOURCE / CLUB architecture
- SessionRegistryの考え方
- realtime threadでlock / allocation / I/O / waitをしない方針
- APVTSによる状態保存
- macOS Universal build
- Speaker Type enumと基本DSP部品
- テスト基盤

### 「完成形の制約」としては撤回するもの

現行の以下は**プロトタイプ上の都合であり、製品要件ではない**。

- Speakerが4台固定
- `FRONT L / FRONT R / REAR L / REAR R` 固定
- SOURCEが必ず4 Speaker全部を通る
- `StereoSpatialRenderer` が最終アーキテクチャ
- Stereo L/Rのみが主出力
- SOURCE位置 `(0,0)` を中心に全Speakerへ音を放射するモデル
- Spread / Depthで4台をまとめて動かすモデル
- 4 Speaker固定を守ること
- 8ch / Binauralを1.0以降へ後回しにすること

これらを今後の設計判断の前提にしない。

---

## 3. 正しいSOURCE / CLUBモデル

### SOURCE

各DAW Trackに挿す軽量インスタンス。

持つべき情報:

- Source ID
- Display Name
- Icon
- Club Viewでの表示/非表示
- Routing definitions
- 必要ならSource Group

SOURCE自体の「クラブ内の物理位置」を主操作にはしない。

重要なのは**SOURCEがどのSpeakerへ送られているか**。

### CLUB

1つのメイン画面で以下を扱う。

- 全SOURCE一覧
- Speaker追加 / 削除
- Speaker Type
- Speaker位置
- Speaker Level
- SOURCE → Speaker routing
- FULL SIGNAL / BAND
- Listener位置
- RTA
- Output mode
- Binaural preview

ユーザー体験としては「複数のプラグインを触る」のではなく、CLUB画面から1つのVirtual Clubを操作する。

---

## 4. Speaker Sceneモデル

V1設計ではSpeakerを固定4台のarrayとしてハードコードしない。

### 必要なSpeaker属性

- Stable Speaker ID
- Name
- Type
  - SUB
  - WOOFER
  - FULL RANGE
  - MID
  - HIGH
  - CUSTOM
- Position
  - X
  - Y
  - 将来Z
- Output Level
- Output channel assignment
- Enabled / Mute / Solo
- Frequency response profile

### 台数

完成形では可変台数。

初期実装ではrealtime安全性のため、例えば `MAX_SPEAKERS = 16` の固定容量slot方式でもよい。
UI上では追加/削除可能に見せ、audio threadでは事前確保されたslotだけを使う。

**4台固定構造をそのまま拡張し続けないこと。**

---

## 5. Routingモデル

これはClub Craftの最重要部分。

### 例

```text
KICK
├ Full Signal → FULL L
├ Full Signal → FULL R
├ 20–100 Hz → SUB 1
└ 20–100 Hz → SUB 2

HAT
├ Full Signal → HIGH L
└ Full Signal → HIGH R
```

### Routeに必要な情報

- Route ID
- Source ID
- Speaker ID
- Mode
  - FULL
  - BAND
- Band Low Hz
- Band High Hz
- Route enabled
- 必要ならRoute gain（V1では非表示でもよい）

### UI

基本操作:

1. SOURCEを掴む
2. Speakerへドラッグ
3. 接続
4. 接続をクリックすると `FULL / BAND` を選べる

大量の曲線ケーブルを常時表示しない。

Routing lineは:

- straight
- orthogonal
- thin
- selected / hover時のみ強調

を基本とする。

---

## 6. Speaker Frequency Response

Speaker Typeは単純な理想HPF/LPFだけではなく、自然なCharacter Curveを持つ方向へ進める。

V1ではGeneric Profileでよい。

例:

- SUB
- WOOFER
- FULL RANGE
- MID
- HIGH

将来:

- NEXO
- Funktion-One
- JBL
- Martin Audio

などの実機Profileを追加できる構造にする。

実機Profileはメーカー公称値や測定データのライセンス・商標を確認してから実装する。

`Generic Response Tone` のような、通常EQに近いグローバルTone knobは製品の中心にしない。必要性がなければ削除候補。

---

## 7. System Balance = Reverse EQ

Club Craftの特徴。

ユーザーは通常のEQ pointを動かさない。

例:

- SUB Speaker群を上げる
- FULL RANGEを下げる
- HIGHを少し下げる

その結果としてMasterの周波数バランスが変わる。

### RTA

RTA / Spectrum Analyzerをリアルタイム表示する。

目的:

- 今のSpeaker構成がどんな音になっているか見る
- SUBを増やしすぎた時に低域過多を確認する
- クリエイティブな極端設定を禁止しない

RTAは**編集用EQではない**。

自動補正もしない。

必要なら軽いWarningだけ出す。

---

## 8. Listener

ListenerはVirtual Club内で移動できる。

Listenerを動かした時に、ヘッドホン / Stereo preview上で聴こえ方が変わる。

V1で優先する要素:

- Speaker position
- Listener position
- Direction
- Distance
- Speaker response
- Binaural rendering

高度な壁反射・残響・物理音響は後回し。

ユーザーがPA delayを手動で触る機能は中心にしない。

---

## 9. Output

1つのClub Sceneから最終的に以下を扱う。

- BINAURAL 2ch
- STEREO 2ch
- QUAD 4ch
- OCTA 8ch

**4ch / 8chは完成版の中心機能であり、1.0以降の任意拡張ではない。**

DAW/formatごとに入出力制約があるため、実装前にJUCE / VST3 / AU / Ableton Liveの対応を検証する。

必要なら「Preview engine」と「Discrete multichannel output」を分離して設計する。

---

## 10. Binaural

通常の2ch headphoneでVirtual Clubを確認できることは主要機能。

目標:

- Left / Right
- Front / Rear
- Height（将来Z導入後）
- Distance

を知覚可能にする。

HRTFの方式は実装前に比較・検証する。

現時点の簡易StereoSpatialRendererを「Binaural完成」とみなさない。

---

## 11. UI / UX

添付した最終UIモックアップの方向を基準にする。

### Design direction

- light / medium gray
- airy
- minimal
- tactile
- sophisticated
- 箱庭 / 建築模型 / 3Dプリンタ模型の感覚
- Speakerは無彩色の立体フィギュア感
- Accent colorはSOURCE識別など必要最小限
- Apple / macOSの余白感
- Abletonの機能的整理
- Teenage Engineeringのプロダクト的遊び心
- Blenderの模型/viewport感

ただし特定製品のUIをコピーしない。

### 避ける

- Neon
- Cyberpunk
- AI dashboard感
- SaaS dashboard感
- ゲーミングUI
- excessive gradients
- glowing particles
- 派手すぎるSpectrum
- toy-like多色UI
- 大量の数値
- 大量の枠線
- うねうねしたRouting cable

### Tabs

すべてを1画面へ詰め込まない。

候補:

- CLUB
- SOURCES
- SPEAKERS
- MIX / SYSTEM
- OUTPUT

### CLUB View

中心画面。

- 箱庭的Virtual Club
- Speaker
- Listener
- Visible Sources
- Speaker activity
- Speaker Level
- Small RTA

---

## 12. Source Management

100 Track規模でも破綻しない。

登録SOURCE全部をClub Viewへ表示しない。

各Sourceに:

- Show in Club
- Hide from Club

を持つ。

非表示でもRouting / audio processingは維持。

将来:

- DRUMS
- BASS
- SYNTH
- VOCAL
- FX

などGroup化。

SOURCEごとにName / Iconを選択できる。

---

## 13. Sound Visualization

Speakerが鳴っていることをClub Viewで分かるようにする。

避ける:

- neon glow
- particle cloud
- sci-fi wave

候補:

- 小さなspeaker cone movement
- subtle flat-color wave
- simple ring
- restrained surface deformation
- speaker activity meter

色は「音の情報」と「SOURCE識別」に限定して使う。

---

## 14. Realtime Safety

既存方針を維持。

`processBlock()` では:

- mutex禁止
- allocation禁止
- container resize / push_back禁止
- I/O禁止
- sleep / wait禁止
- logging禁止

可変Speaker / Routeを実現する場合は、control threadでSceneを構築し、audio threadへは固定容量のimmutable / atomic-friendly snapshotを渡す。

例:

- `MAX_SPEAKERS = 16`
- `MAX_ROUTES = 128`

など、V1では上限を設けてもよい。

---

## 15. 開発の次の順序

現行0.5.0のPhase 4ロードマップは廃止する。

### Re-alignment Phase — 0.6.0

**コードを大きく追加する前にアーキテクチャを修正する。**

目的:

- 固定4Speaker構造を完成形の前提から外す
- Scene modelを可変Speaker / Route前提へ設計
- SOURCE→Speaker routingを中心に置く
- Stereo previewとDiscrete outputを分離して考える

まず設計資料を作り、コード変更前にレビューする。**実装開始前に [ARCHITECTURE_GATE_0_6.md](ARCHITECTURE_GATE_0_6.md) のGate A〜Hを設計として明示的に承認すること。** Gateは設計上GOとなったが、0.6.0ではOutput backendを固定しない。CLUBをControl Plane、SOURCEをAudio Workerとし、Route Contribution → Virtual Speaker Voice → Preview / Discrete Outputの3層を実装境界とする。Virtual Speaker Voiceは線形DSPだけに限定する。

### 0.6.x — Host I/O Decision

0.6.0で実施する`OUTPUT_FEASIBILITY_SPIKE.md`の結果をArchitecture Decision Recordとして確定する。VST3 / AUとAbleton Live / Logic Pro / ReaperにおけるStereo、Quad、8ch、Preview + Aux出力のbus negotiationを確認し、その結果が出るまでSOURCE audioの最終multi-channel aggregation方式を決め打ちしない。

### 0.7.0 — Dynamic Speaker, Full Routing & Listener UI

0.7.0では、0.6.0のDynamic Sceneをユーザー操作へ公開する。唯一の実装判断正本は [ARCHITECTURE_GATE_0_7.md](ARCHITECTURE_GATE_0_7.md) であり、Gate A〜Oの最終GOなしにコードを書かない。

- 最大16 Speakerのadd/remove、Type、Level、X/Y、mute
- Floor View上のSpeaker dragとListener drag
- SOURCE一覧、offline source表示、virtualized Routing Matrix
- SOURCE→Speakerのone-to-many Full + SUM_MONO Route create/delete/mute/gain
- legacy implicit 0.25 Routeの確認付きmaterialisation
- DynamicScene authoritative、legacy APVTS pending mailbox、Stable ID bridge
- revision付きcandidate transaction、30Hz drag publish、mouseUp即時publish
- 2048 global Route、persistent route slot + generation、gain / mute smoothing、Route delete後のfade-out
- pending APVTS automationをoverlayして矛盾しないschema 7 Stateを保存
- duplicate CLUB read-only、duplicate SOURCE rekey

### 0.8.0 — Band Routing + System Balance

- FULL / BAND route
- band low/high
- Speaker response
- RTA
- group + individual speaker level
- Speaker activity visualization

### 0.9.0 — Binaural Preview

- stereo previewの改善
- real binaural/HRTF research and implementation
- front/rear/distance
- Z/height modelを入れるならここで設計

### 0.11.0 — 4ch / 8ch Output

- QUAD / OCTA bus architecture
- output channel assignment
- DAW compatibility tests
- export/render workflow

### 0.12.0 — Workflow / Polish

- tabs
- presets
- source groups
- solo/mute
- A/B
- clip warning
- state migration
- UI refinement

### 1.0.0 — Production Hardening

- macOS Intel / Apple Silicon
- VST3 / AU
- Ableton / Logic / Reaper
- sample rates
- block sizes
- offline render
- automation
- save/restore
- CPU
- release build
- docs
- signing / notarization where required

---

## 16. 次にAIが最初にすること

**まだ実装を始めない。**

最初に:

1. 現行コードを読み、0.5.0から残せる基盤を列挙
2. 固定4Speaker / StereoSpatialRenderer依存箇所を列挙
3. Dynamic Speaker + Route Sceneのデータ構造を提案
4. realtime-safe snapshot方式を提案
5. SOURCE→Speaker audio routingをどう実装するか提案
6. Stereo/Binaural previewと4/8ch discrete outputの責務分離を提案
7. APVTS / state migration方針を提案
8. 0.7.0上限として `MAX_SPEAKERS=16`、`MAX_SOURCES=128`、`MAX_ROUTES_GLOBAL=2048`、`MAX_ROUTES_PER_SOURCE=16` を前提に、overflowをcontrol側でrejectする設計を確認
9. 0.7.0では`parameterChanged()`がatomic pending mailboxだけを更新し、message threadがStable ID基準でDynamicSceneへ適用する設計を確認
10. revision付きcandidate transaction、route slot + generation、gain / mute smoothing、legacy materialisation rollbackを確認
11. 変更対象ファイル一覧を提示
12. [ARCHITECTURE_GATE_0_7.md](ARCHITECTURE_GATE_0_7.md) のGate A〜Oを満たす設計を提示する
13. その設計を承認されるまでコードを書かない

---

## 17. 絶対に避けること

- 4 Speaker固定を製品要件として維持する
- SOURCEを必ず全Speakerへ送る
- StereoSpatialRendererだけを完成形として発展させる
- 4ch / 8chを1.0以降へ追いやる
- 普通のEQ UIを中心にする
- Generic Tone knobを中心にする
- PA delay調整ソフトへ寄せる
- 現実のクラブを精密再現すること自体を目的にする
- SOURCE / CLUBを別binaryに分ける
- realtime thread安全性を破る
- UIをネオン / SF / AI dashboard化する

---

## 18. 一文で表す完成形

> **仮想クラブに好きなSpeakerを置き、DAWの各音をSpeakerへ直接つなぎ、Speakerの種類・数・配置・出力バランスとListener位置を操作することでミックス／マスターを作る。結果をBinaural / Stereoで試聴し、そのまま4ch / 8ch作品として出力できる。**
