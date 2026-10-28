#!/usr/bin/env python3
"""
make_phantom.py -- Synthetic CT phantom generator for SKINPEELER testing/demo.

Generates a small multi-slice synthetic CT series (a valid DICOM directory)
that exercises the same loading/windowing pipeline as real CT data, without
requiring any real, proprietary, or human/animal-subject imaging data.

This is a standalone, dependency-light tool: it is NOT part of the
SKINPEELER engine and does not call any engine or V-ImageProcessing code.
Its only purpose is to produce demo/test input that the SKINPEELER GUI or
CLI tools can load through their normal DICOM path.

Phantom design
--------------
Each slice contains three concentric regions, intended to resemble a cross
section through biological tissue scanned on a clinical CT system:

  - Background = air                  (HU approx. -1000)
  - Body interior = soft tissue        (HU approx. 20, with noise)
  - Outer shell = high-density "skin"  (HU approx. 700, with noise)

The body is an ellipse whose radii are tapered slice-to-slice with a smooth
sinusoidal profile, so that stacking all slices traces out a simple
torso-like 3D shape (narrower at the two ends of the stack, widest in the
middle). This gives the engine's shell-peeling pipeline a (toy) 3D structure
with topological continuity across slices, rather than N unrelated 2D disks.

DICOM encoding
--------------
  RescaleSlope=1, RescaleIntercept=0     (stored pixel value = HU directly)
  PixelRepresentation=1                  (signed int16, matches real CT)
  BitsAllocated=16, BitsStored=16, HighBit=15
  TransferSyntaxUID=1.2.840.10008.1.2    (Implicit VR Little Endian)
  WindowCenter/WindowWidth               (set so all three regions are
                                           visible in a single display window)

Dependencies
------------
  pydicom, numpy (both ordinary pip-installable packages; no SKINPEELER
  engine code, no DCMTK, no V-ImageProcessing / lab-internal code is used or
  required to run this script).

Usage
-----
  python3 tools/make_phantom.py -o sample_data/phantom -n 8
  python3 tools/make_phantom.py -o sample_data/phantom -n 8 --size 512 --seed 42

Output
------
  <output_dir>/PHANTOM_CT_0001.dcm ... PHANTOM_CT_000N.dcm
  (one DICOM file per slice, all sharing one SeriesInstanceUID)
"""

from __future__ import annotations

import argparse
import math
import os
import sys

import numpy as np
import pydicom
from pydicom.dataset import FileDataset
from pydicom.uid import ImplicitVRLittleEndian, generate_uid

# =============================================================================
# HU constants (approximate, chosen to resemble a real clinical CT scan)
# =============================================================================
HU_AIR       = -1000   # background
HU_BODY_MU   = 20      # soft-tissue interior mean HU
HU_BODY_SD   = 15      # soft-tissue interior HU std (noise)
HU_SKIN_MU   = 700     # outer high-density "skin" shell mean HU
HU_SKIN_SD   = 40      # outer shell HU std (noise)

# Display window: wide enough to show air, body and skin all at once.
WINDOW_CENTER = 0
WINDOW_WIDTH  = 2000

INT16_MIN = -32768
INT16_MAX = 32767


# =============================================================================
# Geometry helpers
# =============================================================================
def ellipse_mask(size: int, cx: float, cy: float, rx: float, ry: float) -> np.ndarray:
    """Boolean mask of a filled ellipse centered at (cx, cy)."""
    yy, xx = np.ogrid[:size, :size]
    return ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2 <= 1.0


def taper_scale(slice_index: int, num_slices: int,
                 min_scale: float = 0.4, max_scale: float = 1.0) -> float:
    """
    Smooth slice-to-slice taper factor in [min_scale, max_scale].
    Narrowest at the two ends of the stack (slice_index = 0 or num_slices-1),
    widest in the middle -- a simple stand-in for a tapering biological body
    (e.g. a torso) so that stacking all slices gives a toy 3D shape.
    """
    if num_slices <= 1:
        return max_scale
    t = (slice_index + 0.5) / num_slices  # in (0, 1)
    return min_scale + (max_scale - min_scale) * math.sin(math.pi * t)


# =============================================================================
# Per-slice image synthesis
# =============================================================================
def build_slice(size: int, skin_thickness: int, scale: float,
                 base_rx: float, base_ry: float,
                 rng: np.random.Generator) -> np.ndarray:
    """
    Build one int16 HU slice: air background + soft-tissue body + bright
    outer shell ("skin"). Returns an (size, size) int16 array.
    """
    cx = cy = size / 2.0
    rx = base_rx * scale
    ry = base_ry * scale

    hu = np.full((size, size), HU_AIR, dtype=np.int16)

    outer_mask = ellipse_mask(size, cx, cy, rx, ry)
    inner_mask = ellipse_mask(size, cx, cy,
                               max(rx - skin_thickness, 1.0),
                               max(ry - skin_thickness, 1.0))
    skin_mask = outer_mask & ~inner_mask
    body_mask = inner_mask

    n_body = int(body_mask.sum())
    if n_body > 0:
        hu[body_mask] = np.clip(
            rng.normal(HU_BODY_MU, HU_BODY_SD, n_body),
            INT16_MIN, INT16_MAX).astype(np.int16)

    n_skin = int(skin_mask.sum())
    if n_skin > 0:
        hu[skin_mask] = np.clip(
            rng.normal(HU_SKIN_MU, HU_SKIN_SD, n_skin),
            INT16_MIN, INT16_MAX).astype(np.int16)

    return hu


# =============================================================================
# DICOM writer
# =============================================================================
def write_phantom_dicom(hu_int16: np.ndarray, out_path: str,
                         series_uid: str, study_uid: str, sop_uid: str,
                         patient_id: str, instance_number: int,
                         slice_location: float,
                         slice_thickness: float,
                         size: int) -> None:
    """Write one int16 HU slice as a valid DICOM CT Image Storage file."""
    assert hu_int16.dtype == np.int16
    assert hu_int16.shape == (size, size)

    file_meta = pydicom.dataset.FileMetaDataset()
    file_meta.MediaStorageSOPClassUID = '1.2.840.10008.5.1.4.1.1.2'  # CT Image Storage
    file_meta.MediaStorageSOPInstanceUID = sop_uid
    file_meta.TransferSyntaxUID = ImplicitVRLittleEndian
    file_meta.ImplementationClassUID = generate_uid()

    ds = FileDataset(out_path, {}, file_meta=file_meta, preamble=b'\x00' * 128)

    # Patient / Study / Series identifiers.
    ds.PatientName = f'PHANTOM^{patient_id}'
    ds.PatientID = patient_id
    ds.StudyDate = '20261028'
    ds.StudyTime = '000000'
    ds.StudyInstanceUID = study_uid
    ds.SeriesInstanceUID = series_uid
    ds.SOPClassUID = '1.2.840.10008.5.1.4.1.1.2'
    ds.SOPInstanceUID = sop_uid
    ds.Modality = 'CT'
    ds.Manufacturer = 'SKINPEELER-synthetic-phantom'
    ds.StudyDescription = 'Synthetic phantom (no real patient/specimen data)'
    ds.SeriesDescription = 'SKINPEELER demo phantom'
    ds.InstanceNumber = instance_number

    # Image geometry.
    ds.Rows = size
    ds.Columns = size
    ds.PixelSpacing = [1.0, 1.0]
    ds.SliceThickness = slice_thickness
    ds.SliceLocation = slice_location
    ds.ImageOrientationPatient = [1, 0, 0, 0, 1, 0]
    ds.ImagePositionPatient = [0, 0, slice_location]

    # Pixel encoding -- signed int16, matching real clinical CT.
    ds.SamplesPerPixel = 1
    ds.PhotometricInterpretation = 'MONOCHROME2'
    ds.BitsAllocated = 16
    ds.BitsStored = 16
    ds.HighBit = 15
    ds.PixelRepresentation = 1  # signed

    # Rescale: stored value = HU directly (slope=1, intercept=0).
    ds.RescaleSlope = 1.0
    ds.RescaleIntercept = 0.0
    ds.RescaleType = 'HU'

    # Display window.
    ds.WindowCenter = float(WINDOW_CENTER)
    ds.WindowWidth = float(WINDOW_WIDTH)

    ds.PixelData = hu_int16.tobytes()
    ds['PixelData'].VR = 'OW'
    ds.is_implicit_VR = True
    ds.is_little_endian = True

    pydicom.dcmwrite(out_path, ds)


# =============================================================================
# Main
# =============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(
        description='Generate a synthetic CT phantom DICOM series for '
                    'SKINPEELER testing/demo (no real patient/specimen data).')
    parser.add_argument('-o', '--output', required=True,
                         help='Output directory for the DICOM series '
                              '(created if it does not exist).')
    parser.add_argument('-n', '--num-slices', type=int, default=8,
                         help='Number of slices to generate (default: 8).')
    parser.add_argument('--size', type=int, default=512,
                         help='Slice width/height in pixels (default: 512).')
    parser.add_argument('--skin-thickness', type=int, default=14,
                         help='Outer "skin" shell thickness in pixels '
                              '(default: 14).')
    parser.add_argument('--slice-thickness', type=float, default=2.0,
                         help='DICOM SliceThickness / slice spacing in mm '
                              '(default: 2.0).')
    parser.add_argument('--patient-id', default='PHANTOM01',
                         help='Synthetic PatientID / PatientName tag '
                              '(default: PHANTOM01).')
    parser.add_argument('--seed', type=int, default=42,
                         help='Random seed for reproducible noise '
                              '(default: 42).')
    args = parser.parse_args()

    if args.num_slices < 1:
        print('ERROR: --num-slices must be >= 1', file=sys.stderr)
        return 1

    os.makedirs(args.output, exist_ok=True)

    rng = np.random.default_rng(seed=args.seed)
    series_uid = generate_uid()
    study_uid = generate_uid()

    # Base ellipse radii at full taper scale (max_scale=1.0), leaving a
    # margin from the image border.
    base_rx = args.size * 0.34
    base_ry = args.size * 0.30

    print(f'Generating {args.num_slices} synthetic phantom slice(s) '
          f'({args.size}x{args.size}) -> {args.output}')

    for k in range(args.num_slices):
        scale = taper_scale(k, args.num_slices)
        hu = build_slice(args.size, args.skin_thickness, scale,
                          base_rx, base_ry, rng)

        instance_number = k + 1
        slice_location = k * args.slice_thickness
        sop_uid = generate_uid()
        out_path = os.path.join(args.output, f'PHANTOM_CT_{instance_number:04d}.dcm')

        write_phantom_dicom(hu, out_path, series_uid, study_uid, sop_uid,
                             args.patient_id, instance_number, slice_location,
                             args.slice_thickness, args.size)

        # Round-trip verification: re-read and confirm pixel values survive
        # the DICOM write exactly (catches encoding mistakes immediately).
        back = pydicom.dcmread(out_path)
        pa_back = back.pixel_array.astype(np.int16)
        if not np.array_equal(pa_back, hu):
            print(f'ERROR: DICOM round-trip mismatch for {out_path}', file=sys.stderr)
            return 1

        n_skin = int((hu >= (HU_BODY_MU + HU_SKIN_MU) / 2).sum())
        print(f'  slice {instance_number:2d}/{args.num_slices}  '
              f'taper_scale={scale:.3f}  skin_px~{n_skin:6d}  -> {out_path}')

    print(f'\nDone. {args.num_slices} DICOM file(s) written to {args.output}')
    print(f'  Series UID : {series_uid}')
    print(f'  Window     : WC={WINDOW_CENTER}, WW={WINDOW_WIDTH}')
    print(f'  HU regions : air~{HU_AIR}, body~{HU_BODY_MU}, skin~{HU_SKIN_MU}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
