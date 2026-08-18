# Club Craft 0.7.0 — Gate A〜R 最終GO判定レビュー依頼文

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

今回のGateは、以前のレビューで残ったcallback、2048 Route、pending automation保存、Route削除fade、snapshot interleave、retiring Voice容量境界に加え、mailboxのcommit前consume競合を解決するため、Gate A〜Rへ更新済みです。

1. parameterChanged()はatomic pending valueとdirty flagだけを更新し、DynamicScene、mutex、vector、string、ValueTree、Compiler、Registryを触らない。
2. Timer / AsyncUpdaterのmessage threadがpending automationをStable ID `legacy-speaker-N`へ適用する。
3. candidate SceneはbaseSceneRevisionでcompare-and-swapし、stale candidateをretry / rebaseしてlost updateを防ぐ。
4. Stable Route IDはpersistent routeSlot + routeGenerationへ対応し、Renderer stateはplan配列indexでなくRoute identityに紐付ける。
5. Route / Speaker muteは即skipでなくtarget gain=0への10〜20ms rampとする。
6. `MAX_ROUTES_GLOBAL=2048`、`MAX_SPEAKERS=16`、`MAX_SOURCES=128`、`MAX_ROUTES_PER_SOURCE=16`へ同期済み。
7. Host state保存はauthoritative Scene copyへpending automation mailboxのrevision / seqlock snapshotをoverlayしてserializeし、保存のためにauthoritative stateを変更しない。
8. Gate P: Scene copyの前後でcontrolSceneRevisionを読み、途中でTimerがcommitしてrevisionが変化したらsnapshot全体をretryする。Timerはpending mailboxを非破壊のrevision付きsnapshotとして読み、CAS commit成功後に一致するrevisionだけをack / consumeする。commit前、preflight failure、CAS retry時にdirty/value/revisionをclearしないため、「古いScene copy + clear済みmailbox」を保存するinterleaveを防ぐ。
9. Gate Q: SourceRouteStereoRendererはprepare時に、最大16 active Voiceと最大16 retiring Voice、合計32 Voiceの固定容量poolを事前確保する。16 Route同時削除直後に16新Routeができる最悪ケースでもaudio threadでallocation / waitしない。retiring Voiceとslot再利用後の新generation VoiceはDSP stateを共有しない。
10. Gate R: state snapshot interleave、Timer snapshot後・Scene commit前のHost state保存、mailbox一貫snapshot、16 retiring + 16 active、slot reuse、10ms完了後のretiring pool解放を必須テストへ追加済み。
11. CHATGPT_HANDOFF.md、CHATGPT_PROMPTS.md、PHASE7_PLAN.md、READMEをGate A〜R基準のロードマップへ同期済みであり、今回の最終技術正本はARCHITECTURE_GATE_0_7.mdのGate A〜Rである。

以下を確認してください。

| 確認項目 | 判定観点 |
|---|---|
| Realtime callback境界 | parameterChanged()がaudio callback文脈でも安全か。 |
| transaction整合性 | Scene revision、membership revision、legacy materialisation rollback、State snapshot retry、commit後revision ackがlost updateとautomation値消失を防ぐか。 |
| Route lifecycle | Route slot / generation、reorder、mute ramp、16 active + 16 retiringの固定容量pool、future Band DSPへの拡張性が十分か。 |
| legacy互換 | Stable ID bridge、0.25 gain、offline route、late SOURCE、duplicate rekeyがSetを壊さないか。 |
| UI / UX | Floor View、virtualized Matrix、offline source、Conflict read-only、selection同期が妥当か。 |
| テスト | Gate M / RのTimer snapshot後・commit前のState保存interleave、commit失敗時のmailbox保持、最大retirement境界テストが実装開始に十分か。 |
| scope | Band DSP、HRTF、完成版Binaural、4ch / 8ch本出力、RTAが0.7へ混入していないか。 |

各項目を表でレビューし、最後に必ず以下を明記してください。

- 承認できる点
- 実装前に必ず修正すべき点
- 修正版0.7.0を実装開始してよいか（GO / NO-GO）

今回はレビューだけです。コード、ファイル、commit、pushは変更しないでください。
```
