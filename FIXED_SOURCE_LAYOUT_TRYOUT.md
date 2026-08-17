# Club Craft 0.5.0 — Speaker & Listener Layout 試用手順

この更新では、SOURCEの位置操作を廃止しました。SOURCEはステージ原点に固定されます。CLUBで操作するのは、4本のSpeakerの個別X/Y位置と、`LISTENER / AUDIENCE POSITION` のX/Yです。

## 1. 更新後に最初に確認すること

CLUBインスタンスを開き、次の表示があることを確認します。

| 表示 | 意味 |
|---|---|
| `LISTENER / AUDIENCE POSITION` | 観客の聴く位置を操作する領域。 |
| `INDIVIDUAL SPEAKER PLACEMENT & VOICES` | SpeakerごとのType、Level、X、Yを操作する領域。 |
| `SOURCE` 画面に位置スライダーがない | SOURCEが固定されたことを示す。 |

## 2. 最初の比較方法

最初はSpeaker Typeを4本すべて `FULL RANGE`、Speaker Levelをすべて `0 dB`、Listenerを `X = 0 / Y = 0` にします。この状態を基準にします。

次の表のように、**一度に1つだけ**変えてください。

| 確認したいこと | 操作 | 期待する差 |
|---|---|---|
| 左右位置 | `FRONT L` のXを `-12 m`、`FRONT R` のXを `+2 m` にする | 左右のpanとSpeakerごとの距離が非対称になり、左の寄与が強くなる。 |
| 観客を左へ移動 | `LISTENER X` を `-5 m` にする | 左Speakerに近づき、左右Speakerの距離・pan・delay差が変わる。 |
| 観客を前へ移動 | `LISTENER Y` を `+5 m` にする | 前方Speakerへの経路が短くなり、後方Speakerとの距離・tone・delay差が変わる。 |
| 後方Speakerを離す | `REAR L / REAR R` のYを `-12 m` にする | Rearの経路が長くなり、相対delayと高域減衰が増える。 |

完全な左右対称のままListenerを中央に置くと、左右差は小さくなります。最初は非対称な配置で確認するのが重要です。

## 3. FRONT / REARの正しい理解

本バージョンはStereo L/R出力であるため、Rear Speakerがヘッドホン上の「完全な真後ろ」から聞こえるHRTF処理ではありません。前後配置は、経路距離に基づくLevel・高域・到達タイミングの差として反映されます。

FRONTとREARの違いを聞き取りやすくするには、Listenerを少し前または後ろへ動かし、Rear SpeakerのYだけを大きく変えてください。将来の初期反射やHRTFは、この座標モデルを保ったまま前後感を強化する改善です。

## 4. Intel Macでの更新

Phase 3と同じく、Liveを終了してからTerminalで実行します。

```bash
cd ~/Developer/ClubCraftPhase0
git pull origin main
./build-release.sh
```

成功時のReleaseフォルダは次です。

```text
~/Developer/ClubCraftPhase0/dist/ClubCraft-0.5.0-macos-universal
```

VST3の入れ替えでは、必ずコピー元があることを確認してから、既存VST3を日時付きでバックアップしてください。

```bash
PLUGIN_DIR="$HOME/Library/Audio/Plug-Ins/VST3"; SOURCE="$HOME/Developer/ClubCraftPhase0/dist/ClubCraft-0.5.0-macos-universal/Club Craft.vst3"; STAMP="$(date +%Y%m%d-%H%M%S)"; test -d "$SOURCE" || exit 1; [ ! -d "$PLUGIN_DIR/Club Craft.vst3" ] || mv "$PLUGIN_DIR/Club Craft.vst3" "$PLUGIN_DIR/Club Craft-before-fixed-layout-$STAMP.vst3"; cp -R "$SOURCE" "$PLUGIN_DIR/Club Craft.vst3"; codesign --force --deep --sign - "$PLUGIN_DIR/Club Craft.vst3"; echo "Fixed Source / Speaker & Listener Layoutを配置しました"
```

Liveを完全に再起動し、空のLive SetへBrowserの`Club Craft`本体を新規挿入して確認してください。`.vstpreset` は設定ファイルであり、プラグイン本体のバージョン確認には使いません。
