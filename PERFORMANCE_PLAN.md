# CurveSmear 高速化計画

2026-09-14。0.1.7を画質と挙動の比較基準とする。この段階では実装・バイナリの変更は行わない。

初回計測を実施済み：[計測結果と再実行手順](benchmarks/README.md)。1080pの現行CPU処理は短い弧で約69ms、長い弧で約896ms。探索候補の絞り込みを最初の高速化対象とする。AE 26.3でMFR off/onの書き出し時間も記録した。実素材の55msケース、パス取得・checkoutの個別計測は未実施。

## 確認した環境と現状

- Windows、Core i7-14700F、RTX 4060（VRAM 8GB）。nvcc 13.3とNVIDIAドライバー616.56を検出。CUDA/MSVC/AEの組み合わせでビルド・実行できるかは試作で検証する。
- Adobe SDK 25.2のSDK_Invert_ProcAmpにCUDA実装、GPU_DEVICE_SETUP、SMART_RENDER_GPU、GPUDeviceSuiteを使う例がある。
- 現在はCPU描画。MFR対応フラグはあるが、renderPixels内の1フレームのループは逐次処理。
- Curve::mapはカーブ全体の矩形外を除外した後、各ピクセルについて全セグメントを探索する。大きく湾曲したパスや長いパスで無駄な探索が増える。
- ピクセルごとの除算、sin、弧長位置の検索、全画像の入力checkoutも計測対象。主因の順位はまだ未計測。
- 約55msはユーザー報告値であり、現在版・同じ素材・同じ設定で再計測が必要。

## 1. 基準計測と比較画像

1920×1080の短い弧・長い弧・S字・交差パス、小さいRadius・広いRadiusを用意する。Nearest/Linear、マット有無、8/16/32bpcを代表条件で比較する。

パス準備、最近区間探索、座標計算、画像/マットサンプリングを個別に計測する。ネイティブ処理時間と、AE内の入力準備・変換・転送を含む待ち時間を分ける。未キャッシュの初回、設定編集後、画像だけ変わる連番を区別し、中央値と遅い側の値を記録する。MFRでの連番処理速度も別に評価する。

成果物：再現可能なベンチマーク、0.1.7の比較画像、時間内訳。

## 2. CPUとGPUで共用する探索削減

- 画像を小さいタイルに分け、各タイルに影響し得るセグメントの候補リストを作る。Radiusを含む保守的な境界を使い、候補を取りこぼさない。
- 影響外はコピー、影響し得るタイルだけ座標計算する。全セグメント探索を局所的な探索へ置き換える。
- 等距離の候補は従来のセグメント順で決定し、交差部分の選択が変わらないようにする。
- 逆数・単位接線などの事前計算、Streak=0等の不要処理省略を差分確認しながら追加する。
- 計測結果に応じてAEのiterate系APIで行/タイルを並列処理する。MFRと併用した際の競合も測る。
- 広いRadius等で候補が多くなる場合のメモリ量を制限し、全探索へ戻せる構造にする。

成果物：比較用CPU高速版。同じ離散化を使い、画質を落とす近似は初期段階では導入しない。

## 3. CUDA版をAE内で試作

最初に小さいGPUコピー/サンプリング試作で、AE 25/26のGPU呼び出し・入力画像・Source Matteの取得・GPU形式・行幅・座標原点を確認する。デバイスsetupとpre-renderで適切に対応を宣言する。

パス取得と候補リスト準備はCPU、ピクセルごとの最近区間探索・変位・画像/マット参照・合成はGPUへ移す。AEから渡されるGPU worldと実行環境を使い、プラグイン内部で画像を毎フレームCPUへ戻す構造を避ける。GPU以外の前後エフェクトによる転送時間もAE内で測る。

まずCUDAを対象にし、CPU経路を残す。未対応デバイス/設定はGPU描画を受け付ける前にCPU経路へ振り分ける。GPUエラー後にAEが自動でCPU再描画すると仮定しない。OpenCL/DirectX等の追加は対象GPUを広げる必要が出た段階で判断する。

成果物：旧CPU・高速CPU・CUDAの時間と画像差分。GPU計算単体の短さだけで採否を決めない。

## 4. 必要に応じて流れ場を再利用

パス・解像度等が同じとき、画像に依存しない最近区間、弧長位置、符号付き距離等を再利用する案を測る。AmountやWidth Profile編集時に再利用できれば操作応答が改善する。

AE Compute Cacheを候補に、キー・所有権・解放・同時フレーム処理を設計する。元画像とマットのピクセルは毎回評価する。パス、Radiusを含む候補構築条件、縮小率、座標原点等に応じて正しく無効化する。フルHD/4Kと複数レイヤーのメモリ予算を決める。GPU計算だけで十分速い場合は追加しない。

## 5. 画質と実運用の合格条件

- 0/1素材でNearestによる中間値を新たに作らない（既存の合成による値は別）。
- GPUの浮動小数点差で最近傍の参照先が変わり得るため、半ピクセル境界、交差パス、Flat端を重点比較する。差が出れば演算精度/順序を調整し、残る差を明示する。
- Linear、HDR/負値、premultiplied alpha、Source MatteのLuminance/Alpha、Round/Flat、Reverse、Width Profileを検証。
- AE 25系/26系、8/16/32bpc、縮小プレビュー、MFR、複数インスタンス、旧AEP、CPU描画で確認。
- GPUの内部float画像とプロジェクトの色深度の変換も含めて比較する。

仮の目標は、以前55msだった代表ケースでまず10ms未満、GPU版で5ms前後を目指す。これは未計測の開発目標であり達成予測ではない。初回・操作中・連番を別に報告する。初回計測後に現実的な値へ見直す。

## 参照

- native/SmearCore.h、native/CurveSmear.cpp
- C:/SDK/Adobe/AE_SDK/25.2/AfterEffectsSDK/Examples/Effect/SDK_Invert_ProcAmp/
- C:/SDK/Adobe/AE_SDK/25.2/AfterEffectsSDK/Examples/Headers/AE_ComputeCacheSuite.h
- [Adobe: GPU in After Effects](https://helpx.adobe.com/au/after-effects/desktop/get-started/technical-requirements/basics-gpu-after-effects.html)
