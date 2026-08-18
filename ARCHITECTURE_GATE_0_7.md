# Club Craft 0.7.0 — Dynamic UI Architecture Gate

## 判定

ChatGPTレビューにより、初版の`PHASE7_PLAN.md`はそのままでは**NO-GO**と判定した。Floor ViewとRouting Matrixの方向は正しいが、以下のGateをすべて満たすまで実装を開始しない。

> 0.7.0では、DynamicSceneが唯一のauthoritative control stateである。旧APVTS parameterはschema≤6互換とDAW automation bridgeのためだけに残し、TimerがDynamicSceneを一方向に上書きしてはならない。

## Gate A — Legacy APVTS bridgeの方向を反転する

### 問題

0.6.0はTimerごとに`DynamicScene.speakers[0..3]`へ旧APVTS parameterを書き戻す。Floor ViewがLegacy Speakerを移動しても、次のTimerで元に戻る危険がある。

### 決定

| 操作の起点 | データの流れ |
|---|---|
| Floor View / Inspector / Routing Matrix | UI edit → DynamicScene → compile / publish → legacy APVTS mirror（必要な最初の4 Speakerだけ） |
| 旧APVTS automation | parameterChanged → stable ID `legacy-speaker-N`を検索 → DynamicScene編集 → compile / publish |
| 新規Speaker 5〜16 | DynamicSceneだけを操作する。旧APVTSへ割り当てない。 |

- Legacy mappingはvector indexではなく`legacy-speaker-1`〜`legacy-speaker-4`のStable IDを使う。
- Speaker削除後に、同じvector indexへ入った新Speakerを旧automationが操作してはならない。
- parameter mirrorの更新ではfeedback loopを防ぐ`isSynchronisingLegacyMirror` control-side guardを使う。
- `RESET POSITIONS`は位置だけを初期4 Speaker位置へ戻す。RouteやSpeakerを消さない。

## Gate B — Scene edit transaction

すべての破壊的UI操作は、現在Sceneを直接変更しない。次の手順で行う。

1. `sceneEditMutex`を短時間取得し、authoritative DynamicSceneのコピーを作る。
2. mutexを解放する。
3. candidate SceneへSpeaker / Route / Listenerの編集を適用する。
4. candidate Sceneをcontrol-side validateし、必要ならSceneCompilerでpreflightする。
5. 成功時のみ`sceneEditMutex`を短時間取得してauthoritative Sceneとswapし、`sceneDirty=true`にする。
6. 失敗時はauthoritative Sceneを一切変えない。

`sceneEditMutex`を持ったまま、SceneCompiler、SessionRegistry、ValueTree、UI callback、Host parameter通知を呼ばない。これによりmutex順序の循環とaudio threadへの影響を避ける。

### Speaker削除

Speaker削除はcandidate transaction内で次を原子的に行う。

- 対象Speakerを削除する。
- 対象SpeakerのStable IDを参照するすべてのRouteを削除する。
- legacy modeなら先にmaterialisation確認を要求する。
- preflight失敗なら全変更をrollbackする。

## Gate C — Scene dirty / membership dirty / publish cadence

| フラグ | 設定する条件 | compile / publish |
|---|---|---|
| `sceneDirty` | Speaker、Listener、Route、gain、mute、legacy materialisation編集 | 最大30Hzで実行。 |
| `sourceMembershipDirty` | SOURCEのregister、unregister、rekey、heartbeat期限 | 最大30Hzで実行。 |
| `urgentPublish` | mouseUp、Route削除、最後のRoute削除、CLUB ownership取得 | 次のTimer tickを待たずmessage threadから安全にscheduleする。 |

- UI描画は60Hzでもよいが、SceneCompilerはmouseDrag eventごとに呼ばない。
- drag中のpublishは最大30Hz、mouseUpは最終位置を即時publishする。
- source membershipの変化がなく、sceneDirtyもfalseならTimerはcompile / publishしない。
- Realtime SceneとSourceRoutePlanは同一revisionでpublishされる。SOURCEは一致しない組合せを処理しない。

## Gate D — Route容量の整合

128 SOURCE × 16 SpeakerのFull Routing Matrixは最大2,048 Routeを持つ。`MAX_ROUTES_GLOBAL=512`はこのUI仕様と矛盾するため、0.7.0で以下に更新する。

| 定数 | 0.6.0 | 0.7.0 Gate | 根拠 |
|---|---:|---:|---|
| `MAX_SPEAKERS` | 16 | 16 | UI上限と一致する。 |
| `MAX_SOURCES` | 128 | 128 | 0.6.0 Gateを維持する。 |
| `MAX_ROUTES_PER_SOURCE` | 16 | 16 | 1 SOURCEが16 SpeakerへFull Routeを持てる。 |
| `MAX_ROUTES_GLOBAL` | 512 | **2048** | 128 × 16を完全に表現できる。 |

- legacy materialisationは`registered source count × 4` Routeをcandidateへ作り、requested editを含めて容量preflightする。
- 失敗時は`legacyDefaultRouting=true`のまま維持し、candidateを捨てる。
- Matrixには現在Route数 / 2048を表示する。ただし通常操作が容量上限に達した時だけセルをdisabledにする。

## Gate E — Legacy materialisation

legacy Setを開いた時、implicit 0.25 Routeは内部でのみ存在する。ユーザーが最初にRoute構造を変更しようとした時、確認ダイアログを出す。

| ダイアログ表示 | 内容 |
|---|---|
| 現在のSOURCE数 | `N Sources` |
| 生成されるRoute数 | `4 × N Routes` |
| 互換gain | `0.25 linear`（約`-12.04 dB`） |
| 操作後 | explicit Routeへ変換し、以後はDynamic Routingとして編集可能 |
| Cancel | Sceneはbyte-equivalentに変更しない |

- materialiseと最初のRoute操作は同一candidate transactionに含める。
- 遅れてregisterしたSOURCEには、materialised legacy Routeを自動追加しない。新規SOURCEとして`NO ROUTES`表示にする。
- SOURCE unregister時、persisted `RouteConfig`を自動削除しない。Matrixではoffline rowとして灰色表示する。

## Gate F — Route identityとsmoothing

### Route identity

- 0.7 UIが重複拒否するのは`same source + same speaker + FULL + SUM_MONO`だけである。
- 同SOURCE / 同Speakerでも、将来のBAND Routeは別Stable Route IDとして許可する。
- `CompiledRoute`にはnumeric `routeToken`と`routeGeneration`を追加する。Rendererはこれを使ってgain smootherと将来のBand filter stateを正しいRouteへ対応させる。

### Gain / mute / position smoothing

| 対象 | 保存 | UI | DSP動作 |
|---|---|---|---|
| Route gain | linear | `-60 dB`〜`+12 dB`。unity=`0 dB` | 20ms linear ramp |
| legacy gain | `0.25`を保持 | 約`-12.04 dB` | 再計算しない |
| Route mute | `enabled=false` | MUTE | 10ms gain rampで無音へ遷移 |
| Speaker Level | linear snapshot | dB UI | 20ms ramp |
| Speaker / Listener位置 | position snapshot | 2D drag | 20ms程度でpan / path gainを補間 |

NaN、Inf、負のgain、無効なdB値はcontrol-sideでrejectする。

## Gate G — Matrixとlifecycle UX

- 2,048セルを個別JUCE Componentとして作らない。custom drawing + Viewport + visible row virtualizationを使う。
- SOURCEは`online`、`offline`、`new/unrouted`に分け、Display Name→Source IDでstable sortする。
- 列headerと行headerを固定し、検索と縦横scrollを提供する。
- セルclickはFull RouteのOn/Offだけ。gain、mute、deleteは選択Route Inspectorで編集する。
- Floor ViewとMatrixは双方向selectionを持つ。
- duplicate CLUBは大きなConflict bannerを表示し、non-authoritative CLUBの編集UIをread-onlyにする。ownership取得後に解除する。

## Gate H — 追加必須テスト

0.7.0では既存のSceneCompiler / Registryテストに加え、以下を必須とする。

1. Legacy Speaker drag後にTimerが旧APVTS値で位置を戻さない。
2. Stable ID legacy mappingにより、Speaker削除・追加後も旧automationが別Speakerを操作しない。
3. Scene save中のdragとEditor未作成state restoreが安全。
4. no route / last route deletionで古いlastGoodRoutePlanを使い続けない。
5. legacy materialise cancel、success、capacity failure、requested editとの同一transaction。
6. 128 SOURCE legacy materialisationと2,048 Route境界。
7. SOURCE unregister / re-register後もpersisted Routeが残る。
8. Route gain / mute / position drag中にclick、NaN、Infがない。
9. slot reuse中にstale Routeが新Speakerへ接続しない。
10. 60〜120Hz mouseDragでもcompile/publishが30Hzを超えず、mouseUpの最終位置は即時publishされる。
11. duplicate CLUB takeover後にUIがread-onlyから解除される。
12. Full duplicateはrejectし、将来のBand Route dataは保持可能である。

## 0.7.0 GO条件

上記Gate A〜Hを`PHASE7_PLAN.md`、`CHATGPT_HANDOFF.md`、`CHATGPT_PROMPTS.md`へ反映し、ChatGPTが修正版をレビューして**GO**と判断してから、Processor editing APIとtransaction testsに着手する。

## Gate I — APVTS callbackはSceneを編集しない

`AudioProcessorValueTreeState::Listener::parameterChanged()`は、audio callbackから同期的に呼ばれ得る。したがって、0.7.0ではこのcallbackからDynamicSceneを直接編集しない。

| 場所 | 許可する処理 | 禁止する処理 |
|---|---|---|
| `parameterChanged()` | legacy parameterごとのatomic pending value書込み、atomic dirty bit設定 | mutex、string、vector検索、UUID、ValueTree、SceneCompiler、Registry、Host通知 |
| Timer / AsyncUpdater（message thread） | pending automation取得、Stable ID検索、candidate transaction、APVTS mirror、compile / publish schedule | audio buffer処理 |
| `processBlock()` | 固定容量snapshotとRoutePlan読出しだけ | すべてのcontrol-side操作 |

### Pending automation mailbox

- 最初の4 legacy SpeakerのLevel、Type、X、YとListener、Master、Response Toneについて、atomic pending valueとdirty bitを持つ。
- callbackはpending valueをstoreしてdirty bitをsetするだけで終了する。
- Timerはpending bitを交換して取得し、Stable ID `legacy-speaker-N`でcandidate Sceneへ適用する。
- Floor Viewからlegacy Speakerを編集した時は、DynamicScene commit後にAPVTSをmirrorできる。ただしcallbackで同値pendingを受け取っても、TimerはScene値と比較してno-opにする。
- `isSynchronisingLegacyMirror`は補助guardに留め、正しさをguardだけへ依存しない。

## Gate J — Revision付きcandidate transaction

DynamicSceneはcontrol-side revisionを持つ。すべての編集APIはrevisionを使ったcompare-and-swap型transactionにする。

```text
lock sceneEditMutex
copy authoritative Scene + baseSceneRevision
unlock
apply edit to candidate
validate / preflight candidate
lock sceneEditMutex
if authoritative revision != baseSceneRevision:
    unlock
    retry / rebase candidate from newest Scene
else:
    swap candidate into authoritative Scene
    ++authoritative revision
    sceneDirty = true
    unlock
```

- stale candidateはswapしてはならない。
- retry上限を設け、上限超過時はUIへ編集失敗を通知しauthoritative Sceneを保持する。
- `sourceMembershipRevision`も別atomicで管理する。legacy materialisation dialogは対象Source ID一覧とmembership revisionをsnapshotし、commit時に変更があれば再確認または指定一覧だけを変換する。0.7.0は後者を採用し、ダイアログに「現在表示中のN Sourcesを変換」と明示する。

## Gate K — Persistent Route realtime slot / generation

`RouteConfig.stableId`をcontrol-side identityとし、CompilerはStable Route IDを固定容量の`routeSlot + routeGeneration`へ対応させる。

| 操作 | routeSlot / routeGeneration |
|---|---|
| 既存Stable Routeを再compile | 同じslot、同じgenerationを維持する。 |
| Routeを削除 | slotをinactiveにする。generationは保持する。 |
| 新しいRouteが空きslotを再利用 | generationをincrementする。 |
| plan内の並び順だけが変わる | Route identityは変えない。 |

Rendererのgain smoother、mute ramp、将来のBand filter stateは`routeSlot + routeGeneration`に紐付ける。RoutePlan配列indexに紐付けてはならない。

### Muteの実装境界

- `enabled=false`は保存上のmute状態を表す。
- Rendererではdisabled Routeを即skipせず、target gainを0にして10ms rampが完了するまでVoice stateを処理する。
- Speaker muteも同じ方式でtarget speaker gainを0へrampする。
- 0 routeの新revisionは有効なPlanであり、古い`lastGoodRoutePlan`を継続処理してはならない。

## Gate L — 正本文書の同期

0.7.0の実装開始前に、以下を必ず同一仕様へ更新する。

| 文書 | 必須更新 |
|---|---|
| `PHASE7_PLAN.md` | 2048 Route、pending automation、revision transaction、Route slot lifecycle、追加テストを反映する。 |
| `CHATGPT_HANDOFF.md` | 0.7でFloor View、Listener drag、Full Routingを実装する新ロードマップへ更新し、旧0.8 / 0.10分割と512 Routeを削除する。 |
| `CHATGPT_PROMPTS.md` | 0.7 Gateと2048 Route、pending automation、revision transactionを明示し、0.6専用の512 Route前提を削除する。 |
| `README.md` | `ARCHITECTURE_GATE_0_7.md`を0.7の唯一の実装判断正本として示す。 |

## Gate M — 再レビューで追加された必須テスト

Gate Hのテストに、以下を追加する。

1. APVTS parameter callbackがScene mutex、vector、stringを触らずatomic mailboxだけを更新する。
2. pending automationがmessage threadで正しいlegacy Stable IDへ適用される。
3. UI edit → APVTS mirror → callbackで無限更新が起きない。
4. candidate作成後にScene revisionが変わった時、stale candidateがswapされずretry / rebaseされる。
5. materialisation dialog中のSOURCE register / rekeyに対し、snapshot対象Sourceだけを安全に変換する。
6. RoutePlanの並び替えでsmootherが別Routeへ移らない。
7. Route slot再利用でgenerationが変わり、旧DSP stateを再利用しない。
8. Route muteとSpeaker muteが10ms ramp後にsilentになり、clickを出さない。
9. gain 0、NaN、Inf、負のgainがUIまたはDSPへ入らない。
10. pending automation中にHost state保存しても、保存Stateが矛盾しない。

## 更新後のGO条件

Gate A〜Mを`PHASE7_PLAN.md`、`CHATGPT_HANDOFF.md`、`CHATGPT_PROMPTS.md`、READMEへ同期し、`CHATGPT_REVIEW_PROMPT_0_7_FINAL.md`による最終レビューでGOを得るまで、0.7.0のコード実装は開始しない。
