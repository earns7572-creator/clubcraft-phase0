# Club Craft 0.7.0 — 最終GO判定レビュー依頼文

以下を、GitHubを読めるChatGPTへそのまま送る。

```text
Club Craft 0.7.0の最終Architecture Gateをレビューし、実装開始のGO / NO-GOだけを判定してください。コードを書かず、ファイル変更、commit、pushもしないでください。

Repository:
https://github.com/earns7572-creator/clubcraft-phase0
branch: main

必ず以下を最初から最後まで読んでください。

- CHATGPT_HANDOFF.md
- CHATGPT_PROMPTS.md（特に「G. 0.7.0」節）
- PHASE7_PLAN.md
- ARCHITECTURE_GATE_0_7.md
- PHASE6_FOUNDATION.md
- IMPLEMENTATION_REVIEW_0_6.md

今回のGateは前回レビューの残る4点に加え、pending automation保存とRoute削除fadeを解決するため、次を追加済みです。

1. parameterChanged()はatomic pending valueとdirty flagだけを更新し、DynamicScene、mutex、vector、string、ValueTree、Compiler、Registryを触らない。
2. Timer / AsyncUpdaterのmessage threadがpending automationをStable ID `legacy-speaker-N`へ適用する。
3. candidate SceneはbaseSceneRevisionでcompare-and-swapし、stale candidateをretry / rebaseしてlost updateを防ぐ。
4. Stable Route IDはpersistent routeSlot + routeGenerationへ対応し、Renderer stateはplan配列indexでなくRoute identityに紐付ける。
5. Route / Speaker muteは即skipでなくtarget gain=0への10〜20ms rampとする。
6. `MAX_ROUTES_GLOBAL=2048`、`MAX_SPEAKERS=16`、`MAX_SOURCES=128`、`MAX_ROUTES_PER_SOURCE=16`へ同期済み。
7. Host state保存はauthoritative Scene copyへpending automation mailboxのrevision付きsnapshotをoverlayしてserializeし、保存のためにauthoritative stateを変更しない。
8. Route deleteでPlanから消えたVoiceはretiring状態で10ms fade-outしてからDSP stateをreleaseする。
9. CHATGPT_HANDOFF.md、CHATGPT_PROMPTS.md、PHASE7_PLAN.md、READMEをGate A〜Oへ同期済み。

以下を確認してください。

| 確認項目 | 判定観点 |
|---|---|
| Realtime callback境界 | parameterChanged()がaudio callback文脈でも安全か。 |
| transaction整合性 | Scene revision、membership revision、legacy materialisationのrollbackがlost updateを防ぐか。 |
| Route lifecycle | Route slot / generation、reorder、mute ramp、future Band DSPへの拡張性が十分か。 |
| legacy互換 | Stable ID bridge、0.25 gain、offline route、late SOURCE、duplicate rekeyがSetを壊さないか。 |
| UI / UX | Floor View、virtualized Matrix、offline source、Conflict read-only、selection同期が妥当か。 |
| テスト | Gate Mまでのテストが実装開始に十分か。 |
| scope | Band DSP、HRTF、完成版Binaural、4ch / 8ch本出力、RTAが0.7へ混入していないか。 |

各項目を表でレビューし、最後に必ず以下を明記してください。

- 承認できる点
- 実装前に必ず修正すべき点
- 修正版0.7.0を実装開始してよいか（GO / NO-GO）

今回はレビューだけです。コード、ファイル、commit、pushは変更しないでください。
```
