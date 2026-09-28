# EXLLM Model Check

CASIO EX-word上でEXQ12モデルをストリーム検査する追加コンテンツです。
`MODELS`フォルダ内の複数の`.q12`ファイルから対象を選び、モデル全体を8 KiB単位で読み込みます。

- ファイルサイズ
- FNV-1a 32-bit
- EXQ12署名とversion
- tensor数
- INT8 / FP16 tensor数
- payloadサイズ
- 現行EXLLM runtimeの39-tensor構成との一致

モデル全体をヒープへ展開せず、ファイルや端末内ログを書き換えません。

> [!NOTE]
> EXQ12構造と転送結果を検査するツールです。実際の推論成功、必要RAM、回答品質は保証しません。

## 使い方

1. [`libexword`](https://github.com/brijohn/libexword)でAPPID `XQCHK`をインストールします。
2. 本体内蔵領域の`MODELS`フォルダへ`.q12`ファイルを配置します。
3. ライブラリーから「EXLLM Model Check」を起動します。
4. 上下キーでモデルを選び、決定キーで検査します。戻るキーで終了します。

## ビルド

[`devkitSH4`](https://brain.fandom.com/ja/wiki/devkitSH4)と、その配布元から取得した
[`libdataplus`](https://github.com/brijohn/libdataplus)が必要です。

```sh
export DEVKITPRO=/path/to/devkitPro
export DEVKITSH4="$DEVKITPRO/devkitSH4"
export PATH="$DEVKITPRO/tools/bin:$DEVKITSH4/bin:$PATH"
make
```

生成物は`build/ja/XQCHK/`へ出力されます。

## 対応環境

[`exword-template`](https://github.com/brain-hackers/exword-template)の対象範囲に基づき、DATAPLUS 5 / 6 / 7を理論上の対象とします。
EXQ12モデルの全体読み込みと構造検査は、次の実機で確認済みです。他の機種での動作は保証しません。

| 機種 | 世代 | 結果 |
| --- | --- | --- |
| CASIO XD-B4800 | DATAPLUS 6 | 動作確認済み |
| CASIO XD-N6500 | DATAPLUS 7 | 動作確認済み |

両機種でEXLLM 5Mモデル（5,443,105 bytes、FNV-1a `A16CDCA6`）を検査し、39 tensors（INT8 26 / FP16 13）、payload 5,381,568 bytesとして同一の結果を確認しました。

関連プロジェクト：

- [EXLLM model and training source](https://github.com/ToTo-40417/exllm)
- [EXLLM runtime for EX-word](https://github.com/ToTo-40417/exllm-exword)
- [EXLLM model on Hugging Face](https://huggingface.co/ToTo-40417/EXLLM)
- [EX-word hardware dump](https://github.com/ToTo-40417/exword-hardware-dump)
- [EX-word RAM scanner](https://github.com/ToTo-40417/exword-ram-scanner)

## License

GPL-2.0。詳細は[`COPYING`](COPYING)と[`THIRD_PARTY.md`](THIRD_PARTY.md)を参照してください。

---

EXLLM Model Check is a CASIO EX-word add-on that validates EXQ12 model files as a stream. Select a `.q12` file from the device's `MODELS` directory; the checker reads it in 8 KiB chunks and reports its size, FNV-1a checksum, EXQ12 version, tensor counts, quantization types, payload size, and whether it matches the current 39-tensor EXLLM runtime layout.

The checker does not load the complete model into the application heap and does not modify model files or write device-side logs. Structural validation does not guarantee successful inference, sufficient RAM, or model quality.

### Usage

1. Install APPID `XQCHK` with [`libexword`](https://github.com/brijohn/libexword).
2. Place one or more `.q12` files in `MODELS` on internal storage.
3. Launch **EXLLM Model Check** from Library.
4. Choose a model with Up/Down and press Enter to inspect it. Back exits.

Build with `devkitSH4` and the upstream `libdataplus` dependency using the commands shown above. Artifacts are written to `build/ja/XQCHK/`.

Based on the supported scope of [`exword-template`](https://github.com/brain-hackers/exword-template), DATAPLUS 5, 6, and 7 are theoretical targets. Full-file EXQ12 streaming and structural validation have been tested on the following devices; other models are not guaranteed.

| Device | Generation | Result |
| --- | --- | --- |
| CASIO XD-B4800 | DATAPLUS 6 | Tested |
| CASIO XD-N6500 | DATAPLUS 7 | Tested |

Both devices produced the same result for the EXLLM 5M model: 5,443,105 bytes, FNV-1a `A16CDCA6`, 39 tensors (26 INT8 / 13 FP16), and a 5,381,568-byte payload.

Licensed under GPL-2.0. See [`COPYING`](COPYING) and [`THIRD_PARTY.md`](THIRD_PARTY.md).
