# Club Craft — ChatGPT開発引き継ぎ資料

**対象リポジトリ:** `https://github.com/earns7572-creator/clubcraft-phase0`  
**基準branch:** `main`  
**基準commit:** `6eaa05e185fb291ea8e9b2989868e25bbdf29279`  
**現行バージョン:** `0.5.0`  
**作業言語:** 日本語。依頼者は初心者なので、専門用語を使う場合は先に短く説明すること。

> **最重要の製品概念:** Club Craftは、通常のEQノブを中心にしたミキシング／マスタリングではなく、仮想クラブのSpeaker配置・Speaker Type・Speaker出力・Listener（観客）位置によってトーンと空間を作るDAWプラグインである。

---

## 1. 今すぐ守るべき正しい空間モデル

以前はSOURCE（各DAW TrackのClub Craft）がX/Y位置を持つ実装だったが、これは製品要件から外れているため撤回済みである。**今後、SOURCE位置のUIやパラメータを復活させてはいけない。**

| 対象 | 正しい役割 | 操作できる場所 |
|---|---|---|
| SOURCE | DAW Trackの音をクラブの固定ステージ原点から発生させる。位置は常に `(0, 0)`。 | 操作不可 |
| CLUB | 全SOURCEに共通するSpeaker、Listener、Speaker Type、Masterを管理する。 | CLUBインスタンスのみ |
| Speaker | `FRONT L`、`FRONT R`、`REAR L`、`REAR R`。各Speakerは独立したX/Y位置、Type、Levelを持つ。 | CLUBインスタンスのみ |
| Listener / Audience | 観客がクラブ内のどこで聴くか。X/Y位置を持つ。 | CLUBインスタンスのみ |

### 座標と現行DSP

X軸は左（負）から右（正）、Y軸は後方（負）から前方（正）である。SOURCEは固定原点 `(0, 0)`。各Speakerについて、`SOURCE → Speaker` と `Speaker → Listener` の合計経路長を求める。

1. 合計経路長に応じた相対Levelを計算する。
2. 最短経路との差から最大60 msの相対delayを計算する。
3. 経路が長いほどGeneric Responseの高域を暗くする。
4. SpeakerとListenerの相対X位置を、Speaker→Listener距離で正規化してconstant-power stereo panを計算する。
5. Speaker Type CharacterとGeneric Responseを通し、Stereo L/Rへ合算する。

普通のStereo出力だけでは、Rearがヘッドホン上で「完全に真後ろ」へ定位するわけではない。現状のFRONT/REAR差は、距離、delay、高域減衰、panの差として表現される。**HRTF、反射、残響は未実装であり、将来のPhaseで足す。**

---

## 2. プロジェクトの技術構成

| 項目 | 内容 |
|---|---|
| 言語 | C++20 |
| Framework | JUCE 9.0.1 |
| Plugin形式 | VST3。macOSでは対応時にAUも生成する |
| モジュール | `Club Craft.vst3` の**単一バイナリ**。各インスタンスでSOURCE / CLUB roleを選択する |
| Build | CMake 3.22以上 + Ninja |
| State | `juce::AudioProcessorValueTreeState` (APVTS)、現行schema version 6 |
| Mac Release | `./build-release.sh`。clean build、Universal Binary、VST3、AU、CTest、`dist/` packagingを行う |
| GitHub | private repository。`main`へ小さく検証済みcommitをpushする |

### 単一VST3バイナリが必須である理由

SOURCEとCLUBの共有Sceneは、同一ホストプロセス内の静的Registryを使う。SOURCE用とCLUB用を別VST3バイナリにすると、ホストが別アドレス空間へロードする可能性があり、共有状態が壊れる。**SOURCEとCLUBを別プラグインへ分離しないこと。**

### Realtime Safetyの絶対条件

`processBlock()` はaudio threadで実行される。次を絶対に守ること。

| 禁止事項 | 理由 |
|---|---|
| lock / mutex取得 | 音切れの原因になる |
| メモリ確保・解放 | realtime安全ではない |
| `std::vector`のresize / push_back | メモリ確保を伴う可能性がある |
| 待機、sleep、I/O、ログ出力 | audio threadを止める |
| `std::atomic<std::shared_ptr<T>>` | 旧Intel macOS libc++互換性を失う |

`SessionRegistry` は、control threadではmutexを使うが、audio threadではprimitive atomicsだけを読む。複数値の整合性はseqlockで保つ。DSP用のdelay line、filter、scratch領域は `prepareToPlay()` で確保し、`processBlock()` 中に確保しない。

---

## 3. 現行ファイルの役割

| ファイル | 役割 |
|---|---|
| `CMakeLists.txt` | JUCE target、plugin format、tests、version `0.5.0` |
| `build-release.sh` | macOS Universal Releaseの自動化。できるだけ変更しない |
| `src/common/SessionRegistry.h/.cpp` | CLUB→SOURCEのScene公開。Speaker Type、Speaker X/Y、Listener X/Y、Levelをprimitive atomicsで共有 |
| `src/common/SpatialMath.h` | 距離、relative gain、relative delay、constant-power pan、Speaker/Listener相対pan |
| `src/common/SpeakerType.h` | `SUB`、`WOOFER`、`FULL RANGE`、`MID`、`HIGH` のenumとparameter変換 |
| `src/common/SpeakerCharacterResponse.h` | Speaker Typeごとの広い帯域Character |
| `src/common/GenericSpeakerResponse.h` | Generic Response（距離で暗くなるlow-pass） |
| `src/common/StereoSpatialRenderer.h` | 固定SOURCE、可動Speaker、可動ListenerをStereo L/Rへ合成するDSP |
| `src/club/PluginProcessor.h/.cpp` | APVTS parameters、role管理、Scene publish、audio callback |
| `src/club/PluginEditor.h/.cpp` | 暖色グレー基調のminimal UI。CLUBではListenerと個別Speakerを操作する |
| `tests/SessionRegistryTests.cpp` | assertベースの軽量CTest。Scene公開、Type、Character、pan、delayを検証 |

---

## 4. 実装済みの履歴

| 段階 | commit | 実装内容 | 状態 |
|---|---|---|---|
| Phase 0 | 初期履歴 | VST3基盤、SOURCE / CLUB role、Session Registry、state save/load、pass-through | 完了 |
| Phase 1 | `0051694` | 4 Speaker Full Signal、個別Level、Generic Response、正規化 | 完了 |
| Phase 2 | `9a81310` | 初期の平面Spatial処理。後に操作モデルを修正 | 一部置換済み |
| Phase 3 | `a847c64` | Speaker Type (`SUB/WOOFER/FULL RANGE/MID/HIGH`) とCharacter | 完了 |
| Fixed Source update | `6eaa05e` | SOURCE固定、各Speaker X/Y、Listener X/Y、Listener相対pan | 現行基準 |

### 現行Speaker Type Character

| Type | 役割の目安 | 現行filter目安 |
|---|---|---|
| `SUB` | 最低域の重心 | 28 Hz high-pass、110 Hz low-pass |
| `WOOFER` | Kick / Bassの胴鳴り | 55 Hz high-pass、420 Hz low-pass |
| `FULL RANGE` | 旧Phase互換の基準 | Type固有filterなし |
| `MID` | Vocal / Snare / Synthの中心 | 220 Hz high-pass、4.6 kHz low-pass |
| `HIGH` | Hats / Air / 輪郭 | 2.4 kHz high-pass |

これらは市販Speakerの測定モデルではない。実在機材の再現と主張しないこと。

---

## 5. 現在の完了事項と未解決事項

### 実装・検証済み

- VST3 Debug build成功（Linux）。
- CTest `ClubCraftCoreTests` 合格。
- Version 0.5.0のVST3 `moduleinfo.json` 生成確認。
- バイナリに `LISTENER / AUDIENCE POSITION` と `INDIVIDUAL SPEAKER PLACEMENT & VOICES` が含まれることを確認。
- macOS Intelで `build-release.sh` によるUniversal Binary `x86_64 / arm64` VST3/AU生成を過去に確認。
- Phase 0.5.0のmacOS Live実機試用は、次回必ず行うこと。

### 現在の弱点

| 弱点 | 正しい対応方針 |
|---|---|
| Speaker位置とListener位置が数値sliderのみ | 次に2Dフロアビューと直接ドラッグを実装する |
| FRONT / REARの前後感が弱い | HRTFを急がず、まず初期反射・crossfeed・距離に連動する空間差を検討する |
| Speaker配置の結果が見えない | Speakerごとの寄与メーター、経路距離、delay、panを可視化する |
| Speaker Typeは固定Character | 将来、強さ（Character Amount）や安全なCrossover設定を追加する |
| A/B・bypass・solo・muteがない | ミックス判断のためのworkflow機能を追加する |
| Presetがない | venue templateとユーザーScene保存を設計する |
| 多ホストQAが不足 | Ableton Live Intel / Apple Silicon、Logic AU、Reaperなどで検証する |
| 署名・notarization未設定 | Release直前まで必須にせず、環境変数対応の既存scriptを維持する |

---

## 6. 完成版（1.0）までの必須ロードマップ

ChatGPTは、原則として一度に1 Phaseだけ実装すること。各Phaseの最後に、tests、VST3 build、資料、Git commit/pushを完了し、ユーザーにMacで試すタイミングを確認する。

### 現在地点 — Version 0.5.0: Correct Spatial Ownership

固定SOURCE、個別Speaker X/Y、Listener X/Yへ修正済み。次の開発はここを壊さずに始める。

### Phase 4 — Version 0.6.0: 2D Club Floor View

**目的:** 数字のsliderではなく、クラブ平面図でSpeakerとListenerを理解・操作できるようにする。

| 実装 | 完了条件 |
|---|---|
| CLUB UIに2Dフロアビューを追加 | SOURCEは固定ステージマーカー、4 SpeakerとListenerが表示される |
| Speaker / Listenerのドラッグ | ドラッグが既存APVTS X/Y parameterを更新し、host automation / state saveと整合する |
| 軸・前後・左右の表示 | ユーザーがY+ = FRONT、Y- = REARを迷わない |
| 数値sliderとの双方向同期 | drag後にもX/Y値が見え、数値変更後にも図が動く |
| SOURCEはドラッグ不可 | 固定SOURCE要件をUIで破れない |

**検証:** SOURCEが固定であること、SpeakerとListenerのdragがstate save/reload後に復元すること、CLUB以外では図がread-onlyまたは非操作であること。

### Phase 5 — Version 0.7.0: Understandable Spatial Feedback

**目的:** Speaker / Listenerを動かした結果を、耳と表示の両方で理解できるようにする。

| 実装 | 完了条件 |
|---|---|
| Speaker寄与メーター | 各Speakerの最終経路Levelを表示する。audio threadから安全にmeter値を渡す |
| Path inspector | 各Speakerの距離、relative delay、pan、toneをcontrol threadで算出・表示する |
| Solo / Mute | Speakerごとに聴き比べでき、状態保存される |
| Safe A/B / global bypass | 変更前後を安全に比較できる |

**注意:** UI meter更新はTimerで行い、audio threadでlockやallocationを行わない。A/Bはparameter stateのcopyやatomic swapを慎重に設計する。

### Phase 6 — Version 0.8.0: Perceptual Front / Rear Cues

**目的:** 固定SOURCE / Speaker / Listenerモデルを維持したまま、FRONT / REARの違いをより聞き取りやすくする。

| 優先実装 | 内容 |
|---|---|
| Speaker directivity | Speakerの向き／広がりを簡易モデル化し、Listener角度に応じて高域とLevelを変える |
| 初期反射 | 少数の短い反射を追加し、前後・距離の差を補う。過度な残響にしない |
| stereo crossfeed | 近接するSpeakerの左右漏れを控えめに追加し、耳に不自然なhard panを避ける |
| HRTF research mode | 必要ならオプション実験として追加するが、個人化HRTFを完成条件にしない |

**完了条件:** 対称ではない配置とListener移動で、FRONT / REARの差を小型モニターとヘッドホンの両方で理解しやすい。CPU、latency、click/noiseを測定する。

### Phase 7 — Version 0.9.0: Club Mixing Workflow

**目的:** 実際の制作で使える操作と安全性を追加する。

| 実装 | 完了条件 |
|---|---|
| Venue template | Small club、wide room、front-heavy、rear-heavyなどの初期Scene |
| Speaker Type template | Sub / Woofer / Full Range / Mid / Highの推奨組み合わせ |
| 入出力meterとclip警告 | MasterとSpeaker経路を監視し、過大入力を分かりやすく表示 |
| Output safety | 任意の安全出力trim / soft clip / limiter。デフォルトで音を破壊しない |
| Session管理 | Session IDを安全に確認・変更でき、複数Club Sceneが混線しない |
| 状態migration | 旧Phaseのstateを読み、可能な範囲で新stateへ安全に移す |

**禁止:** 便利だからと通常の多バンドEQ画面を主インターフェースにしない。Club Craftの中心はSpeakerとListenerである。

### Phase 8 — Version 1.0.0: Production Hardening and Release

**目的:** 配布可能な安定版へ仕上げる。

| 領域 | 必須項目 |
|---|---|
| DSP | sample-rate 44.1/48/88.2/96 kHz、block size 32〜2048、offline render、silence、reset、automationを検証 |
| Host | Ableton Live Intel / Apple Silicon、Logic Pro AU、Reaperなどでロード・保存・再読込・bounceを検証 |
| Format | Universal VST3、必要ならAU。Bundle manifest versionを確認 |
| UI | 低解像度／高解像度、resize、DAWテーマ、keyboard focus、parameter名を確認 |
| Release | `build-release.sh`、CTest、dist manifest、SHA、署名／notarizationは任意環境変数で実行 |
| Docs | 概念、Quick Start、Speaker / Listener用語、既知の制約、troubleshooting、Mac更新手順 |

**1.0の最小完了条件:** 主要DAWで安定してロードでき、Sceneが保存・復元し、Clubの操作によって意図した音色・位置差を作れ、audio thread安全性とCPU負荷の要件を満たすこと。

### 1.0以降の任意拡張

- 個人化HRTF、head tracking、binaural mode。
- 高度なroom model、壁材、遮蔽、残響。
- 8 Speaker以上のlayoutとsurround / immersive output。
- 実機Speaker測定に基づくCharacter pack。
- 複数人共有のクラブSceneや外部controller連携。

これらを1.0の前提にして、現行の操作モデルを複雑にしないこと。

---

## 7. 各Phaseで必ず行う開発手順

1. `main`がcleanであることを確認する。
2. 変更前に、今Phaseの目的・対象外・parameter追加／削除・state migration方針を短い設計資料へ書く。
3. 小さく実装する。既存のSOURCE固定、single-binary、Realtime安全性を破らない。
4. `tests/SessionRegistryTests.cpp` に該当する自動テストを追加する。必要に応じてテストtargetを拡張する。
5. Linuxで次を実行する。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCLUBCRAFT_BUILD_TESTS=ON
cmake --build build --target ClubCraftCoreTests ClubCraft_VST3 --parallel 1
ctest --test-dir build --output-on-failure
```

6. VST3 bundleの `Contents/Resources/moduleinfo.json` とUI文字列を検査する。
7. READMEとIntel Mac試用資料を更新する。
8. `git status`、`git diff --check`を確認する。
9. 一つの意味のあるcommitを作り、`main`へpushする。
10. ユーザーへ、**何が変わったか、何がまだできないか、Macで何を聞けばよいか**を初心者向けに説明する。

---

## 8. macOS IntelでのReleaseと安全な更新

### Release作成

```bash
cd ~/Developer/ClubCraftPhase0
git pull origin main
./build-release.sh
```

成功時のReleaseは次の形式で作られる。

```text
dist/ClubCraft-<VERSION>-macos-universal
```

### VST3更新の安全な原則

1. Ableton Liveを**完全に終了**する。
2. Release側の `Club Craft.vst3` が存在することを先に確認する。
3. 既存VST3を日時付きでバックアップする。
4. 新VST3を `~/Library/Audio/Plug-Ins/VST3/Club Craft.vst3` へコピーする。
5. `codesign --force --deep --sign -` でad-hoc署名する。
6. Liveを完全に起動し直し、必要ならPreferencesのPlug-insからRescanする。
7. `.vstpreset` ではなく、BrowserのPlug-insにある**プラグイン本体**を新しいTrackへ挿して確認する。

`codesign`後はMac binaryのSHAがRelease元と異なることがある。これは署名情報が書き込まれるため正常である。SHA一致だけで失敗と判断しない。

---

## 9. 既存資料

| 資料 | 用途 |
|---|---|
| `README.md` | 概要、Phase 1〜3、Fixed Sourceの説明 |
| `PHASE2_DESIGN.md` | 旧Planar Spatialisationの設計。SOURCE位置は現行仕様では使わない |
| `PHASE2_TRYOUT.md` | 旧試用手順。Source位置部分は現行仕様に適用しない |
| `PHASE3_DESIGN.md` | Speaker Type Characterの設計 |
| `PHASE3_TRYOUT.md` | Speaker Type試用手順 |
| `FIXED_SOURCE_LAYOUT_DESIGN.md` | **現行空間モデルの正しい設計資料** |
| `FIXED_SOURCE_LAYOUT_TRYOUT.md` | **Version 0.5.0のMac / Live試用手順** |

---

## 10. ChatGPTへの実装時の姿勢

- 不明な要件は、実装前に短い質問で確認する。ただし、現在明確な固定SOURCE要件は質問し直さない。
- 初心者へ案内する場合、Terminalコマンドはコピー・貼り付け可能な短い単位にし、コメント行を含めない。過去にzshで `#` 行がコマンドとして実行され、作業が止まった。
- 「立体的」と言う場合、Stereo simulation、HRTF、room reflectionのどこまで実装しているかを正直に区別する。
- 既存の動作を壊す変更では、state migrationとrollback方法を先に設計する。
- macOSで実機確認していないことを、確認済みと表現しない。
- リポジトリへpushする前に、必ずbuildとtestsを通す。
