from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image
from onnxruntime.quantization import (
    CalibrationDataReader,
    CalibrationMethod,
    QuantFormat,
    QuantType,
    quantize_static,
)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MODEL = ROOT / "model" / "soccer-yolo11n-1280-mobile.onnx"
DEFAULT_MUVY_TRAIN = ROOT / "work" / "soccer-model" / "muvy-train" / "images" / "train"
DEFAULT_BROADCAST_TRAIN = ROOT / "work" / "soccer-model" / "Football-Image-Dataset-main" / "images" / "train"
DEFAULT_OUTPUT = ROOT / "model" / "soccer-yolo11n-1280-mobile-int8.onnx"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create the WASM-friendly quantized phone-view soccer model.")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL, help="Float ONNX model to quantize.")
    parser.add_argument("--muvy-train", type=Path, default=DEFAULT_MUVY_TRAIN,
                        help="MUVY training image directory (events 01–03, camera 01).")
    parser.add_argument("--broadcast-train", type=Path, default=DEFAULT_BROADCAST_TRAIN,
                        help="Football Image Dataset training image directory.")
    parser.add_argument("--muvy-count", type=int, default=48, help="Evenly sampled MUVY calibration images.")
    parser.add_argument("--broadcast-count", type=int, default=12, help="Evenly sampled broadcast calibration images.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Output QDQ int8 ONNX model.")
    return parser.parse_args()


def evenly_sample(paths: list[Path], count: int) -> list[Path]:
    if count <= 0 or not paths:
        return []
    if len(paths) <= count:
        return paths
    indexes = np.linspace(0, len(paths) - 1, count, dtype=int)
    return [paths[index] for index in indexes]


def collect_calibration_images(args: argparse.Namespace) -> list[Path]:
    for directory in (args.muvy_train, args.broadcast_train):
        normalized = {part.lower() for part in directory.parts}
        if normalized & {"eval", "evaluation", "val", "validation", "test"}:
            raise ValueError(f"Use training images for calibration, not held-out evaluation images: {directory}")
    muvy = sorted(path for path in args.muvy_train.glob("*.jpg") if not path.name.startswith("broadcast_"))
    broadcast = sorted(args.broadcast_train.glob("*.jpg"))
    broadcast_sample = (
        broadcast[:: max(1, len(broadcast) // args.broadcast_count)][: args.broadcast_count]
        if args.broadcast_count > 0 else []
    )
    selected = evenly_sample(muvy, args.muvy_count) + broadcast_sample
    if not selected:
        raise FileNotFoundError("No calibration images found. Download the documented training datasets first.")
    return selected


def preprocess(path: Path, size: int = 1280) -> np.ndarray:
    image = Image.open(path).convert("RGB")
    width, height = image.size
    scale = min(size / width, size / height)
    resized = image.resize((round(width * scale), round(height * scale)), Image.Resampling.BILINEAR)
    canvas = Image.new("RGB", (size, size), (114, 114, 114))
    canvas.paste(resized, ((size - resized.width) // 2, (size - resized.height) // 2))
    return np.transpose(np.asarray(canvas, dtype=np.float32) / 255.0, (2, 0, 1))[None, ...]


class SoccerCalibrationReader(CalibrationDataReader):
    def __init__(self, paths: list[Path]):
        self._values = iter({"images": preprocess(path)} for path in paths)

    def get_next(self) -> dict[str, np.ndarray] | None:
        return next(self._values, None)


def main() -> None:
    args = parse_args()
    if not args.model.is_file():
        raise FileNotFoundError(f"Input model not found: {args.model}")
    calibration_images = collect_calibration_images(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    print(f"Calibrating on {len(calibration_images)} training images; output: {args.output}", flush=True)
    quantize_static(
        str(args.model),
        str(args.output),
        SoccerCalibrationReader(calibration_images),
        quant_format=QuantFormat.QDQ,
        activation_type=QuantType.QUInt8,
        weight_type=QuantType.QInt8,
        calibrate_method=CalibrationMethod.MinMax,
        per_channel=True,
        op_types_to_quantize=["Conv", "MatMul"],
        extra_options={"ActivationSymmetric": False, "WeightSymmetric": True},
    )
    print(f"Quantized model written: {args.output.stat().st_size:,} bytes", flush=True)


if __name__ == "__main__":
    main()
