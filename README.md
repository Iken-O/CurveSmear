# CurveSmear 0.1.10

開いたAEマスクパスを流れとして使い、パスの周囲だけをカーブに沿って引き伸ばすWindows用After Effectsエフェクトです。

## インストール

GitHub Releasesから取得した`CurveSmear.aex`、または自分でビルドした`dist/CurveSmear.aex`を次の共通プラグインフォルダーへコピーし、After Effectsを再起動します。

`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`

エフェクトは「エフェクト > CurveSmear > CurveSmear」に表示されます。

## 基本的な使い方

1. 素材レイヤーへ開いたマスクパスを描きます。
2. マスクモードを「なし」にします。
3. CurveSmearを適用し、`Flow Path (open mask)`でそのマスクを選びます。
4. `Smear Amount`と`Radius`を調整します。
5. `Width Profile`でカーブのL側とR側それぞれの伸び倍率を調整します。

選択中の素材へ基本設定を作る場合は `scripts/ApplyCurveSmear.jsx` をAEで実行できます。

## パラメーター

- `Smear Amount`: 流れに沿う伸びの量
- `Radius`: カーブからの影響半径
- `Edge Feather`: Radius境界のぼかし
- `Streak Strength` / `Streak Frequency`: 漫画的な筋の強さと細かさ
- `Keep Original`: 元画像との混合
- `Reverse Flow`: 流れを反転
- `Seed`: 筋パターンを変更
- `Show Influence (rendered)`: 影響範囲を描画。L側はオレンジ、R側は青
- `End Caps`: `Round`は始点と終点を円形に、`Flat`は接線に直角な端にする
- `Sampling`: `Linear`は線形補間、`Nearest`は最近傍補間。0/1素材の中間色を避けたい場合は`Nearest`
- `Width Profile`: LからRへ0〜100%の伸び倍率を指定する編集グラフ
- `Smooth Profile`: 点の間を滑らかな曲線で補間。オフでは直線補間
- `Source Matte`: smearへ持ち込める元ピクセルを白黒レイヤーで指定。`None`なら従来動作
- `Matte Channel`: マットの`Luminance`または`Alpha`を使用

`Width Profile`では、点をドラッグして移動し、空いている位置をクリックして中間点を追加します。`Delete Selected Point`で選択中の中間点を削除できます。`Reset`は中間点を消してL/Rを100%へ戻し、`Swap L/R`はグラフ全体を左右反転します。

`Source Matte`と`Matte Channel`はエフェクトの先頭にあります。`Width Profile`はグラフ、`Smooth`、横1列の3操作ボタンを同じ折りたたみグループにまとめています。

倍率グラフはカットごとの固定設定で、キーフレームには対応していません。新規エフェクトの初期値は`Flat`と`Nearest`です。0.1.2以前のプロジェクトを開いた場合は、従来結果を保つため`Round`と`Linear`になります。

`Source Matte`は変形後に参照される元画像座標で評価します。白い元ピクセルはsmearへ運ばれ、黒い元ピクセルを参照する場所は現在位置の画像を維持します。CurveとRadiusは引き続きsmearが現れ得る領域を決めるため、Source Matteの輪郭より外へ対象を伸ばせます。マットの補間は`Sampling`に従います。

## 対応と確認状況

- Windows x64
- AE 25.5および26.3で読み込みと描画を確認
- Adobe After Effects SDK 25.2および25.6でビルド確認
- 8/16/32bpc、HDR値、タイル描画、縮小表示座標をテスト
- PNGとEXRを含む、AEがデコードしたRGBAレイヤーを処理
- 1エフェクトにつき開いたマスクパス1本
- 出力領域はレイヤー境界内

マルチチャンネルEXRでは、必要なパスをAE側で抽出してからCurveSmearを適用してください。

## ビルド

MSVCのDeveloper PowerShellで実行します。

```powershell
python native/build.py --cuda --tests --output CurveSmear.aex
```

別のSDKを使う場合は`--sdk`でAfterEffectsSDKディレクトリを指定します。

```powershell
python native/build.py --cuda --sdk "C:\SDK\Adobe\AE_SDK\AfterEffectsSDK_25.6_61_win\ae25.6_61.64bit.AfterEffectsSDK" --tests --output CurveSmear-0.1.10-cuda-sdk25.6.aex
```
