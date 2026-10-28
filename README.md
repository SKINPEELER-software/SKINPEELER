# SKINPEELER

Voronoi diagram–based iterative shell peeling for high-density boundary
segmentation in biological CT data. C++17 / Qt desktop application.

> This README is a temporary stub. A full README (features, screenshots,
> algorithm overview, architecture diagram) will replace this file.

## Build

```bash
cmake -B build
cmake --build build -j$(nproc)
```

See `CMakeLists.txt` for dependency requirements (Qt 6 or Qt 5 >= 5.13,
DCMTK) and the bundled V-ImageProcessing third-party library notes under
`third_party/v_imageprocessing/NOTICE.md`.
