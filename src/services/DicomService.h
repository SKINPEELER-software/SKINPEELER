#pragma once

#include <vector>
#include <string>
#include <memory>
#include "models/CTImage.h"

// =============================================================================
// DicomService
// Handles DICOM file I/O: finding, loading, normalising, and saving slices.
// =============================================================================
class DicomService {
public:
    // Find all .dcm files in a directory, sorted by trailing number (pre-sort)
    static std::vector<std::string> findDicomFiles(const std::string& directory);

    // Load all .dcm files from a directory into CTImage objects
    static std::vector<std::unique_ptr<CTImage>> loadDirectory(
        const std::string& directory,
        double beta             = DEFAULT_BETA,
        int    defaultThickness = DEFAULT_SHELL_THICKNESS);

    // Normalize intensities across all slices to a common maximum
    static void normalizeIntensities(std::vector<std::unique_ptr<CTImage>>& images);

    // Save all (peeled) images to outputDir with "peeled-" prefix
    // Returns number of files successfully saved
    static int saveDirectory(
        std::vector<std::unique_ptr<CTImage>>& images,
        const std::string& outputDirectory);

private:
    // Extract trailing integer from filename for sort (e.g. _042.dcm → 42)
    static int extractFileNumber(const std::string& filename);
};
