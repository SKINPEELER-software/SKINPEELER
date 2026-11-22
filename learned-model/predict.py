"""Segment the high-density boundary layer in a DICOM series with the trained U-Net.

usage:
    python predict.py <dicom_dir> <out_dir> --weights skinpeeler_unet_fold1.pt [more.pt ...] [--device cuda|cpu]

For every readable slice a binary mask <stem>_skin.png (255 = boundary layer) is written to <out_dir>.
With several weight files the sigmoid outputs are averaged before thresholding at 0.5.
The model was trained on 512 x 512 slices; other sizes must be multiples of 16.
"""
import argparse, os, sys
import numpy as np, pydicom, torch
from PIL import Image
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "src"))
from models import build_model


def load_hu(path):
    ds = pydicom.dcmread(path)
    return ds.pixel_array.astype(np.float32) * float(getattr(ds, "RescaleSlope", 1)) + float(getattr(ds, "RescaleIntercept", 0))


def load_models(paths, device):
    models = []
    for p in paths:
        ck = torch.load(p, map_location=device, weights_only=False)
        m = build_model(ck["config"]).to(device).eval()
        m.load_state_dict(ck["state_dict"])
        models.append((m, ck["config"]))
    return models


@torch.no_grad()
def predict_slice(models, hu, device):
    prob = 0.0
    for m, cfg in models:
        x = np.clip((hu - cfg["hu_min"]) / (cfg["hu_max"] - cfg["hu_min"]), 0, 1)
        x = torch.from_numpy(x)[None, None].to(device)
        with torch.autocast(device, dtype=torch.float16, enabled=device == "cuda"):
            prob = prob + torch.sigmoid(m(x)[0, 0].float())
    return (prob / len(models) > 0.5).cpu().numpy()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dicom_dir"); ap.add_argument("out_dir")
    ap.add_argument("--weights", nargs="+", required=True)
    ap.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)
    models = load_models(a.weights, a.device)
    done = 0
    for name in sorted(os.listdir(a.dicom_dir)):
        try:
            hu = load_hu(os.path.join(a.dicom_dir, name))
        except Exception as e:                                   # not a DICOM image: skip and say so
            print(f"skip {name}: {type(e).__name__}"); continue
        mask = predict_slice(models, hu, a.device)
        Image.fromarray(mask.astype(np.uint8) * 255).save(os.path.join(a.out_dir, os.path.splitext(name)[0] + "_skin.png"))
        done += 1
    print(f"wrote {done} masks to {a.out_dir}")


if __name__ == "__main__":
    main()
