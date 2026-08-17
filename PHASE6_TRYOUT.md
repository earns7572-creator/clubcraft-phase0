# Club Craft 0.6.0 — Intel Mac / Ableton Live試用手順

## この版で確認すること

0.6.0では、新しいSpeaker追加UIやBinauralはまだありません。確認する対象は、旧4 Speaker Live Setが壊れずに開くこと、SOURCE / CLUBの接続状態、legacy compatibility route、そしてDynamic Scene基盤です。

## 1. GitHubから0.6.0を取得してReleaseを作る

Ableton Liveを終了した状態でTerminalを開き、以下を実行します。

```bash
cd ~/Developer/ClubCraftPhase0
git pull origin main
./build-release.sh
```

成功時は、次のようなReleaseフォルダが表示されます。

```text
~/Developer/ClubCraftPhase0/dist/ClubCraft-0.6.0-macos-universal
```

## 2. 既存VST3をバックアップして0.6.0へ入れ替える

Ableton Liveを完全に終了したまま、次の1行を実行します。

```bash
PLUGIN_DIR="$HOME/Library/Audio/Plug-Ins/VST3"; SOURCE="$HOME/Developer/ClubCraftPhase0/dist/ClubCraft-0.6.0-macos-universal/Club Craft.vst3"; STAMP="$(date +%Y%m%d-%H%M%S)"; test -d "$SOURCE" || exit 1; [ ! -d "$PLUGIN_DIR/Club Craft.vst3" ] || mv "$PLUGIN_DIR/Club Craft.vst3" "$PLUGIN_DIR/Club Craft-before-0.6.0-$STAMP.vst3"; cp -R "$SOURCE" "$PLUGIN_DIR/Club Craft.vst3"; codesign --force --deep --sign - "$PLUGIN_DIR/Club Craft.vst3"; echo "0.6.0を配置しました"
```

最後に`0.6.0を配置しました`と出れば成功です。

## 3. 既存Live Setのmigrationを確認する

1. Liveを起動し、Plug-insを再スキャンします。
2. 以前Phase 0.5.0までで保存したSetを開きます。
3. CLUBを開き、4 SpeakerのLevel、Type、X/Y、Listener X/Yが復元されていることを確認します。
4. SOURCEを再生し、音が完全に無音にならず、Speaker TypeやLevel操作へ反応することを確認します。

旧Setでは、内部的に各SOURCEが4本のlegacy Speakerへ`0.25`ずつ送られます。画面上の操作は従来の4 Speaker parameterのままです。

## 4. 新しい接続Statusを確認する

| 表示 | 意味 |
|---|---|
| `CLUB ACTIVE / compiles speaker & route plans` | そのCLUBがSessionの正本をpublishしている。 |
| `SOURCE CONNECTED / follows CLUB route plan` | SOURCEがCLUBのDynamic Scene / RoutePlanを受信している。 |
| `WAITING FOR CLUB` | SOURCEが先に開かれた。CLUBを同じSessionで開く。 |
| `CLUB CONFLICT / another CLUB owns this Session` | 同じSessionにCLUBが2個ある。1つをSOURCEへ切り替えるか削除する。 |
| `SOURCE ID REKEYED / waiting for explicit routes` | DAW Track複製によるID重複を防ぐため新IDが作られた。意図したRouteはまだ自動コピーされない。 |

## 5. 今回は確認しないこと

この版では、Speakerを5本目以降へ増やすUI、SOURCEからSpeakerへドラッグしてRouteを作るUI、Band Routing、HRTF、4ch / 8ch出力はまだ使えません。これらが見えないのは正常です。

## 問題が起きた時

- Liveで古い画面のままなら、Liveを完全終了してから再起動し、プリセットではなくPlug-ins内のClub Craft本体を新しいTrackへ挿します。
- 無音になった場合、CLUBを1つだけ残し、SOURCEとCLUBが同じSession IDを使うか確認します。
- 0.6.0を戻す時は、`Club Craft-before-0.6.0-...vst3`というバックアップを`Club Craft.vst3`へ戻します。
