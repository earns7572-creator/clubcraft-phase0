# Intel MacでClub Craftを試すための操作手順

この手順は、**MacがIntel CPU**で、添付した `clubcraft_phase0_source_macos_ready.zip` をすでにダウンロードしている前提です。画面上でクリックする場所と、ターミナルへ貼り付けるコマンドを分けて説明します。

> 途中でエラーが出た場合は、先へ進まず、その画面またはターミナルの**最後の40行程度**をコピーして送ってください。こちらで次の操作を指定します。

## 0. まず確認すること

Finderで **ダウンロード** フォルダを開き、次のファイルがあることを確認してください。

```text
clubcraft_phase0_source_macos_ready.zip
```

ファイル名が違う場合は、以下のコマンド中のファイル名だけ実際の名前に置き換えてください。

## 1. ターミナルを開いて、MacがIntelであることを確認する

1. キーボードで **⌘ Command + Space** を押します。
2. `Terminal` と入力します。
3. **Terminal** を押して開きます。
4. 次の1行をコピーして、Terminalの画面へ貼り付け、**Return** を押します。

```bash
uname -m
```

表示が次のとおりなら、そのまま進んでください。

```text
x86_64
```

`arm64` と表示された場合はApple Silicon Macです。このガイドはIntel用なので、ここで止めて結果を送ってください。

## 2. Xcode Command Line Toolsを入れる

Terminalへ次を貼り付けて、**Return** を押します。

```bash
xcode-select --install
```

画面にインストール確認が出たら、**インストール** をクリックします。完了には少し時間がかかります。

完了後、次を実行します。

```bash
xcode-select -p
```

以下のようなパスが表示されれば成功です。

```text
/Library/Developer/CommandLineTools
```

## 3. Homebrew、CMake、Ninjaを入れる

まずHomebrewがすでにあるか確認します。

```bash
brew --version
```

バージョンが表示された場合は、次のコマンドへ進んでください。

```bash
brew install cmake ninja
```

`command not found: brew` と表示された場合は、Safariで [Homebrew公式サイト](https://brew.sh/) を開き、表示されているインストールコマンドをコピーしてTerminalへ貼り付けてください。途中でMacのログインパスワードを求められた場合は、自分で入力してください。パスワードを入力しても文字は画面に表示されませんが、正常な動作です。

Homebrewのインストールが完了したら、あらためて次を実行します。

```bash
brew install cmake ninja
```

完了後、確認します。

```bash
cmake --version
ninja --version
```

どちらもバージョン番号が表示されれば成功です。

## 4. ソースを展開する

次のブロックを**まとめて**Terminalへ貼り付け、Returnを押します。

```bash
mkdir -p "$HOME/Developer/ClubCraftPhase0"
unzip -o "$HOME/Downloads/clubcraft_phase0_source_macos_ready.zip" -d "$HOME/Developer/ClubCraftPhase0"
cd "$HOME/Developer/ClubCraftPhase0"
ls
```

最後の `ls` の結果に、少なくとも次が見えていれば成功です。

```text
CMakeLists.txt
CMakePresets.json
README.md
INTEL_MAC_CLICK_BY_CLICK.md
src
tests
```

## 5. JUCEを取得する

同じTerminalで、次を貼り付けてReturnを押します。

```bash
git clone --depth 1 --branch 9.0.1 https://github.com/juce-framework/JUCE.git JUCE
```

ダウンロードが終わったら、次で確認します。

```bash
ls JUCE/CMakeLists.txt
```

次が表示されれば成功です。

```text
JUCE/CMakeLists.txt
```

## 6. Intel Mac用にVST3をビルドする

以下を**上から1ブロックずつ**実行してください。

```bash
cd "$HOME/Developer/ClubCraftPhase0"
cmake --preset macos-intel-debug
```

最後に `Build files have been written to` と表示されれば、最初の構成は成功です。続けて実際のビルドをします。

```bash
cmake --build --preset macos-intel-debug
```

数分かかることがあります。最後がエラーで終わらず、Terminalの入力待ち表示に戻れば成功です。

続けて自動テストを実行します。

```bash
ctest --preset macos-intel-debug
```

次のように表示されれば成功です。

```text
100% tests passed
```

## 7. 作成されたVST3をMacへインストールする

次のブロックを**そのまま**貼り付けてReturnを押します。

```bash
cd "$HOME/Developer/ClubCraftPhase0"
PLUGIN="$PWD/build-macos-intel-debug/ClubCraft_artefacts/Debug/VST3/Club Craft.vst3"
DEST="$HOME/Library/Audio/Plug-Ins/VST3/Club Craft.vst3"

if [ ! -d "$PLUGIN" ]; then
  echo "VST3が見つかりません: $PLUGIN"
  exit 1
fi

mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$DEST"
cp -R "$PLUGIN" "$DEST"
xattr -dr com.apple.quarantine "$DEST" 2>/dev/null || true
codesign --force --deep --sign - "$DEST"
echo "インストール完了: $DEST"
```

最後に以下のような行が表示されれば完了です。

```text
インストール完了: /Users/あなたの名前/Library/Audio/Plug-Ins/VST3/Club Craft.vst3
```

## 8. Ableton Liveでプラグインを認識させる

1. Ableton Liveが開いている場合は、**⌘ Command + Q** で完全に終了します。
2. Ableton Liveを再び起動します。
3. 画面上部の **Live** メニューをクリックします。
4. **Settings** または **Preferences** をクリックします。
5. 左側の **Plug-Ins** をクリックします。
6. **Use VST3 Plug-In System Folders** を **On** にします。
7. 左側Browserの **Plug-Ins** を開き、検索欄へ `Club Craft` と入力します。

`Club Craft` が表示されたら認識成功です。表示されない場合は、Liveを再起動してから、次のTerminalコマンドの結果を送ってください。

```bash
ls -la "$HOME/Library/Audio/Plug-Ins/VST3/Club Craft.vst3"
```

## 9. Ableton Live内でSOURCEとCLUBを置く

既存の曲を開いて、Kick、Bass、Synthなど複数の音源トラックがある状態で試すのが最も分かりやすいです。

1. **Kickトラック**を選択します。
2. Browserの `Club Craft` を、Kickトラックのデバイス一覧へドラッグします。
3. プラグイン画面右上の `ROLE` をクリックし、**SOURCE** を選びます。
4. **Bassトラック**にも同じように `Club Craft` を入れ、ROLEを **SOURCE** にします。
5. 必要ならSynthなど他のトラックにも繰り返します。
6. Kick、Bass、Synthのトラック名をクリックして選択します。複数選択には **Shift** を使います。
7. **⌘ Command + G** を押して、選択したトラックをGroupにまとめます。
8. できた**Groupトラック**を選択します。
9. Browserの `Club Craft` を、Groupトラックのデバイス一覧へドラッグします。
10. Group上のClub Craftで `ROLE` を **CLUB** にします。

期待される表示は以下です。

| 場所 | ROLE | 表示される状態 |
|---|---|---|
| Kick／Bass／Synthトラック | SOURCE | `Connected to CLUB` |
| Groupトラック | CLUB | `CLUB publisher active` |

## 10. 音が変わるかを確認する

1. 曲を再生します。
2. Groupトラックの `Club Craft` を開きます。
3. `PRIMARY SPEAKER` を **0 dB** から **-12 dB** まで左へ動かします。
4. Kick、Bass、Synthなど、SOURCEにしたトラックの音が同時に小さくなることを確認します。
5. `MASTER` を動かすと、Group全体の最終音量が変わることを確認します。
6. **⌘ Command + S** でLive Setを保存します。
7. Liveを終了して再起動し、同じSetを開きます。
8. SOURCE／CLUBのROLEと、2つのゲイン設定が残っていることを確認します。

## 11. 今回まだ入っていない機能

今回のPhase 0は、以下だけを確認する基盤です。

- VST3としてLiveへ表示されること
- SOURCEとCLUBが接続すること
- CLUB側のPrimary Speaker LevelがSOURCE全体に作用すること
- Master LevelがGroup出力に作用すること
- Live Setの保存／再読み込みで設定が復元すること

まだ実装していないものは、Speaker配置、Top／Side View、複数Speaker、Full／Band routing、RTA、HRTF、4ch／8chです。まずこの試用の結果を確認してから、次のフェーズへ進めます。

## 12. エラーが出た場合に送ってほしい内容

エラーが出た場合は、次の3つだけを送ってください。

| タイミング | 送ってほしいもの |
|---|---|
| `cmake --preset` が失敗 | Terminalの最後の40行 |
| `cmake --build` が失敗 | Terminalの最後の40行 |
| Liveに表示されない／落ちる | Liveのバージョン、macOSのバージョン、画面表示またはクラッシュ内容 |

こちらで結果を見て、次に貼り付けるコマンドまたはコード修正を具体的に案内します。
