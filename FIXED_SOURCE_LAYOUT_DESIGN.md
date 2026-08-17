# Club Craft — Fixed Source / Speaker & Listener Layout

この設計は、Phase 2の「SOURCEを動かす」考え方を置き換える。Club Craftでは、DAW Track上のSOURCEはクラブのステージ原点に固定される。ユーザーが操作するのは、CLUBが管理する各Speakerの位置と、観客（Listener）の位置である。

> **音源を動かすのではなく、クラブの音響設備と観客の立ち位置を動かす。**

## 座標系

X軸は左（負）から右（正）、Y軸は後方（負）から前方（正）であり、単位は会場内の相対メートルとする。すべてのSOURCEは固定位置 `(0, 0)` から再生される。Listenerは初期値 `(0, 0)` だが、CLUBで移動できる。

| 操作対象 | 操作できる値 | 初期位置 | 誰が操作するか |
|---|---|---|---|
| SOURCE | 位置操作なし | 固定 `(0, 0)` | 操作不可 |
| FRONT L | X / Y | `(-6, +6)` | CLUB |
| FRONT R | X / Y | `(+6, +6)` | CLUB |
| REAR L | X / Y | `(-6, -6)` | CLUB |
| REAR R | X / Y | `(+6, -6)` | CLUB |
| LISTENER / AUDIENCE | X / Y | `(0, 0)` | CLUB |

## 信号経路

SOURCEごとに同じFull Signalを固定原点から発生させ、Speakerごとに次の経路を計算する。

1. 固定SOURCEからSpeakerまでの距離を求める。
2. Speakerから可動Listenerまでの距離を求める。
3. 2つを合計し、相対的なLevel・高域減衰・到達delayを求める。
4. **SpeakerとListenerの相対X位置をSpeaker→Listener距離で割る**ことでpanを決める。
5. Speaker Type CharacterとGeneric Responseを通し、Stereo L/Rへ合算する。

このpan設計では、Listenerから見てSpeakerが真横に近いほど左右へ強く寄り、正面・背後に近いほど中央へ寄る。旧来のSpeaker Spread正規化とは異なり、Speakerの位置やListener位置を変えると、pan・距離・delay・toneの少なくとも一部が変わる。

## FRONT / REARについて

この段階の出力は通常のステレオL/Rである。したがって、FRONT / REARがヘッドホン上で完全に前後へ定位するHRTF処理はまだ含まれない。ただし、SpeakerまたはListenerを動かすことで、前後経路の距離・delay・高域減衰が変わる。これにより、旧Phase 2のように完全対称な計算になって変化が消える問題を避けられる。

将来はこの固定SOURCE / Speaker / Listener座標モデルを基礎に、2Dフロアビュー、初期反射、HRTFまたはクロスフィードを追加する。これらは前後感をさらに明確にする改善であり、座標の責任分担は変更しない。

## Realtime Safety

CLUBはTimerからSpeaker X/Y、Listener X/Y、Type、LevelをSceneへ公開する。共有値はprimitive atomicsとseqlockで整合性を保つ。SOURCEのaudio callbackは固定SOURCE座標と公開済みSceneを読み、prepare済みのdelay lineとfilterだけを使う。audio callback内でlock、メモリ確保、コンテナ変更、待機は行わない。
