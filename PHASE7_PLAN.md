# Club Craft 0.7.0 — Dynamic Speaker & Routing UI 実装計画

## 目的

0.6.0で導入したDynamic Scene、Stable Speaker ID、source別RoutePlanを、初めてユーザーが直接操作できるようにする。0.7.0の目的は、**CLUBを仮想会場の司令塔にし、Speakerを置き、Listenerを動かし、SOURCEをSpeakerへ明示的に接続する**ことにある。

> SOURCEは固定の音源workerであり、配置操作はしない。CLUBだけがSpeaker、Listener、Routeを編集する。

## 今回の範囲

| 0.7.0で実装する | 0.7.0では実装しない |
|---|---|
| 最大16 Speakerの追加・削除 | BAND DSP / crossover処理 |
| 2D Club Floor View上のSpeaker自由配置 | HRTF / 完成版Binaural |
| Listenerの2D配置 | 4ch / 8ch本出力 |
| SOURCE一覧とSpeakerへのFull Route追加・削除 | saturation / limiter / nonlinear DSP |
| Routeごとのmute・gain | RTA / 周波数分析UI |
| schema 7 Dynamic Scene保存・復元 | SOURCEの位置操作 |
| legacy 4 Speaker Setを壊さないmigration | Sceneを複数CLUBで共同編集する機能 |

## ユーザー操作モデル

### 1. CLUB FLOOR VIEW

CLUB画面中央に、X/Y平面の会場を表示する。中央のステージ原点は固定であり、SOURCEを表す小さな`STAGE / SOURCES`マーカーを置く。Listenerは円形の`AUDIENCE`マーカー、SpeakerはType色ではなく控えめな役割ラベル付きカードとして表示する。

| 操作 | 結果 |
|---|---|
| `+ ADD SPEAKER` | 新しいSpeakerを安全な空き位置へ追加する。初期Typeは`FULL RANGE`、Levelは`-12 dB`、enabledはtrue。 |
| Speakerをクリック | InspectorでName、Type、Level、X/Y、Mute、Deleteを操作できる。 |
| Speakerをドラッグ | CL​​UB control thread上の`DynamicScene.speakers[n].position`だけを更新する。audio threadへ直接触れない。 |
| Listenerをドラッグ | `DynamicScene.listener`を更新する。Previewのpan / distanceだけが変わる。 |
| `DELETE SPEAKER` | Speakerを削除し、そのSpeakerを指すRouteも同じcontrol transactionで削除する。 |
| `RESET LAYOUT` | 初期4 Speaker配置へ戻す。ただし確認ダイアログを出す。 |

初期4 Speakerは名前を`FRONT L`、`FRONT R`、`REAR L`、`REAR R`のまま維持する。追加Speakerは`SPEAKER 5`のように連番で表示し、Stable IDにはUUIDを使う。UUID生成はUI操作時だけであり、audio callbackでは絶対に行わない。

### 2. ROUTING MATRIX

Floor Viewの下部または右側に`SOURCE ROUTING`を置く。行は現在Sessionへ登録されているSOURCE、列は現在のDynamic Speakerとする。セルは明示的なRouteの有無を表す。

| セル状態 | 操作 | 保存されるRoute |
|---|---|---|
| 空 | クリックでRoute作成 | `FULL + SUM_MONO + gain 1.0 + enabled true` |
| 接続済み | クリックでRoute削除 | 対応する`RouteConfig`を削除 |
| mute | セル内の小さなmute操作 | Routeは残し`enabled=false` |
| gain | Inspectorまたはセルの簡易gain | `gainLinear`を更新 |

同じSOURCEとSpeakerの間でも、将来はFULLとBANDが共存できる。そのためRouteの識別は`sourceId + speakerStableId`ではなく、必ず`RouteConfig.stableId`を使う。0.7.0 UIが作るのはFULL Routeだけである。

### 3. LEGACY SETの表示

schema≤6からmigrationしたSetでは、4 Speakerへのimplicit Routeが内部生成される。0.7.0のRouting Matrixでは、これを`LEGACY DEFAULT ROUTING`として最初に表示する。ユーザーが初めてRouteを追加・削除・mute・gain変更した時点で、確認ダイアログを出す。

> 「この操作により、旧4 Speakerの自動Routingを明示的なRouteへ変換します。以前のSetへ戻す場合は、バックアップから復元してください。」

承認後、4×登録SOURCEのRouteを`gain=0.25`で実体化し、`legacyDefaultRouting=false`にする。キャンセルなら何も変更しない。

## データとAPIの追加計画

`PluginProcessor`はControl-side Dynamic Sceneの編集APIを持つ。Editorは`dynamicScene`へ直接アクセスしない。

| API候補 | 実行スレッド | 説明 |
|---|---|---|
| `getSceneForEditor()` | message thread | コピーを返す。audio threadでは呼ばない。 |
| `addSpeaker()` | message thread | 容量を確認してSpeakerConfigを追加する。 |
| `removeSpeaker(stableId)` | message thread | Speakerと関連Routeを同一transactionで削除する。 |
| `updateSpeaker(stableId, patch)` | message thread | Type、Level、Position、enabledを更新する。 |
| `updateListener(position)` | message thread | Listener位置を更新する。 |
| `createFullRoute(sourceId, speakerId)` | message thread | UUID Routeを追加する。重複Routeを作らない。 |
| `removeRoute(routeId)` | message thread | 対象Routeだけを削除する。 |
| `setRouteEnabled(routeId, enabled)` | message thread | muteを更新する。 |
| `setRouteGain(routeId, gainLinear)` | message thread | gainを安全な範囲へclampして更新する。 |
| `materialiseLegacyRoutes()` | message thread | legacy 0.25 gain Routeを明示Routeへ変換する。 |

各編集APIは、`sceneEditMutex`でDynamic Sceneを書き換える。ただしaudio callbackはmutexを取得しない。Timerまたはmessage threadが編集済みSceneを`SceneCompiler`でcompileし、0.6.0のprimitive atomic Realtime Scene / source別RoutePlanとしてpublishする。

## Realtime安全性

| 禁止 | 理由 | 代替 |
|---|---|---|
| `processBlock`内でvector / string / map / ValueTreeを読む | allocation・lock・非決定的時間が起こり得る | 固定容量`RealtimeSceneSnapshot`と`SourceRoutePlan`だけを読む。 |
| UIからRealtime slotを直接書く | seqlock整合性を壊す | Control Sceneを編集し、Compiler経由でpublishする。 |
| Speaker削除時にslot番号だけを見る | slot再利用で別SpeakerへRouteが誤接続する | slot + generationをRoutePlanへ含める。 |
| 2個目のCLUBがSceneをpublishする | ownership競合で音が不安定になる | authoritative CLUBだけがpublishする。 |

## 実装順序

| 順番 | 実装内容 | 完了条件 |
|---:|---|---|
| 1 | ProcessorのControl Scene編集APIとsceneEditMutex | UIなしでSpeaker / Routeを追加・削除でき、stateに保存される。 |
| 2 | Dynamic Scene編集とlegacy materialisationの単体テスト | Speaker削除でRouteも消える。legacy Routeは0.25 gainで明示化される。 |
| 3 | Floor Viewの描画と選択 | 4 SpeakerとListenerを表示し、選択状態を保てる。 |
| 4 | Speaker追加・削除・Inspector | 16 Speaker上限、空き位置、Type / Level / X/Y / Muteが動く。 |
| 5 | dragによるSpeaker / Listener配置 | drag中はUIだけを更新し、drag終了またはthrottleでcompile / publishする。 |
| 6 | Routing Matrix | SOURCE一覧、Full Route作成・削除、mute、gainを操作できる。 |
| 7 | legacy確認ダイアログ | 旧SetのRoutingがユーザー承認後にだけmaterialiseされる。 |
| 8 | state / migration / reset / host automation検証 | Set保存・再読込、Role切替、duplicate SOURCE / CLUBでも安全。 |
| 9 | VST3 / AU buildとMac Live試用 | Intel Macでbuild-release、Live再スキャン、既存Setと新規Setを確認。 |

## テスト計画

### 単体テスト

| テスト | 合格条件 |
|---|---|
| Speaker add | 16本まで追加でき、17本目は失敗してSceneを変更しない。 |
| Speaker remove | Speaker削除時に関連する全Routeが削除される。 |
| Speaker slot reuse | 削除後に追加しても旧Routeが新Speakerへ誤接続しない。 |
| Route add | 同じFULL Routeの重複をUI APIが拒否する。 |
| Route delete / mute / gain | 対象Routeだけを変更し、他SOURCEのRoutePlanは破損しない。 |
| Legacy materialisation | 登録SOURCE×4 Speaker分の0.25 gain Routeが一度だけ生成される。 |
| State round-trip | 16 Speaker、512 Route、Listener、selected UI stateを保存・復元できる。 |
| Capacity rejection | 容量超過がcontrol側で失敗し、Realtime Planを破壊しない。 |

### UI / 実機テスト

| テスト | 合格条件 |
|---|---|
| Floor View | SpeakerとListenerの位置が再起動後も一致する。 |
| Speaker drag | Speakerを左右へ動かすとStereo Previewの左右バランスが聴感上変化する。 |
| Listener drag | ListenerをSpeakerへ近づけると、当該Speaker Routeの寄与が変化する。 |
| Route作成 | SOURCE→Speakerを1本だけ接続すると、そのSpeaker Voiceだけが鳴る。 |
| Route削除 | 最後のRouteを消すとSOURCEは無音になり、他SOURCEは鳴り続ける。 |
| Legacy Set | 既存0.6 Setは、明示Routeへ変換するまで旧音量感を保つ。 |

## 0.7.0の完了条件

0.7.0は、次のすべてを満たした時だけ完了とする。

1. CL​​UBで最大16 Speakerを追加・削除・自由配置できる。
2. Listenerを自由配置できる。
3. CL​​UBでSOURCE→SpeakerのFull Routeを追加・削除・mute・gain操作できる。
4. schema 7 stateにすべて保存され、Set再読込後に復元する。
5. legacy 4 Speaker Setがユーザー承認なしにRoute構造を変更しない。
6. audio callbackのRealtime安全性を崩さない。
7. Debug / Release CTest、VST3、macOS Intel Universal Release、AUが通る。
8. `PHASE7_TRYOUT.md`のAbleton Live試用を完了する。

## ChatGPTレビューに出す論点

1. Scene edit mutexとTimer publishの分離はJUCE host上で安全か。
2. legacy materialisationの確認UXとrollback要件は十分か。
3. Floor Viewのdrag throttleは何Hzが適切か。
4. Routing Matrixを最大128 SOURCE × 16 Speakerでどう仮想化するか。
5. `FULL` Route重複rejectと将来`BAND`共存を両立するRoute identity設計は正しいか。
6. Source registrationの変化をCLUB UIへ反映するpolling周期と負荷は妥当か。
7. 0.7.0でroute gainをdB表示にするかlinear保存にするか。

## ChatGPTレビュー反映 — 実装開始前の必須修正

ChatGPTレビューにより、初版計画はそのままでは実装開始しない。詳細は`ARCHITECTURE_GATE_0_7.md`を正本とする。

| ブロッカー | 0.7.0での確定修正 |
|---|---|
| Legacy APVTS bridge | DynamicSceneをauthoritativeにする。TimerによるAPVTS→DynamicSceneの一方向上書きを廃止し、旧automation時だけStable ID基準でDynamicSceneへ反映する。 |
| 無条件compile / publish | `sceneDirty`、`sourceMembershipDirty`、`urgentPublish`を導入し、drag中のcompile / publishは最大30Hz、mouseUpは即時publishとする。 |
| Route容量 | `MAX_ROUTES_GLOBAL`を512から2048へ変更し、128 SOURCE × 16 SpeakerのMatrixと整合させる。 |
| 編集transaction | candidate Sceneのcopy → edit → validate / preflight → success時swapを必須にする。compile / Registry publish中はscene mutexを保持しない。 |
| Legacy materialisation | materialiseと最初のRoute操作を同一transactionにする。capacity failureまたはCancel時はSceneを変更しない。 |
| gain / mute / drag | Route gain、mute、Speaker Level、pan / path gainへ10〜20ms程度のsmoothingを追加する。 |
| lifecycle UI | offline SOURCE Routeを保持して灰色表示する。duplicate CLUBはread-only、rekey SOURCEは`NEW / UNROUTED`と表示する。 |

Floor Viewの`RESET LAYOUT`は`RESET POSITIONS`へ改名し、SpeakerまたはRouteを削除しない。Speaker削除時は「関連Route N本も削除される」ことを確認表示する。

> 以後、0.7.0の実装判断は`ARCHITECTURE_GATE_0_7.md`を優先する。初版の`PHASE7_PLAN.md`と矛盾する場合は、Gateの定義を採用する。

## 修正後の実装順序

| 順番 | 実装内容 | 完了条件 |
|---:|---|---|
| 0 | Gate A〜Hの再レビュー | ChatGPTが修正版にGOを出す。 |
| 1 | Legacy bridge反転、scene edit transaction、dirty publish | Floor Viewを作る前にDynamicSceneがauthoritativeになる。 |
| 2 | Route token / generation、smoothing、2048 Route容量、追加テスト | 連続gain / mute / dragがclickなく、安全に動作する。 |
| 3 | Floor View、Inspector、Speaker add / delete / listener drag | 最大16 Speakerを安全に編集できる。 |
| 4 | virtualized Routing Matrix、offline source表示、legacy materialisation dialog | Full Routeを明示操作できる。 |
| 5 | state / migration / VST3 / AU / Ableton Live検証 | 0.7.0の完了条件を満たす。 |

## 追加された完了条件

1. Speaker drag後にTimerが旧APVTS値で位置を戻さない。
2. 最後のRoute削除後、SOURCEが古いRoutePlanを使い続けない。
3. 2,048 Route上限とlegacy materialisationの境界がテスト済みである。
4. 60〜120Hzのdrag eventでもcompile / publishが30Hzを超えず、mouseUpで最終位置が即時publishされる。
5. duplicate CLUBではScene編集がread-onlyになり、owner消滅後に次CLUBが編集可能になる。
6. Debug / Release CTest、VST3 / AU、Intel Mac / Ableton Live試用を通過する。
