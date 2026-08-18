# Club Craft 0.7.0 — 修正版Architecture Gate再レビュー依頼文

以下を、GitHubを読めるChatGPTへそのまま送る。

```text
Club Craft 0.7.0の修正版Architecture Gateを再レビューしてください。今回はコードを書かず、ファイル変更、commit、pushも行わないでください。

Repository:
https://github.com/earns7572-creator/clubcraft-phase0
branch: main

最初に以下を最初から最後まで読んでください。

- CHATGPT_HANDOFF.md
- ARCHITECTURE_GATE_0_6.md
- PHASE6_FOUNDATION.md
- PHASE7_PLAN.md
- ARCHITECTURE_GATE_0_7.md
- IMPLEMENTATION_REVIEW_0_6.md

前回レビューでは0.7.0をNO-GOと判断しました。以下の6ブロッカーを修正版Gateで明示的に解決しています。

1. Legacy APVTS bridgeを、TimerがDynamicSceneを上書きする方式から、DynamicScene authoritative + legacy Stable ID bridgeへ反転した。
2. `sceneDirty`、`sourceMembershipDirty`、`urgentPublish`を導入し、drag中のcompile / publishを最大30Hz、mouseUpを即時publishにした。
3. `MAX_ROUTES_GLOBAL`を512から2048へ増やし、128 SOURCE × 16 Speaker Matrixと一致させた。
4. Speaker / Route編集をcandidate Scene → validate / preflight → successful swapのtransactionにした。scene mutexはcopy / swapだけに使い、compile / Registry publish中は保持しない。
5. legacy materialisationと最初のRoute操作を同一transactionにし、Cancel / capacity failureでrollbackする。
6. Route gain / mute / Speaker Level / position変化へ10〜20msのsmoothingを追加し、Realtime Route identityとしてnumeric routeToken / routeGenerationを導入する。

以下を確認してください。

1. 上記6点は前回指摘したブロッカーを実際に解消しているか。
2. Legacy APVTS automation、Speaker削除・追加、Stable ID mapping、feedback loop防止の設計は十分か。
3. candidate transactionとmutex範囲はRealtime安全か。deadlockやstate raceの穴はないか。
4. 2048 global route、16 routes per source、128 sourcesの容量設計は一貫しているか。
5. legacy materialisation、late SOURCE registration、SOURCE unregister / re-register、duplicate SOURCE rekeyの挙動は保存互換性を壊さないか。
6. gain / mute / position smoothingとrouteToken / routeGenerationの仕様は、将来のBand DSPにも使えるか。
7. custom drawing / Viewport virtualized Routing Matrix、offline source、duplicate CLUB read-only、双方向selectionのUXは妥当か。
8. Gate Hのテスト一覧に不足があるか。
9. 0.7.0に混入してはいけない機能（Band DSP、HRTF、4ch / 8ch本出力、RTA）が残っているか。

問題点、理由、具体的な改善案を表で回答してください。最後に次の3つを明記してください。

- 承認できる点
- 実装前に必ず修正すべき点
- 修正版0.7.0を実装開始してよいか（GO / NO-GO）

繰り返しますが、今回はレビューだけです。コード、ファイル、commit、pushは変更しないでください。
```
