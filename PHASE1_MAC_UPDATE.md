# Club Craft Phase 1 — Intel Macの安全な更新・Release・試用手順

この手順では、動作確認済みのPhase 0フォルダとVST3を残したまま、GitHubからPhase 1を新しいフォルダへ取得します。**既存ファイルをいきなり上書きしない**ため、初心者でも戻しやすい方法です。

> Phase 1のGitHub commitは `0051694bc5579e48e798b54c54497ce963542ad1`（`Implement Phase 1 multi-speaker routing`）です。

## 全体像

| 段階 | 何をするか | 既存のPhase 0への影響 |
|---|---|---|
| 1 | 現在のVST3とプロジェクトをバックアップ | なし |
| 2 | GitHubからPhase 1を新規clone | なし |
| 3 | 新規フォルダでUniversal Releaseを生成 | なし |
| 4 | 新しいVST3をAbleton用フォルダへ配置 | 既存VST3をバックアップして置換 |
| 5 | Liveで4 Speaker機能を試す | 問題なら元のVST3へ復元可能 |

## 0. Terminalを開く

Macで **⌘ Command + Space** を押し、`Terminal` と入力してReturnを押します。以下のコードは、1つの枠を**まとめて**コピーしてTerminalへ貼り付け、Returnを1回押してください。

## 1. 現在のPhase 0を退避する

まず、既存プロジェクトを日付付きのバックアップ名へ変更します。ファイルを消す操作ではありません。

```bash
cd "$HOME/Developer" || exit 1

if [ -d "ClubCraftPhase0" ]; then
  BACKUP_NAME="ClubCraftPhase0-phase0-backup-$(date +%Y%m%d-%H%M%S)"
  mv "ClubCraftPhase0" "$BACKUP_NAME"
  echo "Phase 0プロジェクトを退避しました: $HOME/Developer/$BACKUP_NAME"
else
  echo "既存のClubCraftPhase0フォルダは見つかりません。新規取得として続行します。"
fi
```

`Phase 0プロジェクトを退避しました` と出れば成功です。Finderでは `~/Developer/` の中に、`ClubCraftPhase0-phase0-backup-...` というフォルダが残ります。

## 2. GitHubからPhase 1を取得する

次に、GitHub上の最新Phase 1を新しい `ClubCraftPhase0` フォルダとして取得します。

```bash
cd "$HOME/Developer" || exit 1
git clone https://github.com/earns7572-creator/clubcraft-phase0.git ClubCraftPhase0
cd "$HOME/Developer/ClubCraftPhase0" || exit 1

git log -1 --format='SHA: %H%nMessage: %s'
```

成功時は、SHAが次の値で始まり、Messageが一致します。

```text
SHA: 0051694bc5579e48e798b54c54497ce963542ad1
Message: Implement Phase 1 multi-speaker routing
```

異なるSHAが出ても、より新しいcommitであれば問題ありません。`Message` と表示された内容を送ってください。

## 3. JUCEを取得する

JUCEは第三者依存としてGitHubリポジトリに含めていないため、1回だけ取得します。

```bash
cd "$HOME/Developer/ClubCraftPhase0" || exit 1
git clone --depth 1 --branch 9.0.1 https://github.com/juce-framework/JUCE.git JUCE
ls JUCE/CMakeLists.txt
```

最後に `JUCE/CMakeLists.txt` と表示されれば成功です。

## 4. Universal Releaseを1コマンドで作る

実行権限を付けてから、Release buildを開始します。

```bash
cd "$HOME/Developer/ClubCraftPhase0" || exit 1
chmod +x build-release.sh
./build-release.sh
```

Intel Macでは数分かかる場合があります。成功時に次の3行が出ます。

```text
[Club Craft Release] SUCCESS
[Club Craft Release] Release folder: .../dist/ClubCraft-0.2.0-macos-universal
[Club Craft Release] ZIP archive:     .../dist/ClubCraft-0.2.0-macos-universal.zip
```

このスクリプトは、毎回clean build、Releaseビルド、Universal Binaryの `x86_64` / `arm64` 検証、VST3生成、AU生成可能時のAU同梱、CTest、`dist/`への集約まで実行します。途中で失敗した場合はそこで止まるため、古いVST3はまだ置き換わりません。

## 5. 新しいVST3をAbleton Live用フォルダへ安全に配置する

**先にLiveを完全終了してください。** Liveが開いたままだと古いプラグインがメモリに残ります。Liveを終了後、次のコードを実行します。

```bash
cd "$HOME/Developer/ClubCraftPhase0" || exit 1

NEW_VST="$PWD/dist/ClubCraft-0.2.0-macos-universal/Club Craft.vst3"
LIVE_VST="$HOME/Library/Audio/Plug-Ins/VST3/Club Craft.vst3"
VST_BACKUP="$HOME/Library/Audio/Plug-Ins/VST3/Club Craft-phase0-backup-$(date +%Y%m%d-%H%M%S).vst3"

# 生成済みのPhase 1 VST3を確認してから置換を始める。
test -d "$NEW_VST" || { echo "Phase 1 VST3が見つかりません: $NEW_VST"; exit 1; }

# 現在使っているPhase 0 VST3を、削除せず日付付き名で退避する。
if [ -d "$LIVE_VST" ]; then
  mv "$LIVE_VST" "$VST_BACKUP"
  echo "Phase 0 VST3を退避しました: $VST_BACKUP"
fi

# Phase 1 VST3をLiveの標準VST3フォルダへコピーする。
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
cp -R "$NEW_VST" "$LIVE_VST"
xattr -dr com.apple.quarantine "$LIVE_VST" 2>/dev/null || true
codesign --force --deep --sign - "$LIVE_VST"

echo "Phase 1 VST3の配置完了: $LIVE_VST"
```

ここでの「上書き」は、古い `Club Craft.vst3` を削除するのではなく、`Club Craft-phase0-backup-日時.vst3` へ移動して保管してから、新しい `Club Craft.vst3` をコピーする操作です。

## 6. Ableton Liveで確認する

1. Ableton Liveを再起動します。
2. Browserの **Plug-Ins** で `Club Craft` を検索します。
3. Kick、Bass、Synthなどの音源トラックにClub Craftを挿し、ROLEを **SOURCE** にします。
4. それらをGroupへまとめ、GroupトラックへClub Craftを挿してROLEを **CLUB** にします。
5. SOURCEに `Connected / Full Signal to 4 speakers`、CLUBに `CLUB / 4-speaker scene active` と表示されることを確認します。
6. CL​​UB側の `FRONT L`、`FRONT R`、`REAR L`、`REAR R` を動かし、SOURCE全体の音が変化することを確認します。
7. `GENERIC RESPONSE` を100%から0%へ下げ、高域が暗くなることを確認します。
8. Live Setを保存してLiveを再起動し、4 Speaker LevelとResponseが復元することを確認します。

より詳しい音響機能の確認項目は [PHASE1_TRYOUT.md](PHASE1_TRYOUT.md) を参照してください。

## 7. 元のPhase 0 VST3へ戻す方法

Phase 1を試して問題があった場合は、Liveを終了してから次を実行します。`VST_BACKUP` の部分には、手順5で表示されたバックアップの実際のパスを貼り付けます。

```bash
LIVE_VST="$HOME/Library/Audio/Plug-Ins/VST3/Club Craft.vst3"
VST_BACKUP="ここにClub Craft-phase0-backup-日時.vst3のフルパスを貼り付ける"

rm -rf "$LIVE_VST"
mv "$VST_BACKUP" "$LIVE_VST"
codesign --force --deep --sign - "$LIVE_VST"
```

その後、Ableton Liveを再起動すればPhase 0 VST3へ戻ります。プロジェクトソースも `~/Developer/ClubCraftPhase0-phase0-backup-...` に残っているため、コード側も戻せます。
