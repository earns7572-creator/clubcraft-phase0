# Club Craft — ChatGPTへ貼り付けるプロンプト集

このファイルは、ChatGPTへClub Craftの修正と完成版までの開発を引き継ぐためのコピー用文章です。

> **重要:** ChatGPTの通常チャットは、private GitHub repositoryやMac内のフォルダを自動で読めないことがあります。その場合は、リポジトリのZIP、または最低限 `CHATGPT_HANDOFF.md`、`CMakeLists.txt`、`src/`、`tests/`、`build-release.sh` をアップロードしてください。ChatGPTがGitHub connector、Codex、または編集できる開発環境へ接続されている場合は、repository URLとbranchを渡してください。

---

## A. 最初に必ず貼る開始プロンプト

次の枠内をChatGPTへ貼り付け、`CHATGPT_HANDOFF.md` を一緒に添付する。

```text
あなたはC++20 / JUCE 9.0.1のDAWプラグイン開発者です。Club CraftというVST3/AUプラグインを、完成版1.0まで段階的に開発してください。

まず、添付した CHATGPT_HANDOFF.md を最初から最後まで読み、仕様・制約・ロードマップを守ってください。現在の基準はGitHub mainのcommit 6eaa05e、version 0.5.0です。

絶対条件:
1. SOURCEは固定ステージ原点です。SOURCE位置操作を復活させません。
2. CLUBだけが、個別SpeakerのX/Y位置、Speaker Type、Speaker Level、Listener / AudienceのX/Y位置を管理します。
3. SOURCEとCLUBを別プラグインに分けません。単一の Club Craft.vst3 内でROLEを切り替えます。
4. processBlockでlock、allocation、I/O、wait、std::atomic<std::shared_ptr>を使用しません。primitive atomics + seqlockの既存設計を維持します。
5. 初心者でも操作の意味が分かる、暖色グレー基調でminimal、tactile、非ゲーミングなUIにします。
6. 従来EQを主操作にせず、Speaker / Listener操作を製品の中心にします。
7. 一度に全ロードマップを実装しません。各Phaseを個別に設計・実装・テスト・文書化・commitしてから次へ進みます。
8. 実機検証していないことを、検証済みと表現しません。

最初に行うこと:
- repositoryのbranch、最新commit、git status、CMake version、主要ファイルを確認してください。
- CHATGPT_HANDOFF.mdの「現在の完了事項と未解決事項」と「完成版までの必須ロードマップ」を要約してください。
- 次に実装すべきPhase 4（2D Club Floor View）の具体的な設計を、変更対象ファイル、追加parameter、状態保存、Realtime安全性、テスト計画、完了条件の表で提示してください。
- この時点ではコードを書かず、私の承認を待ってください。
```

---

## B. Phase 4 — 2D Club Floor Viewを開始する依頼文

開始プロンプトへの設計回答を確認し、問題なければ次を貼る。

```text
Phase 4の設計を承認します。2D Club Floor Viewを実装してください。

目的は、数値sliderだけでは分かりにくいClubの操作を、平面図で分かるようにすることです。

必須要件:
- CLUB UIに2Dフロアビューを追加します。
- 固定SOURCEをステージ原点に表示しますが、ドラッグ不可です。
- FRONT L / FRONT R / REAR L / REAR R とLISTENERを表示します。
- SpeakerとListenerだけをドラッグで動かせます。
- ドラッグ操作は既存のAPVTS X/Y parameterを更新します。独自の別stateを作りません。
- sliderとfloor viewは双方向に同期します。
- CLUB role以外では位置編集を無効にします。
- state save/reload、DAW automation、window resizeで壊れないようにします。
- 既存DSP、single-binary構造、Realtime安全性を壊しません。

作業手順:
1. 先に PHASE4_DESIGN.md を作成してください。
2. 実装してください。
3. 位置parameterとstate restoreの自動テストを追加してください。
4. CTestとVST3 buildを実行してください。
5. READMEとIntel Mac試用手順を更新してください。
6. 変更ファイル、test結果、未実装の事項を報告して、commit前に私の確認を待ってください。
```

---

## C. Phase 5 — Spatial Feedback / A-B / Soloを開始する依頼文

Phase 4を実機で試し、GitHubへcommit済みにしてから使う。

```text
次はPhase 5を開始してください。目的は、SpeakerとListenerを動かした結果を、耳だけでなく表示でも理解できるようにすることです。

必須要件:
- Speakerごとの寄与メーターを追加してください。
- 各Speakerについて、control thread側で経路距離、relative delay、pan、toneを表示できるPath Inspectorを設計してください。
- SpeakerごとのSolo / Muteを、state saveとhost automationに対応する形で追加してください。
- A/B比較またはglobal bypassを安全に設計してください。
- audio threadからUIへ渡す値はatomic meter値などに限定し、audio threadにlockやallocationを追加しないでください。
- Speaker / Listenerが主役のUIを守り、通常EQ中心の画面に戻さないでください。

先に設計資料を作り、parameter一覧、Realtime安全性、state migration、テスト計画、UI案を提示して承認を待ってください。承認後に実装・build・test・資料更新を行ってください。
```

---

## D. Phase 6 — FRONT / REAR知覚改善を開始する依頼文

```text
次はPhase 6を開始してください。目的は、固定SOURCE / Speaker / Listenerモデルを維持したまま、FRONTとREARの差をより分かりやすくすることです。

前提:
- 現行はStereo L/Rへの空間シミュレーションであり、完全な後方HRTFではありません。
- HRTFを魔法のように約束しません。
- まず、少数の初期反射、Speaker directivity、控えめなstereo crossfeedなど、CPUとlatencyを抑えた手法を比較してください。

最初に、少なくとも3つのDSP案を比較表で提示してください。各案について、聴感上の期待、CPU、latency、Realtime安全性、実装難易度、Stereo出力での限界を説明してください。

私が案を選んだ後だけ、実装してください。実装時は、44.1 / 48 / 96 kHzと異なるblock sizeでtestし、click、denormal、過大なtail、CPU増加がないことを確認してください。
```

---

## E. Phase 7 — 制作Workflowを開始する依頼文

```text
次はPhase 7を開始してください。目的はClub Craftを実際のミックス作業で継続利用できるようにすることです。

実装候補:
- Venue template: small club、wide room、front-heavy、rear-heavy。
- Speaker Type template: Sub / Woofer / Full Range / Mid / Highの推奨配置。
- 入出力meter、clip warning、safe output trim。
- 必要最小限のsoft clipまたはlimiter。ただしデフォルトで音を不必要に変えない。
- Session IDの可視化・安全な編集・複数Club Sceneの混線防止。
- 旧stateのmigrationと、既存Live Setを壊さない読み込み。

先に、機能の優先順位、UI案、state migration方針、DAW automation方針、テスト計画を設計資料にしてください。Speaker / Listenerという中心概念が埋もれないことを最優先にしてください。
```

---

## F. Phase 8 — 1.0.0品質保証とReleaseを開始する依頼文

```text
次は1.0.0向けの品質保証とRelease準備を開始してください。新しい大きな機能は追加せず、安定性・互換性・配布品質を優先してください。

必須検証:
- sample rate: 44.1 / 48 / 88.2 / 96 kHz。
- block size: 32〜2048。
- SOURCE / CLUBの作成順序、削除、role変更、session save/reload。
- parameter automation、offline render、silence、reset、denormal。
- Ableton Live Intel / Apple Silicon、Logic Pro AU、Reaperなどの検証計画。
- VST3/AU Universal Binary、moduleinfo version、dist manifest。
- macOS code signing / notarizationを環境変数で任意に実行する既存build-release.shの維持。
- 初心者向けQuick Start、troubleshooting、Mac更新手順、既知の制約の文書化。

実機確認できない環境では、確認済みと書かず、ユーザーが実行できるチェックリストにしてください。テスト結果、残るリスク、Release candidateの場所を明確に報告してください。
```

---

## G. 各Phase終了時の共通依頼文

各PhaseでChatGPTが実装し終えた後、次を貼る。

```text
このPhaseの最終レビューをしてください。

1. git diffとgit statusを確認してください。
2. processBlockのRealtime安全性を、変更箇所ごとに確認してください。
3. 新旧stateの保存・復元に問題がないか確認してください。
4. CTestとVST3 buildを実行してください。
5. README、設計資料、Intel Mac試用手順を更新してください。
6. ユーザーに説明すべき「できること」「まだできないこと」「Macでの聞き比べ方法」を初心者向けにまとめてください。
7. commit前に変更一覧とcommit message候補を提示してください。

私が承認したら、1つの意味のあるcommitを作り、mainへpushしてください。
```

---

## H. ChatGPTが避けるべき回答・実装

```text
次のことはしないでください。

- SOURCEを移動できるUIやparameterを復活させる。
- SOURCEとCLUBを別VST3バイナリに分離する。
- processBlockにmutex、allocation、I/O、sleep、ログ出力、atomic shared_ptrを入れる。
- Speaker / Listenerの操作を置き換える通常EQ中心のUIを作る。
- HRTFや3D定位を実装していないのに、完全な立体音響だと説明する。
- build/testなしでcommitやpushする。
- Mac実機で試していないのに、Mac互換性を断定する。
- コメント行を含む長いzshスクリプトを初心者へ渡す。短いコピー可能な単位にする。
```
