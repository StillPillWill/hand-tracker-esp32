from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

import torch
import yaml
from ultralytics import YOLO
import ultralytics.utils.callbacks.wb as wandb_callbacks


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATASET = REPO_ROOT / "work" / "soccer-model" / "Football-Image-Dataset-main"
OUTPUT_DIR = REPO_ROOT / "work" / "soccer-model" / "training"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train and export the soccer detector baseline.")
    parser.add_argument("--data", type=Path, default=DEFAULT_DATASET / "data.yaml",
                        help="Path to a YOLO detection dataset YAML.")
    parser.add_argument("--weights", default="yolo11n.pt",
                        help="Ultralytics model checkpoint to fine-tune.")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--imgsz", type=int, default=1280)
    parser.add_argument("--batch", type=int, default=4)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--device", default=None,
                        help="Ultralytics device selector; defaults to CUDA when available.")
    return parser.parse_args()


def resolve_dataset_yaml(source: Path) -> Path:
    source = source.resolve()
    if not source.is_file():
        raise FileNotFoundError(f"Dataset YAML does not exist: {source}")
    config = yaml.safe_load(source.read_text(encoding="utf-8"))
    dataset_root = Path(config.get("path") or source.parent)
    if not dataset_root.is_absolute():
        dataset_root = (source.parent / dataset_root).resolve()
    config["path"] = str(dataset_root)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    local_yaml = OUTPUT_DIR / "dataset.yaml"
    local_yaml.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")
    return local_yaml


def main() -> None:
    args = parse_args()
    dataset_yaml = resolve_dataset_yaml(args.data)
    device = args.device if args.device is not None else ("0" if torch.cuda.is_available() else "cpu")
    weights_path = Path(args.weights)
    weights = str(weights_path.resolve()) if weights_path.is_file() else args.weights
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    os.chdir(OUTPUT_DIR)
    model = YOLO(weights)
    wandb_callbacks.callbacks.clear()
    model.train(
        data=str(dataset_yaml),
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        workers=args.workers,
        device=device,
        seed=42,
        deterministic=True,
        patience=20,
        cache="disk",
        project=str(OUTPUT_DIR),
        name="yolo11n-soccer",
        exist_ok=True,
        plots=True,
    )

    best_checkpoint = OUTPUT_DIR / "yolo11n-soccer" / "weights" / "best.pt"
    best_model = YOLO(str(best_checkpoint))
    metrics = best_model.val(
        data=str(dataset_yaml), imgsz=args.imgsz, device=device, split="val",
        project=str(OUTPUT_DIR), name="yolo11n-soccer-validation", exist_ok=True,
    )
    per_class_metrics = {}
    for metric_index, class_index in enumerate(metrics.box.ap_class_index):
        precision, recall, map50, map50_95 = metrics.box.class_result(metric_index)
        class_name = best_model.names[int(class_index)]
        per_class_metrics[class_name] = {
            "precision": float(precision),
            "recall": float(recall),
            "mAP50": float(map50),
            "mAP50_95": float(map50_95),
        }
    summary = {
        "checkpoint": str(best_checkpoint),
        "device": str(device),
        "imgsz": args.imgsz,
        "metrics": {key: float(value) for key, value in metrics.results_dict.items()},
        "perClassMetrics": per_class_metrics,
    }
    (OUTPUT_DIR / "validation-metrics.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    best_model.export(format="onnx", imgsz=args.imgsz, dynamic=False, simplify=False, opset=17)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
