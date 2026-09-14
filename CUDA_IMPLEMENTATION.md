# CurveSmear 0.1.9 CUDA試作

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

## 現在の制約

- GPU上のカーブデータはレンダーごとに確保・転送・解放する。画像はAEのGPU worldに常駐する。
- MFRではCPU版も複数フレームを並列処理するため、現状の短いパスではCUDAの差が小さい。
- 次の高速化候補は、デバイスごとのカーブデータキャッシュとCUDA allocationの再利用。

## ビルド

```powershell
python native/build.py --cuda --tests --output CurveSmear.aex
```

`--cuda`を省略するとGPUフラグとCUDAコードを含まないCPU版を生成する。
