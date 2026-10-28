#include "DicomService.h"

#include <dcmtk/dcmdata/dctk.h>

#include <filesystem>
#include <regex>

#include <algorithm>
#include <cstdio>
#include <stdexcept>

// =============================================================================
// findDicomFiles — returns sorted list of .dcm paths
// =============================================================================
std::vector<std::string> DicomService::findDicomFiles(const std::string& directory)
{
    namespace fs = std::filesystem;
    std::vector<std::string> paths;

    if (!fs::is_directory(directory)) return paths;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        // case-insensitive .dcm check
        std::string extLower = ext;
        for (char& c : extLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (extLower == ".dcm")
            paths.push_back(entry.path().string());
    }

    std::stable_sort(paths.begin(), paths.end(),
        [](const std::string& a, const std::string& b) {
            return extractFileNumber(a) < extractFileNumber(b);
        });

    return paths;
}

// =============================================================================
// loadDirectory
// =============================================================================
std::vector<std::unique_ptr<CTImage>> DicomService::loadDirectory(
    const std::string& directory,
    double beta,
    int    defaultThickness)
{
    auto paths = findDicomFiles(directory);

    std::vector<std::unique_ptr<CTImage>> images;
    images.reserve(paths.size());

    for (const auto& path : paths) {
        try {
            images.push_back(
                std::make_unique<CTImage>(CTImage::fromDicomFile(path, beta, defaultThickness)));
        } catch (const std::exception& e) {
            // Skip invalid files (matching Python behaviour)
            std::fprintf(stderr, "DicomService: skipping invalid file \"%s\": %s\n",
                         path.c_str(), e.what());
        }
    }

    // Sort by DICOM SliceLocation (or InstanceNumber fallback)
    std::stable_sort(images.begin(), images.end(),
        [](const std::unique_ptr<CTImage>& a, const std::unique_ptr<CTImage>& b) {
            return a->sliceLocation() < b->sliceLocation();
        });

    normalizeIntensities(images);
    return images;
}

// =============================================================================
// normalizeIntensities — global max across all slices
// =============================================================================
void DicomService::normalizeIntensities(std::vector<std::unique_ptr<CTImage>>& images)
{
    if (images.empty()) return;

    double globalMax = 0.0;
    for (const auto& img : images)
        globalMax = std::max(globalMax, img->imageMax());

    if (globalMax <= 0.0) globalMax = 1.0;

    for (auto& img : images)
        img->normalizeToMax(globalMax);
}

// =============================================================================
// saveDirectory
// =============================================================================
int DicomService::saveDirectory(
    std::vector<std::unique_ptr<CTImage>>& images,
    const std::string& outputDirectory)
{
    namespace fs = std::filesystem;
    fs::create_directories(outputDirectory);

    int saved = 0;
    for (auto& img : images) {
        try {
            img->applyPeeledToDicom();

            // applyPeeledToDicom() always writes back uncompressed pixel data
            // (DicomService.cpp loads JPEG-compressed input via DicomImage and
            // only ever re-inserts a native uint16 array — see CTImage.cpp).
            // If the original file's transfer syntax was compressed (e.g. JPEG
            // Lossless), the meta header would otherwise still claim that
            // compressed syntax while the payload is plain uncompressed pixels,
            // producing a file that readers/viewers cannot correctly decode.
            // Force both the dataset representation and the written transfer
            // syntax to Explicit VR Little Endian so meta header and payload
            // agree.
            DcmFileFormat* ff = img->dcmFileFormat();
            ff->chooseRepresentation(EXS_LittleEndianExplicit, nullptr);

            fs::path srcPath(img->filepath());
            fs::path outPath = fs::path(outputDirectory) /
                               (std::string(PEELED_FILE_PREFIX) + srcPath.filename().string());

            OFCondition status =
                ff->saveFile(outPath.string().c_str(), EXS_LittleEndianExplicit);
            if (status.good()) ++saved;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "DicomService: failed to save \"%s\": %s\n",
                         img->filepath().c_str(), e.what());
        } catch (...) {
            std::fprintf(stderr, "DicomService: failed to save \"%s\": unknown error\n",
                         img->filepath().c_str());
        }
    }
    return saved;
}

// =============================================================================
// extractFileNumber  — parse trailing _NNN from filename
// =============================================================================
int DicomService::extractFileNumber(const std::string& filename)
{
    namespace fs = std::filesystem;
    std::string fname = fs::path(filename).filename().string();

    // Primary: _NNN.dcm
    std::regex re1(R"(_(\d+)\.dcm$)", std::regex_constants::icase);
    std::smatch m;
    if (std::regex_search(fname, m, re1)) return std::stoi(m[1].str());

    // Fallback: NNN.dcm
    std::regex re2(R"((\d+)\.dcm$)", std::regex_constants::icase);
    if (std::regex_search(fname, m, re2)) return std::stoi(m[1].str());

    return 0;
}
