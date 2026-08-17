# Club Craft Phase 0 — Intel Mac 試用ガイド

**対象:** Intel CPU搭載Mac（`uname -m` が `x86_64`）
**目的:** Club Craft Phase 0をローカルでビルドし、Ableton Live上でSOURCE／CLUBの最小接続を試すこと

> このガイドで作成するものは**ローカル試作用のDebug VST3**です。macOS用のコード署名・notarizationを伴う配布版ではありません。添付されているLinuxバイナリはMacでは使用できないため、必ず以下の手順でIntel Mac上でビルドしてください。

## 1. 必要なもの

| 項目 | 目的 | 確認コマンド |
|---|---|---|
| Intel Mac | x86_64向けバイナリを作成する | `uname -m` が `x86_64` |
| Xcode Command Line Tools | Apple Clang、SDK、codesign | `xcode-select -p` |
| CMake 3.22以上 | CMakeプリセットで構成する | `cmake --version` |
| Ninja | 高速ビルド | `ninja --version` |
| JUCE 9.0.1 | VST3／UI／AudioProcessor基盤 | プロジェクト直下の `JUCE/` |
| Ableton Live | VST3ロードと通音試験 | LiveのPlug-Ins設定 |

Ableton LiveはmacOSでVST3をサポートし、標準VST3フォルダは `/Library/Audio/Plug-Ins/VST3/` と `~/Library/Audio/Plug-Ins/VST3/` です。[1] このガイドでは、管理者権限を不要にするためユーザーフォルダ側を使用します。

## 2. Xcode Command Line Tools とビルドツールを用意する

ターミナルを開き、まずIntel MacであることとXcode Command Line Toolsの状態を確認します。

```bash
uname -m
xcode-select -p
```

`xcode-select -p` が失敗する場合は、次を実行して表示されるダイアログを完了してください。

```bash
xcode-select --install
```

Homebrewを利用できる環境なら、CMakeとNinjaを追加します。

```bash
brew install cmake ninja
```

Homebrewを使わない場合は、CMakeとNinjaを別途インストールし、上記の確認コマンドが通る状態にしてください。

## 3. ソースを展開し、JUCEを取得する

添付の `clubcraft_phase0_source.zip` を任意の開発フォルダへ展開します。以下では `~/Developer/ClubCraftPhase0` を使用します。

```bash
mkdir -p ~/Developer/ClubCraftPhase0
unzip ~/Downloads/clubcraft_phase0_source.zip -d ~/Developer/ClubCraftPhase0
cd ~/Developer/ClubCraftPhase0
```

次に、プロジェクト直下へ固定バージョンのJUCEを取得します。

```bash
git clone --depth 1 --branch 9.0.1 \
  https://github.com/juce-framework/JUCE.git JUCE
```

ここで次の構成になっていれば準備完了です。

```text
ClubCraftPhase0/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── INTEL_MAC_TRYOUT.md
├── src/
├── tests/
└── JUCE/
```

JUCEは、VST3とAudio Unitを含む複数のプラグイン形式を対象にできるクロスプラットフォームC++フレームワークです。[2]

## 4. Intel Mac向けにビルドする

プロジェクトフォルダ内で、用意済みのIntel専用CMake presetを使います。

```bash
cd ~/Developer/ClubCraftPhase0
cmake --preset macos-intel-debug
cmake --build --preset macos-intel-debug
ctest --preset macos-intel-debug
```

テスト結果に `100% tests passed` と表示されれば、Session Registryのスナップショット公開・読取・SOURCE登録に関するテストは成功です。

生成物は次の場所にあります。

```bash
open "build-macos-intel-debug/ClubCraft_artefacts/Debug/VST3"
```

期待されるバンドルは以下です。

```text
Club Craft.vst3
```

**Intel向けであることを確認**するには、次を実行してください。

```bash
file "build-macos-intel-debug/ClubCraft_artefacts/Debug/VST3/Club Craft.vst3/Contents/MacOS/Club Craft"
```

出力内に `x86_64` が含まれていればIntel Mac向けにビルドされています。

## 5. ローカルVST3としてインストールする

以下はユーザー専用のVST3フォルダへコピーし、ローカル試用向けにad-hoc署名を付ける手順です。

```bash
cd ~/Developer/ClubCraftPhase0
PLUGIN="build-macos-intel-debug/ClubCraft_artefacts/Debug/VST3/Club Craft.vst3"
DEST="$HOME/Library/Audio/Plug-Ins/VST3/Club Craft.vst3"

mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$DEST"
cp -R "$PLUGIN" "$DEST"
xattr -dr com.apple.quarantine "$DEST" 2>/dev/null || true
codesign --force --deep --sign - "$DEST"
```

最後のコマンドはローカル用の**ad-hoc署名**です。販売・配布する製品には、Developer ID署名とnotarizationを別途実装してください。

## 6. Ableton Liveでの最小試用

Ableton Liveを完全に終了してから起動します。`Settings / Preferences` の **Plug-Ins** で **Use VST3 Plug-In System Folders** を有効にしてください。Abletonは、同一Set内で同じプラグインのフォーマットを混在させないことを推奨しています。[1]

最小の試用セットは次の構成です。

| 置く場所 | Club CraftのROLE | 役割 |
|---|---|---|
| Kickトラック | SOURCE | CLUBが公開するPrimary Speakerゲインを受ける。 |
| Bassトラック | SOURCE | CLUBが公開するPrimary Speakerゲインを受ける。 |
| Synthトラック | SOURCE | 同上。 |
| Kick／Bass／SynthをまとめたGroup | CLUB | `PRIMARY SPEAKER` と `MASTER` を公開・適用する。 |

操作手順は次のとおりです。

1. Kick、Bass、Synthなどの音源トラックへ `Club Craft` を挿します。
2. 各プラグイン上部の `ROLE` を **SOURCE** にします。
3. それらのトラックをGroupへまとめます。
4. Groupトラックにもう1つ `Club Craft` を挿し、`ROLE` を **CLUB** にします。
5. CLUB側に `CLUB publisher active` と表示されることを確認します。
6. SOURCE側に `Connected to CLUB` と表示されることを確認します。
7. CLUB側の `PRIMARY SPEAKER` を `0 dB` から `-12 dB` に動かします。全SOURCEのレベルが下がることを確認します。
8. CLUB側の `MASTER` を動かし、Group全体の最終出力が変わることを確認します。
9. Live Setを保存して開き直し、各インスタンスのRoleとゲインが復元されることを確認します。

> **Phase 0の期待値:** これは会場・スピーカー・ルーティングUIの試作品ではなく、通音・状態保存・SOURCE／CLUB接続の基盤検証です。現段階では、SOURCEはすべて単一の仮想Primary Speakerに接続され、Speaker Position、Band routing、RTA、バイノーラル、4ch／8chは未実装です。

## 7. うまく表示されない場合

| 症状 | 対処 |
|---|---|
| LiveのBrowserに表示されない | Liveを完全終了して再起動し、VST3 System Folderを有効にする。`~/Library/Audio/Plug-Ins/VST3/Club Craft.vst3` が存在するか確認する。 |
| 「開発元を確認できない」等でロードできない | 手順5の `xattr` と `codesign --force --deep --sign -` を再実行し、Liveを再起動する。 |
| SOURCEが `Waiting for CLUB` のまま | 同じLive Set内にCLUB役割のインスタンスが1つ存在し、ROLEが `CLUB` であることを確認する。CLUBを一度閉じずに置き直す。 |
| Primary Speakerを動かしても音が変わらない | SOURCEがCLUBより前段のトラックにあり、SOURCE UIが `Connected to CLUB` であることを確認する。Groupへ送る前にSOURCEを挿す。 |
| Apple Silicon Macで試している | このpresetはIntel (`x86_64`) 用です。Apple SiliconではRosettaでLiveを動かすか、`arm64`／Universal Binary用presetを別途追加する。 |

## 8. ここで確認してほしいこと

試用後、次の結果を共有してください。

1. LiveのBrowserで `Club Craft` が見えたか。
2. SOURCEが `Connected to CLUB` になったか。
3. `PRIMARY SPEAKER` が複数SOURCEを同時に変化させたか。
4. Live Setの再読み込み後にROLEとゲインが復元したか。
5. クラッシュ、スキャン失敗、CPU異常、音切れが起きたか。

このフィードバックを基に、Phase 1で **複数Speaker、Full Signalの1対多ルーティング、Generic Speaker Response** へ進みます。

## References

[1]: https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS "Ableton — Using AU and VST plug-ins on macOS"
[2]: https://github.com/juce-framework/JUCE "JUCE — Cross-platform C++ framework for audio plug-ins"
