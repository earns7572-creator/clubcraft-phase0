# Club Craft 0.7.0 — Intel Mac / Ableton Live 試用手順

この手順は、**Club Craft 0.7.0 Dynamic Speaker & Routing UI**をIntel Mac上のAbleton Liveで確認するためのものです。今回から、CLUB instanceではFloor ViewでSpeakerとListenerを操作し、Routing Matrixで各SOURCEをSpeakerへ接続できます。

> **大切な前提**：SOURCEは演奏者や音源の位置を表すものではありません。SOURCEは各DAWトラック上で音声を処理する固定Audio Workerです。Floor Viewで動かせるのは**Speaker**と**Listener**だけで、中央の`STAGE / SOURCE ORIGIN`は固定です。

## 1. 最新版を取得してReleaseを作る

Ableton Liveを完全に終了してから、ターミナルを開き、以下を一行ずつ実行してください。

```bash
cd ~/Developer/ClubCraftPhase0
git pull origin main
./build-release.sh
```

最後に次のような表示が出れば、Release作成は成功です。

```text
[Club Craft Release] SUCCESS
[Club Craft Release] Release folder: .../dist/ClubCraft-0.7.0-macos-universal
```

| 作成物 | 場所 |
|---|---|
| VST3 | `~/Developer/ClubCraftPhase0/dist/ClubCraft-0.7.0-macos-universal/Club Craft.vst3` |
| AU（作成できた場合） | `~/Developer/ClubCraftPhase0/dist/ClubCraft-0.7.0-macos-universal/Club Craft.component` |
| ZIP | `~/Developer/ClubCraftPhase0/dist/ClubCraft-0.7.0-macos-universal.zip` |
| ビルドログ | `~/Developer/ClubCraftPhase0/dist/build-release.log` |

## 2. 古いVST3をバックアップして0.7.0を配置する

以下の**一行全体**をコピーしてターミナルへ貼り付け、Enterを押してください。

```bash
PLUGIN_DIR="$HOME/Library/Audio/Plug-Ins/VST3"; SOURCE="$HOME/Developer/ClubCraftPhase0/dist/ClubCraft-0.7.0-macos-universal/Club Craft.vst3"; STAMP="$(date +%Y%m%d-%H%M%S)"; test -d "$SOURCE" || { echo "Release VST3が見つかりません"; exit 1; }; mkdir -p "$PLUGIN_DIR"; [ ! -d "$PLUGIN_DIR/Club Craft.vst3" ] || mv "$PLUGIN_DIR/Club Craft.vst3" "$PLUGIN_DIR/Club Craft-before-0.7.0-$STAMP.vst3"; cp -R "$SOURCE" "$PLUGIN_DIR/Club Craft.vst3"; codesign --force --deep --sign - "$PLUGIN_DIR/Club Craft.vst3"; test -f "$PLUGIN_DIR/Club Craft.vst3/Contents/Resources/moduleinfo.json"; echo "0.7.0を配置しました: $PLUGIN_DIR/Club Craft.vst3"
```

`0.7.0を配置しました`と表示されたら成功です。Ableton Liveを起動し直してください。既に起動済みのLiveへ上書きした場合は、必ずLiveを終了してから再起動します。

## 3. Ableton Liveでの基本セットアップ

まずAudio Trackを2本以上用意し、たとえば`Kick`と`Music`という名前にします。各Audio TrackへClub Craftを挿し、Roleを**SOURCE**にします。次にMaster Track、またはControl用のGroup TrackへClub Craftを1つ挿し、Roleを**CLUB**にします。

| instance | 置く場所 | Role | 役割 |
|---|---|---|---|
| SOURCE | Kick、Musicなど各Audio Track | `SOURCE` | そのトラックの音をCLUBのRoutePlanに従いStereo previewへ出す。 |
| CLUB | Master TrackまたはControl用Group Track | `CLUB` | Sceneの唯一のControl Plane。Floor ViewとRouting Matrixを操作する。 |

CLUBの右上ステータスが`CLUB ACTIVE / Dynamic Scene Control Plane`になれば、Control Planeとして正常です。`CLUB CONFLICT`と表示される場合は、同じLive Set内でCLUB roleのinstanceを1つだけ残し、他はSOURCEへ戻してください。

## 4. 最初のLegacy SetをDynamic Routingへ変換する

以前の0.6.0 Setを開いた場合、最初は4 Speakerへの`0.25 linear`（約-12 dB）Routeがimplicit legacy routingとして存在します。Routing Matrixでセルをクリックするか、`ADD SPEAKER`または`DELETE SELECTED`を押すと、`MATERIALISE LEGACY ROUTES`ボタンが表示されます。

1. `MATERIALISE LEGACY ROUTES`を押します。
2. ダイアログ内の`N Sources → 4 × N Routes`を確認します。
3. `MATERIALISE`を押します。
4. Matrixが明示的な`ON / —`セル表示へ切り替わります。

Cancelを選ぶとSceneは変更されません。materialise後に遅れて開いたSOURCEは自動的に4 Routeを受け取らず、`NO ROUTES`としてMatrixに表示されます。これは意図した0.7.0仕様です。

## 5. Floor Viewの確認

CLUB instanceの左側に`FLOOR VIEW / FIXED STAGE ORIGIN`が表示されます。

1. 丸いSpeakerをドラッグし、X/Y位置を変えます。
2. 三角形の`LISTENER`をドラッグし、客席位置を変えます。
3. 中央の`STAGE / SOURCE ORIGIN`は動かせないことを確認します。
4. Speakerをクリックし、下のInspectorで`TYPE`と`LEVEL`を変更します。
5. `ADD SPEAKER`で最大16 Speakerまで追加できます。

Speakerを削除すると、そのSpeakerを参照するRouteも一つのtransactionとして削除されます。音声側ではRouteを即切断せず、約10msでfade-outします。

## 6. Routing Matrixの確認

右側の`FULL ROUTING MATRIX / SOURCE → SPEAKER`で、行はSOURCE、列はSpeakerです。セルをクリックするとFull + SUM_MONO Routeを作成または削除できます。

| Matrixの見え方 | 意味 |
|---|---|
| `ON` | そのSOURCEからそのSpeakerへのFull Routeが有効。 |
| `—` | Routeが存在しない。クリックで作成。 |
| `OFFLINE / ...` | 現在Liveで開かれていないが、SetにRoute設定が保存されているSOURCE。 |
| `NEW / ...`相当の未接続行 | 新しく登録され、まだexplicit RouteがないSOURCE。 |

検索欄へ`kick`などを入力し、表示するSOURCE行を絞り込めます。Matrixは最大128 SOURCE × 16 Speaker（**2,048 Route**）を扱う前提で、各セルをJUCE Componentとして大量生成せずcustom drawingとViewportで表示します。

## 7. 保存・再読込の確認

1. Speakerを追加し、位置・TYPE・LEVELを変更します。
2. Listenerを動かします。
3. Matrixで少なくとも1つのRouteをON/OFFします。
4. Live Setを保存します。
5. Liveを完全に終了します。
6. 同じSetを開き直します。

次の項目が残っていればState保存は成功です。

- Speaker数、Stable ID、TYPE、LEVEL、X/Y位置
- Listener位置
- explicit RouteのON/OFF、gain、mute情報
- offline SOURCEのRoute設定
- materialise後の`legacyDefaultRouting=false`状態

## 8. 0.7.0で意図的に未実装のもの

以下は故障ではなく、次のversion以降の対象です。

- Band DSP
- HRTFおよび完成版Binaural
- 4ch / 8chの本出力
- RTA
- SOURCE位置のユーザー操作

不具合を見つけた場合は、Ableton Liveのversion、MacのCPU、使用したVST3かAUか、操作手順、画面表示、`dist/build-release.log`の該当箇所を一緒に記録してください。
