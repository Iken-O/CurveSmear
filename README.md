# CurveSmear 0.1.2

開いたAEマスクパスを流れとして使い、Radius内だけをカーブに沿って引き伸ばすWindows用After Effectsエフェクトです。

## インストール

`dist/CurveSmear.aex` を次の共通プラグインフォルダへコピーまたはシンボリックリンクし、After Effectsを再起動します。

`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`

エフェクトは「エフェクト > CurveSmear > CurveSmear」に表示されます。

## 使い方

1. 素材レイヤーへ開いたマスクパスを描きます。
2. マスクモードを「なし」にします。
3. CurveSmearを適用し、`Flow Path (open mask)`でそのマスクを選びます。
4. `Smear Amount`と`Radius`を調整します。

選択中の素材へセットアップを自動作成する場合は、AEで `scripts/ApplyCurveSmear.jsx` を実行します。1080pの確認用コンポジションは `scripts/CreateDemo.jsx` で作成できます。

## 主なパラメータ

- `Smear Amount`: 流れに沿う伸びの量
- `Radius`: カーブからの影響半径
- `Edge Feather`: Radius境界のぼかし
- `Streak Strength` / `Streak Frequency`: 漫画的な筋の強さと細かさ
- `Keep Original`: 元画像との混合
- `Reverse Flow`: 流れを反転
- `Seed`: 筋のパターンを変更
- `Show Influence (rendered)`: 影響範囲を描画して確認

## 対応状況

- Windows x64
- AE 25.5および26.3で読み込み・8/16/32bpc描画を確認
- レイヤー境界内のみ描画
- 1エフェクトにつき開いたパス1本

PNGやEXRをプラグインが直接開くのではなく、AEがデコードしたRGBAレイヤーを処理します。マルチEXRでは必要なパスをAE側で抽出してから適用してください。
