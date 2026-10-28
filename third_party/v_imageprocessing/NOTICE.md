# V-ImageProcessing — Third-Party Binary Notice

This directory bundles a **prebuilt static library** (`lib/linux-x86_64/libV_2D.a`)
and its public C++ headers (`include/`) for **V-ImageProcessing**, a computational
geometry library (Voronoi diagram / beta-complex construction) developed and
owned by the **Voronoi Diagram Research Center, Hanyang University**.

## Status

- The library source code is **proprietary** and is **not** included in this
  repository. Only the headers required to compile against the library, and a
  prebuilt binary artifact, are provided here so that SKINPEELER can be built
  end-to-end without access to the private source repository.
- This binary/header bundle is **not covered by the MIT license** that applies
  to the rest of this repository (see top-level `LICENSE`). All rights to
  V-ImageProcessing remain with the Voronoi Diagram Research Center.
- Redistribution terms for this binary are being confirmed with the research
  center. **This notice is a draft pending that confirmation** — do not treat
  the mere presence of the binary in this repository as a redistribution
  grant. If you are affiliated with the research center and have concerns
  about this bundling, please contact the repository owner.

## Platform coverage

- `lib/linux-x86_64/libV_2D.a` — built with GCC, Release (`-O3 -DNDEBUG`),
  position-independent code, C++17.
- Windows (`V_2D.lib`, MSVC) is **not yet included** (TODO — see top-level
  README build notes).

## Building against this library

See the top-level `CMakeLists.txt`: by default it links the prebuilt static
library in this directory. An internal/lab build can instead point
`-DV_IP_SOURCE_ROOT=<path-to-PyMGOS-checkout>` at a private source checkout to
build V-ImageProcessing from source.
