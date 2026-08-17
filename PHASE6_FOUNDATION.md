# Club Craft 0.6.0 — Dynamic Scene Foundation

## この版の目的

0.6.0は、完成版の操作UIを追加する版ではありません。従来の固定4 Speaker / 全Speaker自動分配プロトタイプを、**可変SpeakerとSOURCE→Speaker Routeを扱えるRealtime安全な内部基盤**へ置き換える版です。

> CLUBがSceneの正本を持ち、SOURCEは自分専用にcompileされた最大16 Routeだけを処理します。

## 処理責任

| 層 | 0.6.0での責任 |
|---|---|
| CLUB Control Plane | Dynamic Speaker、Dynamic Route、Listener、legacy bridgeを保存し、Scene Compilerを実行する。 |
| Scene Compiler | Stable Speaker IDをslot + generationへ解決し、SOURCEごとの固定容量RoutePlanを作る。 |
| SessionRegistry | primitive atomicsとseqlockでRealtime SceneとSourceRoutePlanを公開する。Audio bufferは保存しない。 |
| SOURCE Audio Worker | 自分のRoutePlanを読む。Full + SUM_MONOの線形Virtual Speaker Voiceだけを処理する。 |
| Preview Renderer | Speaker Position / Listener Positionを使って暫定Stereo Previewへ合成する。 |
| 将来のDiscrete Output | この版では未実装。4ch / 8ch方式はOutput feasibility spikeの結論待ち。 |

## Dynamic Sceneの初期容量

| 定数 | 値 | 0.6.0の扱い |
|---|---:|---|
| `MAX_SPEAKERS` | 16 | 上限超過をcontrol側でrejectする。 |
| `MAX_SOURCES` | 128 | Sessionごとに登録できるSOURCE上限。 |
| `MAX_ROUTES_GLOBAL` | 512 | Scene保存・compile上限。 |
| `MAX_ROUTES_PER_SOURCE` | 16 | SOURCE audio threadが読む上限。 |

## Legacy Live Setの互換

schema≤6のStateを開くと、4 Speaker parameterを`legacy-speaker-1`から`legacy-speaker-4`へ移す。旧版では各SOURCEが4 Speakerへ均等に分配されていたため、migration時だけ4本のimplicit Routeに`gain=0.25`を適用する。

新しいDynamic Sceneで作るRouteは`gain=1.0`が初期値であり、Speaker数で自動的に音量を割らない。これが「Speakerを増やす／Routeを作ること自体が音作りになる」という完成版コンセプトの土台になる。

## この版でまだしないこと

- Speaker 5〜16を追加・削除・ドラッグする完成UI。
- BAND filter DSP、Band routing UI、RTA。
- HRTF、完成版Binaural、room reflection。
- QUAD / OCTAの本出力。
- Speaker saturation、distortion、limiter、nonlinear compression。

## 実装済みの保護機構

| リスク | 0.6.0の保護 |
|---|---|
| Speaker slot再利用 | `slot + generation`不一致のRouteを無効にする。 |
| DAW Track複製 | duplicate SOURCEは新しいSource IDへrekeyし、Routeを自動コピーしない。 |
| CLUB二重配置 | 1 Session = 1 authoritative CLUB。2個目はConflictでpublish不可。 |
| Scene更新中の混在 | SceneとRoutePlanに共通revisionを持たせ、一致しない組合せを処理しない。 |
| audio thread負荷 | lock、allocation、UUID、string、map、ValueTree操作をaudio callbackへ入れない。 |
