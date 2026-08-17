# Club Craft Output Feasibility Spike — 0.6.x

## 目的

0.6.0では、Dynamic SpeakerとRouteの基盤を構築するが、最終的な4ch / 8ch出力backendは確定しない。この資料は、Stereo PreviewとDiscrete Speaker Feedを将来安全に分離するための、host I/O検証計画である。

> **このspikeの結論が出るまで、SOURCE audioを複数Plugin instance間で最終的に集約する方法を不可逆に決めない。**

## 検証対象

| Layout | 入力 | 出力 | 目的 |
|---|---|---|---|
| A | Stereo | Stereo | 現在のPreview互換。必ず維持する。 |
| B | Stereo | Quad 4ch | 4つのVirtual Speaker VoiceをDiscrete channelへ出せるか確認する。 |
| C | Stereo | Octa 8ch | 8つ以上のSpeaker assignmentを扱うI/Oの実現可能性を確認する。 |
| D | Stereo Main | Stereo Preview + Aux 8ch | 制作用Previewと作品用Discrete Feedを同時に持てるか確認する。 |

## 実機マトリクス

| Plugin format | Ableton Live | Logic Pro | Reaper | 判定 |
|---|---|---|---|---|
| VST3 / Layout A | 未検証 | n/a | 未検証 | 0.6.0では既存Stereo経路を使用する。 |
| VST3 / Layout B | 未検証 | n/a | 未検証 | 0.6.xで検証する。 |
| VST3 / Layout C | 未検証 | n/a | 未検証 | 0.6.xで検証する。 |
| VST3 / Layout D | 未検証 | n/a | 未検証 | 0.6.xで検証する。 |
| AU / Layout A | n/a | 未検証 | 未検証 | 0.6.0では既存Stereo経路を使用する。 |
| AU / Layout B | n/a | 未検証 | 未検証 | 0.6.xで検証する。 |
| AU / Layout C | n/a | 未検証 | 未検証 | 0.6.xで検証する。 |
| AU / Layout D | n/a | 未検証 | 未検証 | 0.6.xで検証する。 |

## 確認手順

1. 各Layoutを個別のspike branchまたは小さな試験targetで作る。
2. Plugin validation、host内のI/O表示、Track routing、offline renderを確認する。
3. SOURCE A/B/Cが同一Virtual Speakerへ寄与する場合、DAW側で正しく合算できるかを確認する。
4. Stereo / Binaural Previewを改善してもDiscrete Feedが変化しないことを確認する。
5. 結果を`ARCHITECTURE_DECISION_RECORD_OUTPUT_BACKEND.md`へ記録し、採用backendと不採用案の理由を残す。

## 0.6.0の固定事項

- 本plugin targetのMain I/OはStereo In / Stereo Outのままとする。
- QUAD / OCTAの本出力、HRTF、完成版Binauralは実装しない。
- `SourceRouteStereoRenderer`はPreview用の線形経路であり、将来のDiscrete Output Mapperの代替ではない。
- Speaker PositionとListener PositionはPreviewへ作用し、将来のDiscrete Speaker Feedそのものを変えてはいけない。

## 0.6.xの完了条件

各Host / formatの実測結果を表へ記入し、以下のいずれかをArchitecture Decision Recordとして承認する。

| 選択肢 | 判断基準 |
|---|---|
| Host-native multichannel | 主要DAWで必要なbus layoutとrenderが安定している。 |
| Separate aggregation backend | 複数SOURCEの同一Speaker Feedをplugin外または中心instanceで確実に集約できる。 |
| Hybrid | Stereo Previewは各SOURCE、Discrete Feedは専用CLUB / backendに集約する。 |
