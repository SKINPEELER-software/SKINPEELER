#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "models/CTImage.h"
#include "config/Settings.h"

// =============================================================================
// PeelingService
// High-level peeling operations: auto mode and uniform (section-based) mode.
//
// peelAutoMode()/peelUniformMode() are intended to run on a background
// thread (see MainWindow::onPeelAllFiles) since they synchronously invoke
// CTImage::calculateShells()/peelSkin() for every slice — this can take
// minutes for a full volume. They do not touch Qt at all (engine stays
// GUI-independent); callers that need progress/cancellation marshal the
// provided callbacks back to the GUI thread themselves.
// =============================================================================
class PeelingService {
public:
    // Invoked after each slice finishes (success or per-slice failure) with
    // (completedSlices, totalSlices). May be called from a worker thread —
    // the callback implementation must marshal to the GUI thread itself if
    // it touches UI state (e.g. via QMetaObject::invokeMethod).
    using ProgressCallback = std::function<void(int completed, int total)>;

    // Polled between slices; returning true stops the loop at the next
    // slice boundary (slices already peeled remain peeled — no rollback).
    using CancelCallback = std::function<bool()>;

    // -------------------------------------------------------------------------
    // Auto mode: each slice calculates its own optimal thickness
    //   thicknessIncrement: +/- adjustment applied to every slice's auto index
    //   region           : which part of the cross-section to peel (default = WholeBody)
    //   onProgress       : optional, called after each slice
    //   isCancelled      : optional, polled before each slice
    // -------------------------------------------------------------------------
    static void peelAutoMode(std::vector<std::unique_ptr<CTImage>>& images,
                             int thicknessIncrement = 0, PeelRegion region = PeelRegion::WholeBody,
                             const ProgressCallback& onProgress = nullptr,
                             const CancelCallback& isCancelled = nullptr);

    // -------------------------------------------------------------------------
    // Uniform mode: user sets peel_index for N key sections;
    // intermediate slices are linearly interpolated.
    //   sections         : indices of key slices (sorted)
    //   sectionThicknesses: peel_index for each key slice
    //   region           : which part of the cross-section to peel (default = WholeBody)
    //   onProgress       : optional, called after each slice
    //   isCancelled      : optional, polled before each slice
    // -------------------------------------------------------------------------
    static void peelUniformMode(std::vector<std::unique_ptr<CTImage>>& images,
                                const std::vector<int>& sections,
                                const std::vector<int>& sectionThicknesses,
                                PeelRegion region = PeelRegion::WholeBody,
                                const ProgressCallback& onProgress = nullptr,
                                const CancelCallback& isCancelled = nullptr);

    // Half-peel (right-half only), auto mode
    static void halfPeelAutoMode(std::vector<std::unique_ptr<CTImage>>& images,
                                 int thicknessIncrement = 0);

    // Compute evenly-spaced section indices for numSections key slices
    static std::vector<int> computeSectionIndices(int totalImages, int numSections);

    // Linear interpolation of thickness across sections
    static std::vector<double> interpolateThickness(int totalImages,
                                                    const std::vector<int>& sections,
                                                    const std::vector<int>& thicknesses);
};
