# Soccer detector training

This folder defines a reproducible baseline training run. It does not claim that the model is ready for a full match: the source dataset contains broadcast-view frames, while the intended camera is a fixed iPhone on the sideline.

## Dataset

The baseline uses the [Football Image Dataset](https://github.com/skaczylo/Football-Image-Dataset), released under CC BY 4.0. It contains 612 1920×1080 frames: 512 train and 100 validation, with 500 labeled balls total (417 train, 83 validation), plus player and referee boxes. Credit the dataset author, skaczylo, and include the CC BY 4.0 license when redistributing adapted data or derived assets.

The dataset is downloaded and expanded under work/soccer-model, which is ignored by Git. It is not bundled in this repository. The upstream dataset's validation split is useful for checking that training runs, but it is too small and may not represent independent matches or the sideline viewpoint.

## Train a baseline

Python 3.10 or 3.11 and an NVIDIA GPU with CUDA are recommended. CPU training is supported but slow.

Run python -m pip install -r model/requirements.txt, then run python model/train_soccer.py from the repository root.

The script fine-tunes YOLO11n at 1280 pixels, evaluates the validation split, and exports a fixed-shape ONNX model. Outputs stay in work/soccer-model/training and are not committed automatically.

To use a different dataset, pass --data C:\path\to\sideline-soccer\data.yaml to the training script.

For a deployable model, collect video from the exact iPhone, mount position, pitch size, and shooting distance. Sample frames across multiple matches and lighting conditions; annotate ball, players, and referee. Split by entire match and camera position before training so near-duplicate frames do not appear in both train and validation. Inspect ball recall and false locks at the expected distance, then tune confidence and temporal tracking against held-out games.

## Load the model in the iPhone page

After training, the fixed-shape ONNX file is written beside `best.pt` under `work/soccer-model/training/yolo11n-soccer/weights/`. Copy that `.onnx` file into iPhone Files (for example, through iCloud Drive or USB transfer), open the ESP32 page in Safari, select the file under **Soccer model**, and tap **Load soccer model** before starting the camera. The page runs inference on the phone and keeps the model bytes there; the ESP32 receives only aim and motor commands.

The browser decoder expects the YOLO output for a 1280×1280 input with classes in this exact order: `ball`, `player`, `referee`. The current baseline uses confidence thresholds of 0.26 for balls and 0.18 for players/referees. It requests the WebNN NPU device if the browser exposes it, then tries ONNX Runtime WebGPU with WASM fallback, then WASM CPU. The generic MediaPipe COCO detector remains available as the default. The runtime downloads from jsDelivr, so the phone needs an internet route while loading it.

## Baseline run and browser check

The 2026-10-11 YOLO11n run stopped after 40 epochs and exported a 10.6 MB ONNX model. On the 100-image broadcast validation split, aggregate validation was precision 0.747, recall 0.763, mAP50 0.763, and mAP50-95 0.496. A separate check loaded that ONNX file in ONNX Runtime WebGPU and ran the browser's 1280 preprocessing, 0.45 class-wise NMS, and box decoder against all 100 validation images. At IoU 0.50, using confidence thresholds of 0.26 for ball and 0.18 for players/referees, precision/recall was ball 0.485/0.590, player 0.948/0.983, referee 0.813/0.715. This is a runtime/decoder check on broadcast images, not a sideline test or a full mAP benchmark.

The ONNX export metadata identifies the upstream Ultralytics license as AGPL-3.0. Its warm WebGPU inference took 66–106 ms on the local NVIDIA Pascal desktop; that timing does not predict iPhone performance. Measure the target phone's runtime and capture settings at the field.

The baseline's validation metrics come from broadcast frames, not the intended iPhone sideline viewpoint. Do not treat those scores as proof of sideline recall. Measure ball recall and false locks from held-out games recorded at the actual mount position before relying on automatic tracking.
