# Club Craft Phase 2 — Intel Mac / Ableton Live 試用手順

Phase 2は、Phase 1の4 Speaker Levelに、SOURCE位置・Speaker配置・距離減衰・距離依存Response・相対到達delay・constant-power stereo panを追加します。

## 1. Releaseを作成する

Phase 2を取得したプロジェクトの直下で実行します。

```bash
./build-release.sh
```

成功時のRelease folderは次の形式です。

```text
dist/ClubCraft-0.3.0-macos-universal
```

このフォルダの `Club Craft.vst3` を、Phase 1と同じ安全なバックアップ手順で `~/Library/Audio/Plug-Ins/VST3/` へ配置します。Liveを完全終了してから配置し、配置後にLiveを再起動してください。

## 2. Live Setを構成する

1. Kick、Bass、Hatsなど各音源にClub Craftを挿し、ROLEを **SOURCE** にします。
2. 音源を1つのGroupへまとめます。
3. GroupへClub Craftを挿し、ROLEを **CLUB** にします。
4. SOURCEには `Connected / distance, delay & stereo pan active` と表示されます。
5. CL​​UBには `CLUB / publishes 4-speaker spatial scene` と表示されます。

## 3. Source位置を確認する

SOURCE側の `SOURCE POSITION` を操作します。

| 操作 | 期待される変化 |
|---|---|
| `SOURCE X` を負（左）にする | 左側Speakerの経路が強くなり、ステレオ出力が左へ寄る。 |
| `SOURCE X` を正（右）にする | 右側Speakerの経路が強くなり、ステレオ出力が右へ寄る。 |
| `SOURCE Y` を正（FRONT）にする | 前方Speakerとの距離関係が変わる。 |
| `SOURCE Y` を負（REAR）にする | 後方Speakerとの距離関係が変わる。 |

SOURCE位置は**SOURCEインスタンスごと**に保存されます。たとえばKickを中央、Hatsを右後方、Vocalを左前方に置けます。

## 4. Speaker Layoutを確認する

CLUB側の `SPEAKER LAYOUT` を操作します。

| 操作 | 期待される変化 |
|---|---|
| `SPEAKER SPREAD` を大きくする | 左右Speakerが離れ、左右パンの差が大きくなる。 |
| `SPEAKER DEPTH` を大きくする | FRONT / REARの前後距離が伸び、相対delayと距離差が大きくなる。 |
| Speaker Level | そのSpeaker経路の寄与だけを調整する。 |
| GENERIC RESPONSE | 全Speakerの基礎的な高域Responseを調整する。 |

## 5. 聞こえ方の確認

Phase 2はヘッドホンHRTFではなく、4 Speaker経路をステレオへ合成するシミュレーションです。次の3点を確認してください。

1. SOURCE Xを左右へ動かすと、左右バランスが変化する。
2. SOURCEを遠い位置へ置くと、音量と高域が少し下がる。
3. SPEAKER DEPTHを大きくすると、前後Speakerのdelay差が増える。

最後にLive Setを保存し、Liveを再起動します。SOURCE X/Y、Speaker Spread/Depth、Speaker Level、Responseが復元すればPhase 2の状態保存も成功です。

## Phase 2に含まれないもの

この段階では、個人化HRTF、耳介効果、部屋の壁反射、残響、遮蔽、測定済みSpeakerモデルは未実装です。したがって、Rearを下げたときに「頭の真後ろ」まで精密に聞こえることは期待しないでください。Phase 2は空間処理の土台となる位置・距離・パン・delayを安定して動かす段階です。
