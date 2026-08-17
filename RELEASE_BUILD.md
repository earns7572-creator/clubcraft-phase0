# Club Craft — macOS Release Build

`build-release.sh` は、Intel Mac上でClub Craftの**Universal Releaseビルド**を作成するためのスクリプトです。プロジェクト直下で次の1行を実行してください。

```bash
./build-release.sh
```

スクリプトは、既存のJUCE + CMake + Ninja構成を使い、クリーンなReleaseビルド、Universal Binary (`x86_64` / `arm64`) の検証、VST3生成、AU生成、CTest、`dist/`への集約を連続して実行します。

## 実行前の前提

- macOS上で実行すること。
- `JUCE/` がプロジェクト直下に存在すること。
- Xcode Command Line ToolsまたはXcode、CMake、Ninjaがインストール済みであること。
- Intel MacのSDK／ツールチェーンがarm64のクロスコンパイルをサポートしていること。arm64 sliceを生成できないツールチェーンでは、スクリプトは成功扱いにせず、原因とログの場所を表示して停止します。

## 生成物

成功時は、次の場所をTerminalに明示表示します。

| 生成物 | 場所 |
|---|---|
| 展開済みのプラグイン | `dist/ClubCraft-<version>-macos-universal/` |
| 配布用ZIP | `dist/ClubCraft-<version>-macos-universal.zip` |
| ビルドログ | `dist/build-release.log` |
| 内容一覧とアーキテクチャ | `dist/ClubCraft-<version>-macos-universal/MANIFEST.txt` |

VST3は常に生成対象です。macOSではAUも標準で生成対象にし、JUCEがAU bundleを出力した場合だけ同じ `dist/` フォルダへ同梱します。

## 署名とnotarization

未設定でもRelease buildは動作します。ローカル試用、社内検証、CIで署名前成果物を作る場合、設定は不要です。

Developer ID署名を追加する場合だけ、実行前に環境変数を設定します。

```bash
export CLUBCRAFT_CODESIGN_IDENTITY='Developer ID Application: Your Name (TEAMID)'
./build-release.sh
```

notarytoolのKeychain Profileが準備済みなら、次を追加します。

```bash
export CLUBCRAFT_CODESIGN_IDENTITY='Developer ID Application: Your Name (TEAMID)'
export CLUBCRAFT_NOTARY_PROFILE='clubcraft-notary-profile'
./build-release.sh
```

notarizationを設定した場合、スクリプトは配布ZIPをAppleへ提出して完了を待ち、VST3／AU bundleへstapleしてからZIPを作り直します。notarizationだけを設定し、署名を設定しない状態は無効なので、スクリプトは開始前に停止します。

## 任意の調整

| 環境変数 | 既定値 | 用途 |
|---|---|---|
| `CLUBCRAFT_BUILD_AU` | `1` | `0` にするとAUを作らずVST3だけをRelease出力する。 |
| `CLUBCRAFT_BUILD_JOBS` | CPUコア数 | Ninjaの並列数。メモリを抑えたい場合は `1` または `2`。 |
| `CLUBCRAFT_CODESIGN_IDENTITY` | 未設定 | Developer IDコード署名を有効にする。 |
| `CLUBCRAFT_NOTARY_PROFILE` | 未設定 | notarizationとstapleを有効にするKeychain Profile名。 |

例:

```bash
CLUBCRAFT_BUILD_AU=0 CLUBCRAFT_BUILD_JOBS=2 ./build-release.sh
```

## 失敗時の扱い

スクリプトは `set -Eeuo pipefail` で実行されます。CMake構成、コンパイル、テスト、VST3／AU生成、Universal Binary検証、署名、notarizationのいずれかが失敗すると直ちに停止し、失敗したコマンド、行番号、完全なログのパスを表示します。
