# CurveSmear AE実装メモ

## 初版の範囲

- Windows x64用 `.aex`。AE 25系を基準にし、26系でも動作を確認する。
- AEがデコードしたRGBAレイヤーを処理する。PNG、通常素材、EXRの違いはエフェクト側では区別しない。
- 同じレイヤーの開いたマスクパス1本を流れ場として使う。マスクモードは「なし」を想定する。
- 入力画像の位置と構図を保ち、Radius内だけをカーブに沿って変形する。
- 8/16/32bpcに対応し、32bit floatのHDR値を0〜1へクランプしない。
- 出力領域はレイヤー境界内とする。

## 0.1.3で実装した項目

- `End Caps`に`Round`と`Flat`を追加。Flatは始点・終点の接線に直角な境界を使う。
- `Sampling`に`Linear`と`Nearest`を追加。Nearestではサンプリング由来の中間色を作らない。`Keep Original`による混合は従来どおり残す。
- カーブ法線方向のL〜Rを横軸、smearの伸び倍率0〜100%を縦軸とする`Width Profile`を追加。
- グラフ上の点の追加、ドラッグ、選択中間点の削除、滑らかな曲線／直線補間を実装。
- `Reset`でL/Rを100%に戻し、`Swap L/R`で中間点を含むグラフ全体を左右反転する。
- `Show Influence`はL側をオレンジ、R側を青で表示する。
- グラフはカットごとの固定値とし、キーフレームには対応しない。
- 新規適用時はFlatとNearestを初期値にする。旧プロジェクトはRoundとLinearを維持する。
- プロファイル評価はレンダー開始時に257要素のLUTへ変換し、ピクセルごとの曲線探索を避ける。

## 0.1.4で実装した項目

- 任意の白黒レイヤーを選べる`Source Matte`を追加。未指定時は従来結果を維持する。
- `Matte Channel`でLuminance／Alphaを切り替え、`Invert Matte`で反転できる。
- マットは出力位置ではなく、CurveSmearが計算した元画像の参照座標で評価する。
- 白は従来の変形結果、黒は現在位置の元画像、グレーはその中間として合成する。
- Nearest時はマットも最近傍で読み、0/1マットへ新しい中間値を加えない。Linear時はマットも線形補間する。
- Source MatteのSmart Render checkoutを追加し、レイヤー内容と時間変化をAEの依存関係へ含める。

倍率は法線位置を0〜1へ正規化して評価し、既存の移動量`shift`へ掛ける。0%ではその位置の変位を止め、100%では従来の伸び量を保つ。Radius、Edge Feather、Keep Originalとは独立した制御として扱う。

## 保存形式

`Width Profile`は固定長の任意データパラメーターとして保持する。現在のスキーマは`CSP1`、最大16点。Undo/Redo、エフェクトの複製・コピー、AEP保存・再読み込みに必要なnew/dispose/copy/flatten/unflatten/compare/interpolate/print/scanコールバックを実装している。

## 検証結果

- SDK 25.2および25.6で警告なしにビルド。
- AE 25.5でエフェクト追加、パス指定、描画、カスタムグラフのドラッグ、中間点追加・削除、Reset、Swap L/Rを確認。
- AE 25.5で編集したAEPを保存し、AE 26.3で変換して読み込み。CurveSmearの18パラメーターが保持され、読み込みエラーがないことを確認。
- 0.1.3で保存したAEPを0.1.4として開き、既存値を保ったまま21プロパティへ移行し、Source Matte=None、Luminance、Invertオフで初期化されることをAE 26.3で確認。
- AE 25.5／26.3の実レンダーで、黒Luminanceが無変形、白Luminanceが変形、黒＋Invertおよび黒レイヤーのAlphaが白Luminanceと一致することを確認。
- 8/16/32bpc、HDR、premultiplied alpha、タイル出力、切り抜かれた入力、縮小表示、Reverse、Flat端、プロファイル評価、Nearest、Source Matteの白黒・Luminance・Alpha・反転をネイティブテストで確認。
- `PF_OutFlag2_I_MIX_GUID_DEPENDENCIES`を宣言し、パス未指定時を含むすべてのSMART_PRE_RENDERで`GuidMixInPtr`を呼ぶ。

## 今後の候補

- 1920×1080で約55msという実素材の報告を基準に、セグメント探索や影響範囲の走査を最適化する。
- Flat端の専用フェザーや端位置のオフセットは、実際の使用感を見て追加する。
- 剣の根元から剣先への自動勾配は今回含めない。Width Profileを手動調整として先に評価する。
