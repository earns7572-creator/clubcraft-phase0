# Club Craft — 次のAI / Manusに渡す開始プロンプト

以下をそのまま使用する。

```text
Club Craftの開発を引き継いでください。

Repository:
https://github.com/earns7572-creator/clubcraft-phase0

最初に CHATGPT_HANDOFF.md と ARCHITECTURE_GATE_0_6.md を最初から最後まで読み、これらを製品仕様と0.6.0実装前Gateの正本として扱ってください。

重要:
現行0.5.0は、4 Speaker固定 + Stereo Spatial Renderer中心のプロトタイプです。
これは完成形ではありません。
以前の「4 Speaker固定を維持する」「SOURCE固定原点から全Speakerへ放射する」「8ch/Binauralは1.0以降」というロードマップは撤回されています。

Club Craftの中心は次です。

1. Virtual ClubへSpeakerを自由に追加・削除・配置する
2. SUB / WOOFER / FULL RANGE / MID / HIGH / CUSTOMを選択する
3. DAWの各SOURCEを任意のSpeakerへ直接ルーティングする
4. 1 SOURCEから何台のSpeakerへでも接続可能
5. FULL SIGNALとBAND routingを持つ
6. SpeakerのLevel・種類・台数の結果としてMasterのトーンが変わる「Reverse EQ」
7. RTAは編集EQではなく結果表示
8. ListenerをVirtual Club内で移動可能
9. Binaural / Stereo / 4ch / 8chを同じSceneから扱う
10. 4ch / 8chは完成版の中核で、1.0以降の任意機能ではない

UIは、グレー基調の箱庭・建築模型・Blender viewport的な立体Speaker表現。
Apple/macOSの余白感、Abletonの機能整理、Teenage Engineeringのプロダクト感を参考にするがコピーしない。
Neon、Cyberpunk、AI dashboard、SaaS dashboard、派手なparticle、うねうねしたcableは避ける。

既存コードで維持するもの:
- C++20 / JUCE
- single Club Craft.vst3 binary
- SOURCE / CLUB role
- SessionRegistry思想
- APVTS state
- realtime-safe audio thread
- macOS universal build
- Speaker Type基礎
- tests

最初の作業ではコードを書かないでください。

最初に以下を実施してください。

A. 現行mainを読み、現在のアーキテクチャを要約
B. 4 Speaker固定に依存しているファイル・struct・DSPを列挙
C. StereoSpatialRendererを完成形にしないための移行案
D. Dynamic Speaker Sceneの設計
E. Dynamic Route Sceneの設計
F. SOURCE → SpeakerのFull Signal routingのDSP構造
G. 将来のBand routing追加方法
H. Stereo/Binaural PreviewとDiscrete 4ch/8ch Outputの責務分離
I. realtime safetyを守るための固定容量snapshot案
J. state migration案
K. 0.6.0で変更する最小範囲
L. 変更予定ファイル一覧
M. test plan

推奨例として MAX_SPEAKERS=16 / MAX_ROUTES=128 の固定容量方式を検討してよいですが、最適な値は理由とともに提案してください。

この設計回答では、ARCHITECTURE_GATE_0_6.md のGate A〜Hを一つずつ満たす具体的な設計判断を表で示してください。この設計回答を出した時点で停止し、実装はまだ行わないでください。
```

## 設計承認後の0.6.0実装プロンプト

```text
ARCHITECTURE_GATE_0_6.md のGate A〜Hを満たす設計を確認し、0.6.0のアーキテクチャ設計を承認します。

ただし0.6.0ではUIを完成させるのではなく、本来のClub Craftへ戻す基盤変更に限定してください。

必須:
- 4 Speaker固定arrayを完成形前提から外す
- Dynamic Speaker Scene用の固定容量snapshotを導入
- Stable Speaker ID
- Dynamic Route Scene用の固定容量snapshotを導入
- Stable Route ID
- SOURCE→任意SpeakerへのFull Signal one-to-many routingを内部モデルとして成立させる
- Speaker Type / Levelを維持
- 現行SessionRegistryのrealtime-safe思想を維持
- processBlockでlock/allocation/I/Oしない
- state migrationを用意
- 現行Live Setを可能な限り壊さない
- 既存4 Speaker Sceneはlegacy/default sceneとしてmigration可能にする
- Stereo Previewは暫定互換として残してよいが、完成形APIとは分離する
- CL​​UBをControl Plane、SOURCEをAudio Workerとして実装する。Routingのauthoritative stateはCLUBにのみ保存する
- Route Contribution → Virtual Speaker Voice → Preview / Discrete Outputの3層を明示する。Virtual Speaker Voiceは線形DSPに限定し、saturation / limiter / distortion / nonlinear compressionは追加しない
- SOURCEのaudio threadは自分専用の最大16 RoutePlanだけを読む。Global Routeを全走査しない
- これは**0.6.0の履歴上限**として`MAX_SPEAKERS=16`、`MAX_SOURCES=128`、`MAX_ROUTES_GLOBAL=512`、`MAX_ROUTES_PER_SOURCE=16`を採用した。0.7.0以降の実装では、後半の「G. 0.7.0」節にある`MAX_ROUTES_GLOBAL=2048`とGate A〜Oを必ず優先する。
- Speaker slot + generationでstale Routeを無効にし、duplicate SOURCEは新IDへrekeyしてRouteを自動複製しない。duplicate CLUBはConflictとしてpublishしない
- Dynamic Sceneをauthoritative state、旧APVTSを最初の4 Speaker用compatibility bridgeとして扱う。legacy migration時だけ内部gain 0.25を使い、新Sceneを自動normalizationしない
- `OUTPUT_FEASIBILITY_SPIKE.md` を作成し、4ch / 8ch backendを決め打ちせずHost I/O実現可能性を記録する
- Band routing / Binaural / 4ch / 8ch本実装 / 新UIはまだ完成させない

作業:
1. 先に0.6.0設計文書をcommit対象として作成
2. 小さく実装
3. unit tests追加
4. VST3/AU build
5. CTest
6. state migration test
7. README / CHATGPT_HANDOFFを現状に合わせる
8. 変更内容と未実装事項を報告

各工程で、元の製品コンセプトを変更しないでください。
```


---

## G. 0.7.0 — Dynamic Speaker & Full Routing UI の最終実装プロンプト

```text
GitHubの https://github.com/earns7572-creator/clubcraft-phase0 の main ブランチを読み、以下を最初から最後まで確認してください。

- CHATGPT_HANDOFF.md
- PHASE7_PLAN.md
- ARCHITECTURE_GATE_0_7.md
- IMPLEMENTATION_REVIEW_0_6.md
- PHASE6_FOUNDATION.md

0.7.0の唯一の実装判断正本はARCHITECTURE_GATE_0_7.mdです。Gate A〜Oが最終レビューでGOとなるまで、コードを書いてはいけません。

0.7.0で実装するのは、最大16 Speakerの追加・削除・自由配置、Listener drag、SOURCE→SpeakerのFull + SUM_MONO Route create/delete/mute/gain、virtualized Routing Matrix、legacy materialisationです。SOURCEは固定Audio Workerであり、位置を動かしてはいけません。

必須制約:
- `MAX_SPEAKERS=16`、`MAX_SOURCES=128`、`MAX_ROUTES_GLOBAL=2048`、`MAX_ROUTES_PER_SOURCE=16`。
- DynamicSceneがauthoritative control state。旧APVTSは最初の4 legacy Speakerだけのcompatibility / host automation bridge。
- parameterChanged()はatomic pending value + dirty flagだけを更新する。mutex、vector、string、UUID、ValueTree、SceneCompiler、Registryを触らない。
- Timer / AsyncUpdaterのmessage threadでpending automationをStable ID `legacy-speaker-N`へ適用する。
- すべてのScene editはbaseSceneRevisionを持つcandidate transaction。stale candidateはretry / rebaseし、lost updateを起こさない。
- sceneEditMutexはcopy / swapだけ。compile、Registry publish、ValueTree、Host parameter通知中はmutexを保持しない。
- drag publishは最大30Hz、mouseUpは最終位置を即時publishする。
- Stable Route IDをpersistent routeSlot + routeGenerationへ対応させる。Renderer stateをRoutePlan配列indexに紐付けない。
- route enabled=falseとSpeaker muteは即skipしない。target gain=0へ10〜20ms rampし、fade完了後にsilentにする。Route deleteでPlanから消えたVoiceもretiring状態で10ms fade-outしてからDSP stateをreleaseする。
- Host state保存ではauthoritative Scene copyへpending automation mailboxのrevision付きsnapshotをoverlayしてserializeする。保存のためにauthoritative Sceneやmailboxを変更しない。
- legacy materialisationと最初のRoute操作は同一candidate transaction。Cancel / capacity failureではSceneを一切変更しない。
- SOURCE unregister時にpersisted RouteConfigを削除しない。offline rowとして表示する。duplicate SOURCEはnew/unrouted、duplicate CLUBはread-only。
- Band DSP、HRTF、完成版Binaural、4ch / 8ch本出力、RTAは実装しない。

作業順序:
1. Gate A〜Oの要件を満たすProcessor editing API、pending automation mailbox、revision transaction、route slot lifecycleの設計を提示し、承認を待つ。
2. 承認後にcontrol-side transactionと単体テストを実装する。
3. 次にsmoothingとdirty publishを実装・検証する。
4. その後Floor View、Speaker Inspector、virtualized Matrix、legacy materialisation dialogを小さく実装する。
5. Debug / Release CTest、VST3 / AU build、schema 7 migration、Intel Mac / Ableton Live試用を完了する。
6. 各段階でREADME、PHASE7_PLAN、CHATGPT_HANDOFFを現状へ同期する。

実装開始前に、上記を表で設計し、変更ファイル、Realtime境界、state migration、テストを説明してください。まだコードを書かず、私の承認を待ってください。
```
