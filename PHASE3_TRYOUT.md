# Club Craft Phase 3 — Intel Mac / Ableton Live 試用手順

Phase 3は、Phase 2の位置・距離・delay・stereo panに、Speakerごとの `SUB`、`WOOFER`、`FULL RANGE`、`MID`、`HIGH` を追加します。

## 1. Mac上のプロジェクトを更新する

Terminalを開き、プロジェクトフォルダへ移動します。

```bash
cd ~/Developer/ClubCraftPhase0
git pull origin main
git log -1 --oneline
```

`git pull` が成功した後、表示された最新commitにPhase 3の実装が含まれていることを確認します。

> `fatal: not a git repository` と表示された場合は、このフォルダは以前のZIP展開版です。ファイルを上書きせず、エラー全文を共有してからGit clone版へ移行してください。

## 2. Universal Releaseを作る

同じTerminalで実行します。

```bash
./build-release.sh
```

成功時は、次の形式のReleaseフォルダが表示されます。

```text
dist/ClubCraft-0.4.0-macos-universal
```

`Club Craft.vst3` が生成され、AUが対応環境で生成された場合は `Club Craft.component` も同じフォルダへ入ります。

## 3. VST3を安全に入れ替える

Ableton Liveを**完全に終了**してから実行します。まず、すでに入っているVST3を日時付きでバックアップします。

```bash
PLUGIN_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
SOURCE="$HOME/Developer/ClubCraftPhase0/dist/ClubCraft-0.4.0-macos-universal/Club Craft.vst3"
STAMP="$(date +%Y%m%d-%H%M%S)"

if [ -d "$PLUGIN_DIR/Club Craft.vst3" ]; then
  mv "$PLUGIN_DIR/Club Craft.vst3" "$PLUGIN_DIR/Club Craft-phase2-backup-$STAMP.vst3"
fi

cp -R "$SOURCE" "$PLUGIN_DIR/Club Craft.vst3"
codesign --force --deep --sign - "$PLUGIN_DIR/Club Craft.vst3"
```

その後、Liveを再起動し、Plug-ins設定で再スキャンしてください。

## 4. まず音が出る状態を作る

1. 音源TrackごとにClub Craftを挿し、ROLEを **SOURCE** にします。
2. それらをGroupにまとめ、そのGroupへClub Craftを挿してROLEを **CLUB** にします。
3. CLUBのSpeaker Typeは、最初は4本すべて `FULL RANGE` にしてください。

`FULL RANGE` はPhase 2と同じ基準状態です。最初にこれで音が出ることを確認してから、1本ずつTypeを変えてください。

## 5. Speaker Typeを聞き比べる

CLUBの `SPEAKER VOICES` で、各SpeakerのTypeとLevelを変えます。最初はSpeaker Levelをすべて0 dBにして試すと分かりやすくなります。

| 確認したいこと | 操作 | 期待される変化 |
|---|---|---|
| 低域の役割 | 4本を一度に `SUB` にする | Kick / Bassの最も低い帯域が残り、HatsやVocalの輪郭は大きく減る。 |
| 低中域の厚み | 4本を `WOOFER` にする | KickやBassの胴鳴りが目立ち、高域は減る。 |
| 基準状態 | 4本を `FULL RANGE` に戻す | Phase 2に近いFull Signalの状態へ戻る。 |
| 中域の存在感 | 4本を `MID` にする | Vocal、Snare、Synthの中心帯域が残る。 |
| 高域の輪郭 | 4本を `HIGH` にする | Hatsやairが残り、低域は大きく減る。 |

次に、4本すべてを同じTypeにするのではなく、たとえば `FRONT L = SUB`、`FRONT R = WOOFER`、`REAR L = MID`、`REAR R = HIGH` のように分けてください。それから各Speaker Levelと位置を少しずつ動かすと、直接EQを操作せずにトーンバランスを作る感覚を試せます。

## 6. 保存の確認

Live Setを保存してLiveを再起動します。CLUB側のSpeaker Type、Speaker Level、Generic Response、Speaker Spread/Depthが戻れば、Phase 3の状態保存は成功です。

## 注意

Speaker Typeは精密なEQや市販Speakerの物理モデルではありません。特にSUBやHIGHだけでミックス全体を仕上げる用途ではなく、**仮想クラブのSpeaker配分で音色を考えるための操作**です。最終的な技術的な補正が必要な場合は、既存のmeteringや必要最小限の補助処理を併用してください。
