# CurveSmear

開いたAEマスクパスを流れとして使い、パスの周囲だけをカーブに沿って引き伸ばすWindows用After Effectsエフェクト

## インストール

最新Releasesの`CurveSmear.aex`、または自分でビルドした`dist/CurveSmear.aex`を次の共通プラグインフォルダーへコピーし、After Effectsを再起動

`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`

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

## 対応と確認状況

- Windows x64
- AE 25.5および26.3で読み込みと描画を確認
- Adobe After Effects SDK 25.2および25.6でビルド確認
- 8/16/32bpc、HDR値、タイル描画、縮小表示座標をテスト
- PNGとEXRを含む、AEがデコードしたRGBAレイヤーを処理
- 1エフェクトにつき開いたマスクパス1本
- 出力領域はレイヤー境界内