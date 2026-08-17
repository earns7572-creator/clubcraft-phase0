# Club Craft Phase 3 — Speaker Voices Design

Phase 3では、Phase 2の4本の位置付きFull Signal Speakerに、**Speaker Type**という音楽的な役割を追加する。直接EQの帯域を操作するのではなく、仮想Speakerの種類と出力Levelを選ぶことで最終的なトーンバランスを作る。

## Speaker Type

| Type | Characterの目安 | 用途の考え方 |
|---|---|---|
| `SUB` | 28 Hz high-pass、110 Hz low-pass | 重心・床の揺れ・最も低い帯域を作る。 |
| `WOOFER` | 55 Hz high-pass、420 Hz low-pass | KickやBassの胴鳴り、低中域の厚みを作る。 |
| `FULL RANGE` | Type固有filterなし | Phase 2の音を保つ基準Speaker。 |
| `MID` | 220 Hz high-pass、4.6 kHz low-pass | Vocal、Synth、Snareの存在感を作る。 |
| `HIGH` | 2.4 kHz high-pass | Hats、air、輪郭を作る。 |

これらは特定の市販Speakerを測定・再現したモデルではない。安全で広いcrossover風のCharacterに限定し、実際のスピーカーユニットや会場の物理特性を主張しない。

## Signal Flow

SOURCE側では、入力をmonoのFull Signalへ集約し、Speakerごとに次の順で処理する。

1. Phase 2と同じSource→Speaker→Listenerの経路長から、距離減衰・相対delay・stereo panを計算する。
2. CLUBが公開したSpeaker TypeのCharacterを適用する。
3. 距離に応じたGeneric Responseのlow-passを適用する。
4. Speaker Level、距離減衰、panを適用してStereo L/Rへ合算する。

Type固有Characterの後にもGeneric Responseを残すため、遠いHIGH Speakerが近いHIGH Speakerより暗くなるというPhase 2の空間的な挙動を維持できる。

## 互換性と状態

Speaker TypeはCLUBのAPVTSパラメータ `speakerType1` から `speakerType4` として保存される。初期値はすべて `FULL RANGE` とする。これは既存のPhase 1 / Phase 2 Live SetをPhase 3で開いた場合、音が不必要に帯域分割されないようにするためである。

CLUBのTimerはSpeaker Typeをprimitive atomic `int` として既存のseqlock Sceneへ公開する。SOURCEのaudio callbackは、公開済みSceneを読み、prepare済みのfilterとdelay lineだけで処理する。audio callbackでlock、メモリ確保、コンテナ変更、待機は行わない。

## 意図的な制約

Phase 3はまだlinear-phase crossover、Speaker測定データ、マルチバンドdynamics、resonance、飽和、部屋の反射を導入しない。まずは「Speakerの役割を選び、Levelと位置で音色を作る」操作を分かりやすく、安定して成立させることを目的とする。
