# Club Craft Phase 2 — Planar Spatialisation Design

Phase 2は、Phase 1の4本のFull Signal Speakerを、平面上の位置を持つ仮想Speakerとして扱う。対象出力は引き続きDAWのステレオbusであり、HRTF、畳み込み残響、壁反射はこの段階の対象外とする。

## 座標系

> Listenerは `(0, 0)` に固定する。X軸は左（負）から右（正）、Y軸は後方（負）から前方（正）である。単位は会場内の相対メートルである。

| Entity | Phase 2の操作 | 初期位置 |
|---|---|---|
| SOURCE | `SOURCE X`、`SOURCE Y` | `(0, 0)` |
| FRONT L | `SPEAKER SPREAD` と `SPEAKER DEPTH` から算出 | `(-6, +6)` |
| FRONT R | `SPEAKER SPREAD` と `SPEAKER DEPTH` から算出 | `(+6, +6)` |
| REAR L | `SPEAKER SPREAD` と `SPEAKER DEPTH` から算出 | `(-6, -6)` |
| REAR R | `SPEAKER SPREAD` と `SPEAKER DEPTH` から算出 | `(+6, -6)` |

## SOURCEからステレオ出力まで

各SOURCEは入力ステレオをmonoのFull Signalへ集約し、4本のSpeakerごとに以下を独立して実行する。

1. **Source→Speaker距離**と**Speaker→Listener距離**を合成した経路長を求める。
2. 経路長から相対的な距離減衰を求める。初期位置では0 dBとなるよう参照経路長で校正する。
3. 経路長の差を最大60 msの相対delayへ変換する。最短経路を0 msとして、プラグインの追加latencyを発生させない。
4. 距離が長いほどGeneric Response Toneを下げ、Speakerごとのlow-passを暗くする。
5. SpeakerのX座標からconstant-power stereo panを計算する。左SpeakerはL、右SpeakerはRへ寄る。
6. 4本をStereo L/Rへ加算し、Speaker Levelの合計を4で正規化する。

## Realtime Safety

SceneはCLUBのTimerからprimitive atomic値として公開する。SOURCEのaudio callbackは、公開済みSceneと自分のAPVTS座標を読み、prepareToPlayで確保済みのmono scratch buffer、delay line、filterだけを使用する。processBlock内でロック、メモリ確保、コンテナ変更、待機を行わない。

## 意図的な制約

Phase 2はSpeakerの位置に応じた**ステレオシミュレーション**であり、ヘッドホン用の個人化HRTFではない。Rear Speakerは後方に置かれるが、ステレオ出力上では左右パン、距離、delay、filterの差として表現される。HRTF、部屋の反射、遮蔽、測定済みSpeakerモデルは将来のPhaseで扱う。
