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
- Band routing / Binaural / 4ch / 8ch / 新UIはまだ完成させない

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
