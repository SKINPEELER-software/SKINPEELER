#include "PeelingService.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// =============================================================================
// Auto mode
// =============================================================================
void PeelingService::peelAutoMode(
    std::vector<std::unique_ptr<CTImage>>& images,
    int thicknessIncrement,
    PeelRegion region,
    const ProgressCallback& onProgress,
    const CancelCallback& isCancelled)
{
    const int total = static_cast<int>(images.size());
    for (int i = 0; i < total; ++i) {
        if (isCancelled && isCancelled())
            break;

        auto& img = images[i];
        try {
            if (!img->hasShells())
                img->calculateShells();

            img->setPeelIndex(std::max(0, img->peelIndex() + thicknessIncrement));
            img->peelSkin(region);
            img->releaseVoronoi();
        } catch (const std::exception& e) {
            // Skip this slice, continue with the rest of the volume.
            std::fprintf(stderr, "PeelingService::peelAutoMode: skipping slice \"%s\": %s\n",
                         img->filepath().c_str(), e.what());
        }

        if (onProgress) onProgress(i + 1, total);
    }
}

// =============================================================================
// Uniform mode
// =============================================================================
void PeelingService::peelUniformMode(
    std::vector<std::unique_ptr<CTImage>>& images,
    const std::vector<int>& sections,
    const std::vector<int>& sectionThicknesses,
    PeelRegion region,
    const ProgressCallback& onProgress,
    const CancelCallback& isCancelled)
{
    auto thicknessList = interpolateThickness(
        static_cast<int>(images.size()), sections, sectionThicknesses);

    const int total = static_cast<int>(images.size());
    for (int i = 0; i < total; ++i) {
        if (isCancelled && isCancelled())
            break;

        auto& img = images[i];
        int t = static_cast<int>(std::round(thicknessList[i]));
        t = std::max(1, t);

        try {
            if (!img->hasShells())
                img->calculateShells();

            img->setPeelIndex(t);
            img->peelSkin(region);
            img->releaseVoronoi();
        } catch (const std::exception& e) {
            // Skip this slice, continue with the rest of the volume.
            std::fprintf(stderr, "PeelingService::peelUniformMode: skipping slice \"%s\": %s\n",
                         img->filepath().c_str(), e.what());
        }

        if (onProgress) onProgress(i + 1, total);
    }
}

// =============================================================================
// Half-peel auto mode
// =============================================================================
void PeelingService::halfPeelAutoMode(
    std::vector<std::unique_ptr<CTImage>>& images,
    int thicknessIncrement)
{
    for (auto& img : images) {
        try {
            if (!img->hasShells()) img->calculateShells();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "PeelingService::halfPeelAutoMode: skipping slice \"%s\": %s\n",
                         img->filepath().c_str(), e.what());
            continue;
        }
        img->setPeelIndex(std::max(0, img->peelIndex() + thicknessIncrement));
        img->halfPeelSkin();
        img->releaseVoronoi();
    }
}

// =============================================================================
// computeSectionIndices — evenly spaced key-slice indices
// =============================================================================
std::vector<int> PeelingService::computeSectionIndices(int totalImages, int numSections)
{
    numSections = std::min(numSections, totalImages);
    std::vector<int> indices;
    indices.reserve(numSections);

    double interval = static_cast<double>(totalImages) / static_cast<double>(numSections);
    for (int i = 0; i < numSections; ++i)
        indices.push_back(static_cast<int>(i * interval));

    return indices;
}

// =============================================================================
// interpolateThickness — linear interpolation between key sections
// Matches Python bisect-based implementation in skinPeeler.py
// =============================================================================
std::vector<double> PeelingService::interpolateThickness(
    int totalImages,
    const std::vector<int>& sections,
    const std::vector<int>& thicknesses)
{
    std::vector<double> result(totalImages);

    for (int i = 0; i < totalImages; ++i) {
        // Find position of i relative to sorted sections (upper_bound)
        auto it = std::upper_bound(sections.begin(), sections.end(), i);
        int pos = static_cast<int>(std::distance(sections.begin(), it));

        if (pos == 0) {
            result[i] = thicknesses.front();
        } else if (pos >= static_cast<int>(sections.size())) {
            result[i] = thicknesses.back();
        } else {
            int prevI = sections[pos - 1];
            int prevT = thicknesses[pos - 1];
            int nextI = sections[pos];
            int nextT = thicknesses[pos];

            double t = static_cast<double>(nextT - prevT) /
                       static_cast<double>(nextI - prevI) *
                       static_cast<double>(i - prevI) + prevT;
            result[i] = t;
        }
    }
    return result;
}
