# Club Craft Phase 1 — 実装設計

Phase 1は、Phase 0で検証済みの同一VST3モジュール内のSOURCE／CLUB共有状態を、**4本のGeneric Speaker**を持つ最小会場モデルへ拡張する。音声バッファをプラグイン間で転送せず、CLUBが公開する不変の制御状態を各SOURCEが読む設計は維持する。

## Scope

| 項目 | Phase 1の実装 |
|---|---|
| Speaker数 | 固定4本。FRONT L、FRONT R、REAR L、REAR R。 |
| SOURCEの経路 | 各SOURCEは入力ステレオのFull Signalを全アクティブSpeakerへ分配し、ゲイン正規化した合計を出力する。 |
| Speaker Level | 各Speakerの個別レベルをCLUB側で `-60 dB`〜`+12 dB` に設定する。 |
| Speaker Response | Generic low-pass response。各Speakerへ同一のResponse Toneを適用し、高域の明るさを段階的に調整する。 |
| Master | CLUB自身の出力だけにMaster Levelを適用する。 |
| UI | CLUBは4本のSpeaker LevelとResponse Tone、SOURCEは接続状態を表示する。 |

## Full Signal Routeの定義

Phase 1のFull Signalは、各SOURCE入力を複数Speakerへ複製して分配する意味である。SOURCEインスタンスは、CLUBが公開した各Speakerの線形ゲインを合計し、**アクティブSpeaker数で正規化**した係数を入力へ適用する。これにより、4本のSpeakerが0 dBの場合、SOURCEの通過ゲインは0 dBを保つ。

> `output = input × (Σ speakerGain × genericResponseGain) / activeSpeakerCount`

Speakerを個別に下げれば、そのSpeakerの寄与だけが減る。Response Toneは0〜100%で、全SpeakerのGeneric Response cutoffを約1.2 kHz〜18 kHzへ補間する。Phase 1は物理的な方向・距離・HRTFを実装せず、複数Speakerを制御・保存・レンダリングするための基盤に限定する。

## Realtime Safety

CLUBは非リアルタイムのタイマー経由でSceneを公開する。SOURCEのaudio callbackは、プリミティブatomic値だけを読み、ロック・メモリ確保・スレッド待機を行わない。Speaker係数はseqlock形式で一貫して読まれ、取得競合時は安全な未接続扱いへ戻る。

## State Compatibility

Session ID、Source ID、Role、Master Level、Response Tone、4つのSpeaker Levelは既存のAPVTS stateに保存する。State schemaは2から3へ更新する。Phase 0の既存Sessionを開いた場合、存在しないSpeaker 2〜4パラメータは0 dBの初期値で補完される。
