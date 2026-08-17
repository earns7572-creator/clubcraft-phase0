# Club Craft 0.6.0 — Architecture Gate

## Status

**0.6.0のコード実装は、この文書のGateを承認するまで開始しない。**

ChatGPTによる設計レビューの結論は「条件付きGO」である。Dynamic SpeakerのUI、Band DSP、HRTF、4ch / 8ch本実装を先に始めるのではなく、まずAudio ownership、Route ownership、Realtime plan、legacy migrationを確定する。

> Club Craftの完成形は、Speakerの種類・数・位置・LevelとSOURCE→Speaker routingを操作するVirtual Clubである。現在の0.5.0は4 Speaker固定のStereo prototypeに過ぎない。

## Gate A — Routing Stateの所有者

**提案:** Routingのauthoritative stateはCLUBが所有する。

CLUBはSpeaker、Route、Listener、output modeを保存する。SOURCEは、SessionRegistryを通じて「自分のSource IDに属するcompiled RoutePlan」だけを受け取る。SOURCEが自分のAPVTSへRouting stateを保存する方式は、Club全体の接続を一つの画面で管理する製品体験と矛盾し、source間の一貫性も失いやすい。

| 層 | 所有するもの | Realtimeで扱うもの |
|---|---|---|
| CLUB Control Scene | Speaker IDs、RouteConfig、Listener、template、UI操作 | 扱わない |
| SessionRegistry | control sceneからcompileしたprimitive snapshot | 固定容量のSpeaker snapshotとsource別RoutePlan |
| SOURCE | 入力audio、filter / delay等のDSP state | 自分のRoutePlanのみ |
| Preview / Output | per-Speaker Feedの出力・preview | stringやUUIDを扱わない |

## Gate B — Speaker FeedとListener Previewの分離

**提案:** ListenerはSpeaker Feedを変更しない。

```text
SOURCE input
    ↓
Route Engine (FULL / BAND)
    ↓
Per-Speaker Feed
    ├─ Discrete Output Mapper → QUAD / OCTA
    └─ Preview Renderer → Stereo / Binaural
                              ↑
                    Speaker layout + Listener
```

Speaker Feedは「どのSpeakerへどんな電気信号を送るか」であり、RouteとSpeaker Typeだけで決まる。Listener、Speaker位置、距離、HRTFはpreview側だけに作用する。これによりBinauralの改善が4ch / 8ch作品のルーティングを変えない。

### 現行Plugin形式での注意

各SOURCEとCLUBは別インスタンスである。audio bufferをRegistryへ入れて集中mixすることは、host scheduling・latency・Realtime安全性の面から採用しない。SOURCEは自分のRoutePlanをローカルに処理し、previewまたはhostが許すmulti-channel busへ出力する。CLUBはauthoritative SceneとMaster / system管理を行い、SOURCE audioをRegistry経由で集約しない。

## Gate C — Stereo SOURCEのRoute入力規則

1台の物理Speaker Feedはmonoとして扱う。RouteConfigには将来の拡張としてinput channel modeを持たせる。

| Mode | V1での扱い |
|---|---|
| `SUM_MONO` | 標準。`0.5 * (L + R)`。旧prototype互換の基準。 |
| `LEFT` | 将来予約。Stereo SOURCEの左成分だけをSpeakerへ送る。 |
| `RIGHT` | 将来予約。Stereo SOURCEの右成分だけをSpeakerへ送る。 |
| `STEREO_PAIR` | 1 Routeではなく、L/R Speakerへ2 Routeを作る概念。 |

0.6.0では、Route Engineのデータ構造にchannel modeを入れてもよいが、UIとDSPで必須にするのは `SUM_MONO` のFull Routeだけに限定する。これでBand routingを後から追加しても、入力規則を作り直さずに済む。

## Gate D — 固定容量Realtime Scene

可変台数の製品体験とaudio threadの安全性を両立するため、control SceneとRealtime Planは別構造にする。

| 項目 | control側 | realtime側 |
|---|---|---|
| Speaker | 可変リスト、Stable ID、name、type、position、level | `MAX_SPEAKERS`固定slot、active flag、numeric coefficients |
| Route | 可変リスト、Source ID、Speaker ID、mode、Hz、enabled | source別の固定RoutePlan、speaker slot、generation、flags、gain |
| ID | UUID / stringを保存可能 | numeric slot / generationのみ |
| 更新 | UI / timerでcompile | seqlock / primitive atomicsでread |

### 0.6.0の推奨初期上限

| 定数 | 推奨値 | 理由 |
|---|---:|---|
| `MAX_SPEAKERS` | 16 | SUB群、Full、Mid、High、8 Speaker outputに対応できる。 |
| `MAX_SOURCES` | 128 | 100 Track規模に余裕を持たせる。 |
| `MAX_ROUTES_GLOBAL` | 512 | 100 SOURCE × 平均5 Route程度に対応する。 |
| `MAX_ROUTES_PER_SOURCE` | 16 | まずは全SpeakerへFull Routeできる。Band導入前の0.6.0には十分。 |

`MAX_ROUTES_PER_SOURCE` は0.9.0のBand routing導入時に32以上へ増やす必要がある可能性がある。固定容量は「無制限」を偽装するためではなく、明示的でテスト可能なV1上限である。容量超過時は黙ってrouteを落とさず、control UIで明確に拒否・警告する。

## Gate E — Stable ID、Slot Reuse、Duplicate Source ID

### Speaker

SpeakerはStable Speaker IDを持つ。Realtime Planはspeaker slotに加えてgenerationまたはStable ID由来のnumeric generationを保存する。Speakerを削除後にslotを再利用しても、古いRouteが新しいSpeakerへ誤接続してはいけない。

### Source

現行の`sourceId`はplugin stateに保存されるため、DAW Trackの複製時に重複する危険がある。0.6.0では必ず次を設計・テストする。

1. Registryが同じsession内のduplicate Source IDを検出する。
2. 後から登録したSOURCEは、新しいSource IDを生成してstateへ保存するか、ユーザーへ重複を解消する明確な状態を出す。
3. CL​​UBのRouteConfigは、解決後のSource IDへ安全に移行する。
4. 原因不明の上書きやrouteの消失を起こさない。

## Gate F — Legacy State Migration

既存schema≤6のLive Setを可能な限り壊さない。

| 旧値 | Dynamic Sceneへのmigration |
|---|---|
| `speakerLevel1..4` | 最初の4 Speaker SlotのLevelへコピー |
| `speakerType1..4` | 最初の4 Speaker SlotのTypeへコピー |
| `speakerPositionX/Y1..4` | 最初の4 Speaker SlotのPositionへコピー |
| `listenerPositionX/Y` | Listener Positionへコピー |
| 旧4 Speaker全経路 | legacy default Routeとして作る |

現行prototypeは4 Speaker全体へのmono分配を`/4`で正規化している。新規SceneではSpeaker数で自動normalizationしない。ただしmigration時だけ、legacy routeに内部`legacyGain = 0.25`を持たせ、旧Live Setの出力Levelを可能な限り維持する。

旧APVTS parameter IDは削除しない。最初の4 Speaker slotと互換parameter bankとして残す。5〜16 Speakerの状態は、0.6.0ではautomation対象外のcustom ValueTree Scene stateとして保存してよい。RouteConfigもcustom ValueTree stateとして保存する。

## Gate G — 4ch / 8ch Host I/O Feasibility Spike

4ch / 8ch本実装は0.11.0だが、I/O feasibilityは0.6.0の設計ゲートで検証する。少量の調査または隔離prototypeで、少なくとも次を確認する。

1. JUCE AudioProcessorのBusesLayoutでStereo、Quad、8ch discrete / Octagonalをどう表すか。
2. VST3とAUで、target DAWがeffect pluginのmulti-channel outputをどう提示するか。
3. Ableton Live、Logic Pro、Reaperのどれで何が可能か。
4. SOURCEがrouteごとのper-Speaker feedをhost busへ渡す現実的な方法。
5. Preview busとdiscrete output busを同時に出す場合のplugin bus構成。

結果を`OUTPUT_FEASIBILITY_SPIKE.md`として残す。対応可否がhostごとに異なる場合、製品UIで誤解させない。

## Gate H — 0.6.0の最小範囲

0.6.0で実装してよいのは、次だけである。

| 実装する | 実装しない |
|---|---|
| Dynamic Speaker / Routeのcontrol data model | Speaker add/remove UI |
| fixed-capacity compiled Scene / source別RoutePlan | Band DSP |
| Stable Speaker / Route ID | HRTF / binaural renderer |
| duplicate Source ID検出とmigration設計 | QUAD / OCTA本実装 |
| legacy-equivalent Stereo route path | RTA |
| Output feasibility spike | CUSTOM Speaker profile UI |
| unit tests、migration tests、capacity tests | 大規模な新UI |

0.6.0の完了条件は「可変Virtual Clubの基礎を正しく保存・compile・Realtime readできること」であり、「見栄えのよいUI」や「4ch音が出ること」ではない。

## 0.6.0開始の承認条件

次のすべてを満たした時だけ、コード実装を開始する。

- [ ] Routing stateのauthoritative ownerがCLUBである。
- [ ] Speaker FeedとListener Previewが別層である。
- [ ] `SUM_MONO`のStereo SOURCE入力規則が決まっている。
- [ ] fixed capacityとoverflow方針が決まっている。
- [ ] Speaker slot reuse / Source ID duplicateの規則が決まっている。
- [ ] schema≤6 migrationとlegacy `/4`互換方針が決まっている。
- [ ] 4ch / 8ch feasibility spikeを実施する計画がある。
- [ ] 0.6.0ではBand、HRTF、4/8ch、RTA、new UIを実装しないことに合意している。

## Architecture Gate Review Resolution

ChatGPTの設計レビューを反映し、以下を0.6.0開始時点の確定判断とする。この節が前節と衝突する場合は、この節を優先する。

### Control Plane / Audio Worker

| 役割 | 確定した責務 |
|---|---|
| CLUB | Control Plane。Speaker、Route、Listener、Output設定のauthoritative stateを保存し、Scene Compilerを実行する。 |
| SOURCE | Audio Worker。自分のSource IDとSession membershipを持ち、自分専用にcompile済みのRoutePlanだけをaudio callbackで処理する。 |
| SessionRegistry | Audio bufferを持たない。CLUBがcompileしたprimitive realtime planをSOURCEへ公開する。 |

1 Sessionには**authoritative CLUB publisherを1つだけ**許可する。同じSessionへ2個目のCLUBが登録された場合は、silent last-writer-winsにせずConflict状態にする。Conflict側はScene publishを行わず、UIでユーザーへ明示する。

### 3層Audio Model

Speaker Feedを次の3層に分ける。

```text
SOURCE INPUT
  → Route Contribution (FULL / 将来のBAND)
  → Virtual Speaker Voice (Type response / Level / Enable / Mute)
  → Preview Renderer または Discrete Output Mapper
```

Speaker Position、Listener Position、距離、HRTFはPreview Rendererだけに作用する。これらはVirtual Speaker VoiceやDiscrete 4ch / 8ch Feedを変更しない。これによりBinaural Previewを改善しても、作品として出力するSpeaker Feedは変わらない。

SOURCEごとに分散して処理するV1では、Virtual Speaker VoiceのDSPは**線形処理だけ**に限定する。Speaker Type filter、level、muteは許可するが、saturation、speaker distortion、limiter、nonlinear compressionのように「SOURCEごとの処理結果を後で加算してもSpeaker合算後の結果と等しくならない」DSPは追加禁止とする。

### Route Input Rule

0.6.0で実際に処理する入力は `SUM_MONO = 0.5 * (L + R)` のFull Routeだけとする。RouteConfigには将来の互換性のため、次のfieldを今から保存する。

| InputChannelMode | 0.6.0 |
|---|---|
| `SUM_MONO` | 実装・テスト対象 |
| `LEFT` | data modelのみ予約 |
| `RIGHT` | data modelのみ予約 |
| `STEREO_PAIR` | 2本のRouteで表す将来概念 |

### ID / Slot Reuse / Duplicate Rules

Speaker Routeのrealtime参照は、`speakerSlot + speakerGeneration` を使う。Speaker削除後に同じslotを別Speakerへ再利用した場合、generationが一致しない古いRouteは無効として扱う。古いRouteを新Speakerへ自動接続してはいけない。

同一Sessionでduplicate Source IDが登録された場合、後から登録したSOURCEは新しいSource IDを生成して登録する。既存Routeは自動で複製しない。これはDAW Track複製で意図しないSpeaker routingを作らないための安全規則である。

### State Authority / Legacy Bridge

0.6.0以降のauthoritative stateはDynamic Sceneである。旧APVTS parameterは削除しないが、最初の4 Speaker Slotとの**compatibility bridge**として扱う。Dynamic Sceneと旧APVTSの二重authoritative ownerを作らない。

Legacy schema≤6を開く時は、4 Speaker parameterをSlot 0〜3へ移し、legacy routeに限って内部linear gain `0.25` を設定する。新Sceneのroute gainはunityを初期値とし、Speaker数で自動normalizationしない。

### Host I/O Decision Gate

0.6.0では4ch / 8ch Output backendを不可逆に決めない。隔離された`OUTPUT_FEASIBILITY_SPIKE.md`を作り、Stereo In→Stereo Out、Stereo In→Quad Out、Stereo In→8ch Out、Stereo Preview + Aux 8chの各案について、VST3/AUとAbleton/Logic/Reaperのbus negotiationを確認する。

結果は0.6.xのArchitecture Decision Recordとして確定する。SOURCE audioを最終multi-channelへ集約する方式は、このspikeの結果が出るまで固定しない。

### 0.6.0の追加テスト必須項目

- 0 / 1 / 16 Speaker、0 Route、16 Route per Source、512 Global Route。
- 513本目のRouteをcontrol側で明確にrejectすること。
- Speaker delete、slot reuse、generation mismatch、deleted Speakerへのstale Route。
- duplicate Source ID、duplicate CLUB publisher。
- schema≤6 migration、legacy 4 Speaker mapping、legacy `0.25` gain、新Scene unity route、no auto-normalization。
- Route enable / disable、Scene revision更新、save / restore、CLUB/SOURCE作成順。
- 将来の必須QAとして100 SOURCE、offline render、block size / sample rate変更、rapid scene update、route deletion during processingをロードマップへ残す。

### 最終Gate判定

| Gate | 判定 |
|---|---|
| A: Routing ownership | PASS |
| B: Speaker Voice / Preview separation | PASS（3層化・線形DSP制約込み） |
| C: Stereo input rule | PASS |
| D: Fixed capacity plan | PASS |
| E: Stable ID / duplicate handling | PASS（duplicate CLUB込み） |
| F: State migration | PASS |
| G: 4/8ch | Plan PASS。technical resultはspike待ち |
| H: 0.6 scope | PASS |

**0.6.0は設計上GOとする。** ただし、Output backendを決め打ちせず、Gateの実装範囲を超えるBand DSP、HRTF、4/8ch本実装、RTA、Dynamic Speaker UIを追加しないことを開始条件とする。
