# CurveSmear 0.1.10 CUDA実装

0.1.8の32px小区画インデックスをCUDAへ移植し、AEのGPU Smart RenderからGPU worldを直接処理する。

## 対応範囲

- NVIDIA CUDAを使用する。非CUDA環境では従来のCPU Smart Renderへ戻る。
- AEの`PF_PixelFormat_GPU_BGRA128`を直接読み書きし、入力画像をプラグイン側でCPUへ戻さない。
- カーブ区間、セルoffset、候補番号、Width Profile LUTだけを各レンダーでGPUへ転送する。
- カーブ探索、変位、Nearest/Linear、Source MatteのLuminance/Alpha、Keep Original、Influence表示を1カーネルで処理する。
- CUDAカーネルではfast-mathを使用せず、CPU版と同じdouble演算順を保つ。
- CUDA 13.3でcompute capability 7.5のSASSとPTXを格納する。RTX 4060ではPTXから実行できる。

## 検証結果

ネイティブテストでは800×520のfloat入力に対し、次のCUDA出力がCPU版と全チャンネル完全一致した。

- Linear
- Nearest
- Linear＋Luminance Matte
- Nearest＋Alpha Matte
- Show Influence

AE 26.3の1920×1080、6フレームTIFF書き出し中央値：

| 条件 | 0.1.8 CPU ms/frame | 0.1.9 CUDA ms/frame |
|---|---:|---:|
| MFR off、8bpc Nearest | 80.17 | 43.00 |
| MFR off、8bpc Linear＋Matte | 84.67 | 48.00 |
| MFR off、32bpc Linear＋Matte | 110.00 | 78.83 |
| MFR on、32bpc Linear＋Matte | 88.83 | 83.50 |

AE出力のNearest 8bpcとLinear＋Matte 32bpcはCPU版と画素一致した。Linear＋Matte 8bpcでは約829万チャンネル値のうち4値だけが1/255異なった。ネイティブfloat出力は一致しているため、AEのCPU/GPU色深度変換と最終量子化による差と判断する。

AE 25.5でも同じSDK 25.2 CUDAバイナリを読み込み、MFR offで8bpc Nearest 47.83ms/frame、32bpc Linear＋Matte 83.17ms/frameを確認した。

## 0.1.10のカーブデータキャッシュ

- `PF_Cmd_GPU_DEVICE_SETUP`でデバイスごとのキャッシュを作成し、`PF_Cmd_GPU_DEVICE_SETDOWN`で解放する。
- CUDAストリームごとに独立した領域を持ち、MFRで別ストリームが同時に描画してもカーブデータを上書きしない。
- セグメント、セルoffset、候補番号を前回値と完全比較し、同一ならCUDA allocationとHost-to-Device転送を省略する。
- マスクパスがアニメーションして値が変わった場合だけ、そのストリームのデータを更新する。
- ストリーム領域はデバイスごとに最大32個。上限を超えた描画だけ一時領域を使い、VRAMが無制限に増えないようにする。

ネイティブテストでは同一パスの6描画を1回の転送と5回のキャッシュヒットで処理し、パス変更後だけ再転送されることを確認している。さらに2本のCUDAストリームを別スレッドから同時に描画し、独立した2領域が作られることを確認している。全ケースのCUDA出力は引き続きCPU float出力と全チャンネル完全一致する。

画像転送とファイル出力を除いたCUDA単体計測（RTX 4060、800×520、50回×7組、3プロセスの中央値）では、通常描画が0.9164msから0.8889msへ約0.0275ms（3.0%）、Amount=0が0.0379msから0.0193msへ約0.0186ms（49.1%）短縮した。通常のAE描画では1フレーム当たりの差が小さく、TIFF書き出しを含むベンチマークの揺れに収まる。AE 26.3のMFR offでは0.1.9と同等、AE 25.5でも全ケースを正常に完走した。

単体計測は次で再実行できる。

```powershell
python native/build.py --cuda --cuda-benchmark --output CurveSmear.aex
```

## 現在の制約

- 画像はAEのGPU worldに常駐する。パスのHost側配列生成と完全比較はレンダーごとに行う。
- MFRではCPU版も複数フレームを並列処理するため、現状の短いパスではCUDAの差が小さい。
- 次の高速化候補は、カーネル内部の演算計測と出力ROIの縮小。

## ビルド

```powershell
python native/build.py --cuda --tests --output CurveSmear.aex
```

`--cuda`を省略するとGPUフラグとCUDAコードを含まないCPU版を生成する。
