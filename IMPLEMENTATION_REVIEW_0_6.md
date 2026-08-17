# Club Craft 0.6.0 — ChatGPT実装提案レビュー

## 結論

ChatGPTの提案は、`ARCHITECTURE_GATE_0_6.md` のGate A〜Hと整合しており、**0.6.0の実装計画として条件付きで採用可能**である。特に、CLUBをControl Plane、SOURCEをAudio Workerとする責務分離、source別固定容量RoutePlan、Speaker slot + generation、legacy route gain `0.25`、Output feasibility spikeは正しい。

ただし、実装を始める前に、この文書の「必須修正」を計画へ組み込むこと。以下の修正は、Realtime安全性・旧Live Set互換・将来の4/8ch拡張を守るために必要である。

## 採用する構成

| 提案 | 判定 | 採用理由 |
|---|---|---|
| `SceneModel.h`のcontrol-side Dynamic Scene | 採用 | string、vector、Stable IDをcontrol threadへ限定できる。 |
| `RealtimePlan.h`の固定容量snapshot | 採用 | audio callbackからvector、map、UUIDを排除できる。 |
| source別`SourceRoutePlan` | 採用 | 各SOURCEがGlobal Route全件を走査せず、最大16 Routeだけ処理できる。 |
| `SceneCompiler` | 採用 | Control SceneとRealtime planの責任を分けられる。 |
| Speaker slot + generation | 採用 | slot再利用時のstale Route誤接続を防ぐ。 |
| Legacy default routing + gain 0.25 | 採用 | 旧4 Speaker prototypeの出力Levelを可能な範囲で保てる。 |
| `SourceRouteStereoRenderer` | 採用 | 0.6の間はStereo Previewを保持しつつ、旧4 Speaker rendererを主経路から外せる。 |
| Output feasibility spike | 採用 | 4ch / 8ch backendを早期に不可逆に決めない。 |

## 必須修正

### 1. 型の分離

`SceneModel.h`内で新しい`PlanarPosition`を定義してはいけない。現行`SessionRegistry.h`にも同名型があり、二重定義になる。`PlanarPosition`、`RouteMode`、`InputChannelMode`、容量定数のようにRealtime側も必要な小さな値型は、`SceneTypes.h`などの軽量ヘッダーへ置く。

`RealtimePlan.h`は`SceneModel.h`をincludeしない。`SceneModel.h`には`std::vector`と`std::string`があり、audio thread用ヘッダーへ誤ってcontrol-side型を持ち込む危険がある。Realtime Planは`SceneTypes.h`、`SpeakerType.h`、`<array>`、`<cstdint>`だけへ依存させる。

### 2. Dynamic Stateの保存形式

現行APVTS rootは`ClubCraftPhase1State`であり、`parameters.replaceState()`はこのrootを前提にする。schema 7で`ClubCraftState`というwrapperを導入するなら、次の形式を明示する。

```text
ClubCraftState
├ schemaVersion
├ sessionId
├ sourceId
├ APVTS                  ← ClubCraftPhase1Stateのcopy
└ DynamicScene
   ├ Speakers
   └ Routes
```

旧stateを読む時は、root自身が`ClubCraftPhase1State`ならlegacyとして`parameters.replaceState(root)`を行う。新stateを読む時は`APVTS` childだけを`parameters.replaceState()`へ渡す。Dynamic Sceneがauthoritativeで、旧APVTS parameterはSlot 0〜3のcompatibility bridgeに限定する。

### 3. Source ID rekeyの戻り値

`registerSource()`はvoidのままにしない。duplicateを検出した時に、Processorが新しいIDをstateへ保存できる結果型が必要である。

```text
SourceRegistrationResult
- acceptedSourceId
- rekeyed
- conflict / error state
```

rekeyはTimerなどcontrol threadでだけ実行する。audio callbackでUUID生成、string比較、state変更を行わない。rekeyされたSOURCEに既存Routeを自動コピーしない。

### 4. CLUB publisher conflict

CLUB publisher登録は`sessionId`だけでは不十分である。各Plugin instanceのruntime tokenまたはpublisher IDを使い、Sessionごとに1個だけauthoritative publisherを許可する。2個目はConflictとして扱い、Scene publishを拒否する。先に存在したauthoritative CLUBが消えた時のownership移行もtestする。

### 5. Speaker slot assignmentの永続性

`SceneCompiler`のslot assignmentとgenerationはcompileごとに初期化してはいけない。前回assignmentを保持し、同じStable Speaker IDには同じslotとgenerationを維持する。削除されたslotを再利用する時だけgenerationをincrementする。

Control側の`CompiledScene`が`unordered_map<string, SourceRoutePlan>`を持つことは許可するが、Registryへ公開する前にsource別fixed-capacity planへ変換する。audio callbackではstring、map、vectorを読まない。

### 6. Route重複の扱い

同じSourceとSpeakerを指すRouteを単純に`duplicateRoute`として拒否しない。将来のFULL + BAND、異なるBand複数本、異なるinput channel modeでは、同じSource→Speakerが正当な複数Routeになり得る。重複判定が必要なら、Stable Route IDの重複だけをrejectし、routing条件の同一性は0.6では許可またはUI警告に留める。

### 7. SOURCE DSP state

Speaker Type filterやGeneric Responseのような線形DSPをsourceごとに正しく処理するには、`SourceRouteStereoRenderer`が`kMaxRoutesPerSource`分のfilter / delay stateを`prepareToPlay()`で確保する。Routeが別Speaker slotを指すよう変化した場合、state reset / gain rampをcontrol-safeに扱う。新Routeはunity gainだが、旧legacy routeだけ`0.25`を使う。Speaker数で割る正規化は行わない。

### 8. SceneとRoutePlanのrevision

Realtime SceneとSourceRoutePlanは同じcompile transactionのrevisionを持つ。SOURCEはrevision不一致のScene + Planを混ぜて使わない。無音を避けるため、SOURCE processorは直近の整合したscene / planを固定容量のlocal copyとして保持し、瞬間的な不一致時にはそれを使う。local copyにもallocationを伴う型を入れない。

## 0.6.0で実装しないこと

- Speaker add/removeの完成UI、Top View、箱庭UI。
- Band filter DSP、Band routing UI。
- HRTF、完成版Binaural、room reflection。
- QUAD / OCTAの本出力。
- RTA、FFT、CUSTOM Speaker profile。
- Speaker saturation、distortion、limiter、nonlinear compression。

## 実装順序

| 順序 | 内容 | 実装後の確認 |
|---|---|---|
| 1 | `SceneTypes.h`、`SceneModel.h`、`RealtimePlan.h` | 値型がRealtime / controlへ適切に分離される。 |
| 2 | `SceneCompiler` | 0〜16 Speaker、0〜512 Route、slot / generation、capacityをunit testする。 |
| 3 | `SessionRegistry`拡張 | source別Plan公開、duplicate SOURCE / CLUB、revision整合をtestする。 |
| 4 | `SceneState`とschema 7 migration | schema≤6、legacy four routes、gain 0.25、APVTS bridgeをtestする。 |
| 5 | Processor統合 | CLUB compile/publish、SOURCE handle/rekey、legacy roleを確認する。 |
| 6 | `SourceRouteStereoRenderer` | Full + SUM_MONO、no normalization、stale Route無効化、legacy audio経路をtestする。 |
| 7 | 最小Status UIとOutput spike | Conflict / rekey / connection表示、`OUTPUT_FEASIBILITY_SPIKE.md`を追加する。 |
| 8 | 全体検証 | CTest、VST3、AU、macOS Universal build、0.5 Live Set migration試用。 |

## 実装開始前チェックリスト

- [ ] `ARCHITECTURE_GATE_0_6.md`を読む。
- [ ] 本レビューの「必須修正」を受け入れる。
- [ ] source / route / speakerのcontrol stateとrealtime stateを混ぜない。
- [ ] `processBlock()`にlock、allocation、UUID、string、map、ValueTree操作を入れない。
- [ ] state wrapper / legacy rootの読み分けを明文化する。
- [ ] Output backendをspike前に固定しない。
- [ ] Band、HRTF、4/8ch、RTA、新UIを0.6へ追加しない。
