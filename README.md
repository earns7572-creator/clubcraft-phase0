# Club Craft — Phase 0

DAW向け **Virtual Club** プラグインの最小基盤です。V1全体のUIやスピーカー・ルーティングを実装する前に、VST3として安全にロード・通音・状態保存できること、および同一モジュール内の `SOURCE` と `CLUB` が会場スナップショットを共有できることを検証します。

## 現在の実装範囲

| 項目 | 実装内容 |
|---|---|
| プラグイン形式 | VST3。現在の添付ビルドは **Linux x86_64 / Debug**。 |
| モジュール | `Club Craft.vst3` の単一モジュール。各インスタンスで `SOURCE` または `CLUB` を選択する。 |
| 音声経路 | ステレオpass-through。SOURCEはCLUBが公開する `PRIMARY SPEAKER` ゲインを適用し、CLUBはそのバスに `MASTER` ゲインを適用する。 |
| 共有状態 | 同一ホストプロセス内の `SessionRegistry`。SOURCEとCLUBはデフォルトの `phase0-default-club` Sessionを共有する。 |
| リアルタイム方針 | オーディオスレッドはロック、メモリ確保、IPCを行わない。SOURCEはimmutable snapshotを原子的に読む。 |
| 状態保存 | Session ID、Source ID、Role、Master Level、Primary Speaker LevelをVST3状態として保存する。 |
| UI | 暖色グレー基調の最小UI。Role、Session状態、Master、Primary Speakerを表示する。 |
| 自動テスト | Session snapshotの公開／読取、ゲイン換算、SOURCE登録／解除をCTestで検証する。 |

> **設計上の重要な判断:** SOURCEとCLUBを別々のVST3バイナリにすると、ホストにより別アドレス空間で読み込まれる可能性があります。Phase 0では、同じプロセス内の静的レジストリを確実に共有するため、単一の `Club Craft.vst3` を使い、各インスタンスで役割を選びます。将来、別バイナリとして分離する場合は、明示的なIPC／helper設計とホスト別検証が必要です。

## DAWでの試用手順

1. `Club Craft.vst3` をVST3フォルダへコピーし、DAWに再スキャンさせます。Linuxの一般的なユーザー領域は `~/.vst3/` です。
2. Kick、Bass、Synthなど各ソーストラックへ `Club Craft` を挿し、UI上部の `ROLE` を **SOURCE** にします。
3. それらのトラックをGroupまたはBusへルーティングします。
4. Group／Busに同じ `Club Craft` を挿し、`ROLE` を **CLUB** にします。
5. CLUBインスタンスの `PRIMARY SPEAKER` を動かすと、同じSessionのSOURCE全ての出力ゲインが変わります。`MASTER` はCLUBの最終出力ゲインです。
6. DAWプロジェクトを保存し、再度開いてRoleとゲイン設定が復元されることを確認します。

Phase 0はスピーカー配置、Band routing、RTA、HRTF、4ch／8chをまだ実装していません。これらは次のフェーズで、ここで確立した状態保存と信号経路の上に追加します。

## Linuxでのビルド

### 前提

- CMake 3.22以上
- C++20対応コンパイラ
- Ninja
- JUCE 9.0.1
- ALSA、X11、OpenGL、Freetype、Fontconfigの開発パッケージ

Ubuntu 24.04の例:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ \
  libasound2-dev libfontconfig1-dev libfreetype6-dev libgl1-mesa-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev
```

このプロジェクトは `JUCE/` ディレクトリにJUCE 9.0.1があることを前提にします。

```bash
git clone --depth 1 --branch 9.0.1 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCLUBCRAFT_BUILD_TESTS=ON
cmake --build build --target ClubCraft_VST3 ClubCraftCoreTests --parallel 1
ctest --test-dir build --output-on-failure
```

`--parallel 1` はメモリに余裕のない環境向けです。通常の開発マシンでは、CPU・メモリ状況に応じて並列数を増やせます。

生成先:

```text
build/ClubCraft_artefacts/Debug/VST3/Club Craft.vst3
```

## macOS／Windows向けビルド

CMake構成とCoreはクロスプラットフォームを前提にしていますが、**この添付物はLinuxでのみ実ビルド・検証済み**です。

| OS | 推奨ツールチェーン | 必要な追加確認 |
|---|---|---|
| macOS | Xcode、CMake、JUCE | Universal Binary、コード署名、Apple Silicon、VST3フォルダへのインストール、AU対応は次フェーズ。 |
| Windows | Visual Studio 2022、CMake、JUCE | x64 Release、VST3 System Folder、署名、Ableton Liveでのロード・保存・バウンス。 |

## 検証済み項目

- CMake Debug構成の成功
- `Club Craft.vst3` のLinux x86_64生成
- VST3 `moduleinfo.json` の生成
- 動的リンクに未解決ライブラリがないこと
- `ClubCraftCoreTests` の合格

## 次の実装フェーズ

Phase 1では、単一のPrimary Speakerゲインを、`Speaker`／`Route`／`Source`からなる不変のSceneモデルへ拡張します。最初にFull Signalの1対多ルーティングとGeneric Speaker Responseを追加し、次にTop View、Speaker Levelフェーダー、RTAへ進みます。

## 依存関係とライセンス

- [JUCE](https://github.com/juce-framework/JUCE) — AGPLv3または商用JUCEライセンス。クローズドソース配布の前に商用ライセンス条件を確認してください。
- [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk) — VST3仕様・SDK。JUCEのVST3サポートの基礎として利用されます。

このプロジェクトは、JUCEの優れたオープンソース基盤に支えられています。役に立った場合は、同プロジェクトへのスターなどでメンテナを支援してください。

## Phase 1: 4-Speaker Full Signal Scene

Phase 1では、同じVST3モジュール内のSOURCE／CLUB共有状態を4本のGeneric Speakerへ拡張しました。CLUBは `FRONT L`、`FRONT R`、`REAR L`、`REAR R` の個別Levelと、全Speakerへ適用する `GENERIC RESPONSE` を公開します。SOURCEは入力ステレオのFull Signalを全Speakerへ分配し、4本の線形ゲインを正規化して合成します。そのため、全Speakerが0 dBならSOURCEの通過レベルも0 dBのままです。

Generic Responseは、0%で約1.2 kHz、100%で約18 kHzのlow-pass cutoffを使う意図的に汎用的なSpeaker colorationです。特定スピーカーモデルや空間測定を表すものではありません。詳細な実機試用は [PHASE1_TRYOUT.md](PHASE1_TRYOUT.md) を参照してください。
