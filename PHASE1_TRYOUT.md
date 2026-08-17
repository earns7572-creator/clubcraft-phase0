# Club Craft Phase 1 — Intel Mac / Ableton Live 試用手順

Phase 1は、Phase 0で確認済みのSOURCE／CLUB接続を4本のGeneric Speakerへ拡張します。SOURCEは各Speakerへ**Full Signal**を分配し、CLUBは各Speaker LevelとGeneric Responseを公開します。

## 1. 最新コードを取得してReleaseを作る

GitHubリポジトリをclone済みのIntel Macで、プロジェクト直下から実行します。

```bash
git pull --ff-only origin main
./build-release.sh
```

Release生成後、VST3をローカルのLive用フォルダへ上書きします。

```bash
SOURCE="$PWD/dist/ClubCraft-0.2.0-macos-universal/Club Craft.vst3"
DEST="$HOME/Library/Audio/Plug-Ins/VST3/Club Craft.vst3"
rm -rf "$DEST"
cp -R "$SOURCE" "$DEST"
codesign --force --deep --sign - "$DEST"
```

Ableton Liveを完全終了してから再起動してください。

## 2. Live Setを構成する

1. Kick、Bass、Synthなど音が出る各トラックへ `Club Craft` を挿し、ROLEを **SOURCE** にします。
2. それらをGroupへまとめます。
3. Groupトラックへ `Club Craft` を挿し、ROLEを **CLUB** にします。
4. SOURCE側に `Connected / Full Signal to 4 speakers`、CLUB側に `CLUB / 4-speaker scene active` と出れば接続成功です。

## 3. Speaker Levelを確認する

CLUBには以下の4本のレベル操作があります。

| 操作 | 意味 |
|---|---|
| FRONT L | 前方左SpeakerのFull Signal Level |
| FRONT R | 前方右SpeakerのFull Signal Level |
| REAR L | 後方左SpeakerのFull Signal Level |
| REAR R | 後方右SpeakerのFull Signal Level |

全Speakerが **0 dB** の場合、Full Signalの正規化によりSOURCE出力は0 dBを維持します。たとえば `REAR R` を `-60 dB` へ下げると、そのSpeakerの寄与が外れ、SOURCEの合成レベルが下がります。4つのLevelを変え、複数SOURCEが同時に反応することを確認してください。

## 4. Generic Speaker Responseを確認する

`GENERIC RESPONSE` は全Speakerの簡易的な高域応答です。

| 設定 | 期待される音 |
|---|---|
| 100% | 約18 kHzの低域通過。最も明るい設定。 |
| 50% | 中程度に高域を丸める。 |
| 0% | 約1.2 kHzの低域通過。明確に暗いSpeaker Response。 |

Phase 1のResponseは特定モデルの測定値ではなく、複数Speaker制御・状態保存・Realtime DSP経路を検証するためのGenericモデルです。

## 5. 保存・再読み込み

Live Setを保存し、Liveを終了して開き直します。ROLE、Master、Generic Response、4つのSpeaker Levelが復元すればPhase 1の状態保存は成功です。

## 6. フィードバックしてほしいこと

1. 4本のSpeaker Level操作が正しく表示されたか。
2. いずれか1本のLevelを変えると、全SOURCEが同時に変化したか。
3. `GENERIC RESPONSE` を下げると高域が暗くなったか。
4. Set再読み込み後に全設定が復元したか。
5. スキャン失敗、クラッシュ、音切れ、CPU異常がなかったか。
