# Club Craft 0.7.0 — ChatGPT設計レビュー依頼文

以下を、GitHubを読めるChatGPTへそのまま送る。

```text
Club Craft 0.7.0の設計レビューだけをしてください。コードを書かず、GitHubへのcommitやpushもしないでください。

Repository:
https://github.com/earns7572-creator/clubcraft-phase0
branch: main

最初に、以下を最初から最後まで読んでください。

- CHATGPT_HANDOFF.md
- ARCHITECTURE_GATE_0_6.md
- IMPLEMENTATION_REVIEW_0_6.md
- PHASE6_FOUNDATION.md
- PHASE7_PLAN.md

現在の0.6.0では、CLUBがDynamicSceneを所有し、SceneCompilerが固定容量RealtimeSceneSnapshotとSOURCEごとのSourceRoutePlanを作ります。SOURCEは音源workerであり、SOURCEの位置を操作してはいけません。SpeakerとListenerだけをCLUBで操作します。

0.7.0では、最大16 Speakerの追加・削除・自由配置、Listenerの2D配置、SOURCE→SpeakerのFull Route追加・削除・mute・gainを操作するFloor ViewとRouting Matrixを実装する予定です。

以下を厳しくレビューしてください。

1. Floor ViewにおけるSpeaker選択、追加、削除、ドラッグ、ListenerドラッグのUXは初心者に理解できるか。
2. DynamicSceneの編集をProcessor API + sceneEditMutex + Timer compile/publishに分ける設計は、JUCE / DAW hostでRealtime安全か。
3. Speaker削除と関連Route削除を1 transactionにする要件は十分か。
4. legacyDefaultRoutingから明示Routeへmaterialiseする確認UX、0.25 gain、rollback要件は十分か。
5. Routing Matrixで最大128 SOURCE × 16 Speakerを表示する場合の仮想化、scroll、検索、選択UXはどうすべきか。
6. 0.7.0ではFULL RouteだけをUI作成対象にし、将来FULL + BAND共存を壊さないRoute identity設計になっているか。
7. Route gainをlinear保存・dB表示にする設計の注意点は何か。
8. SOURCE register / unregister、duplicate SOURCE rekey、duplicate CLUB conflictをUI上でどう扱うべきか。
9. 0.7.0で実装してはいけない機能が混入していないか。
10. テスト計画に不足があるか。特にstate restore、slot generation、no route、last route deletion、legacy migration、drag中のpublish頻度を確認する。

各項目について、問題点、理由、具体的な改善案を表で回答してください。最後に、次の3つを明記してください。

- 承認できる点
- 実装前に必ず修正すべき点
- 0.7.0を実装開始してよいか

繰り返しますが、今回はレビューのみです。コード、ファイル、commit、pushは変更しないでください。
```
