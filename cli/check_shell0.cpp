// Quick check: print shell[0] vertex details for 1 DICOM file
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "models/CTImage.h"
#include "config/Settings.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) { cerr << "Usage: check_shell0 <dicom_file> [beta=0.69]\n"; return 1; }
    string filepath = argv[1];
    double beta = argc > 2 ? atof(argv[2]) : 0.69;

    cout << "Loading: " << filepath << ", beta=" << beta << "\n";

    auto img = CTImage::fromDicomFile(filepath, beta, 10);
    img.normalizeToMax(65535.0);
    img.calculateShells();

    const auto& shells = img.shells();
    if (shells.empty()) { cerr << "No shells\n"; return 1; }

    // Print shell 0~4 summary
    for (int s = 0; s < min(5, (int)shells.size()); ++s) {
        const auto& sh = shells[s];
        cout << "  Shell " << s << ": n=" << sh.size()
             << ", mu=" << sh.mu() << ", sigma=" << sh.sigma() << "\n";
    }

    // Shell 0 vertex details
    const auto& s0 = shells[0];
    const auto& verts = s0.vertices();
    cout << "\nShell 0 vertices (" << verts.size() << " total):\n";

    double sumR = 0, minR = 1e9, maxR = -1;
    int lowCount = 0, highCount = 0;
    for (const auto& v : verts) {
        sumR += v.radius;
        minR = min(minR, v.radius);
        maxR = max(maxR, v.radius);
        if (v.radius < 0.01) lowCount++;
        if (v.radius > 0.1) highCount++;
    }

    cout << "  Radius: min=" << minR << ", max=" << maxR
         << ", mean=" << sumR/verts.size() << "\n";
    cout << "  r < 0.01 (near-zero): " << lowCount << "/" << verts.size()
         << " (" << 100.0*lowCount/verts.size() << "%)\n";
    cout << "  r > 0.10 (tissue+):   " << highCount << "/" << verts.size()
         << " (" << 100.0*highCount/verts.size() << "%)\n";

    // Print first 10 vertices with coords
    cout << "\nSample vertices (x, y, radius):\n";
    for (int i = 0; i < min(10, (int)verts.size()); ++i) {
        cout << "  (" << verts[i].x << ", " << verts[i].y << ") r=" << verts[i].radius << "\n";
    }

    // Histogram of radius
    cout << "\nRadius histogram:\n";
    int bins[10] = {};
    for (const auto& v : verts) {
        int b = min(9, (int)(v.radius * 20));  // 0-0.05, 0.05-0.10, ..., 0.45-0.50
        bins[b]++;
    }
    for (int i = 0; i < 10; ++i) {
        double lo = i * 0.05, hi = (i+1) * 0.05;
        cout << "  [" << lo << ", " << hi << "): " << bins[i] << "\n";
    }

    return 0;
}
