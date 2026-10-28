#include "BedCache.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// =============================================================================
// Internal helper: minimal JSON parser/writer
//
// Only handles the fixed schema used by BedCache. Not a general-purpose parser.
// All parsing is line-by-line; no recursive descent needed for this schema.
// =============================================================================

namespace {

// ---- Writer helpers --------------------------------------------------------

static std::string intVecToJson(const std::vector<int>& v) {
    std::string s = "[";
    for (int i = 0; i < static_cast<int>(v.size()); ++i) {
        if (i > 0) s += ",";
        s += std::to_string(v[i]);
    }
    s += "]";
    return s;
}

static std::string intVecToJson(const std::vector<int>& v,
                                 size_t start, size_t count) {
    std::string s = "[";
    for (size_t i = start; i < start + count && i < v.size(); ++i) {
        if (i > start) s += ",";
        s += std::to_string(v[i]);
    }
    s += "]";
    return s;
}

// Format beta to 3 decimal places
static std::string betaToStr(double beta) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", beta);
    return buf;
}

// Escape a string for JSON (handles backslash and double-quote)
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"') { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else { out += c; }
    }
    return out;
}

// ---- Parser helpers --------------------------------------------------------

// Trim whitespace from both ends
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Parse a JSON integer array like [1,2,3] into a vector<int>
static std::vector<int> parseIntArray(const std::string& s) {
    std::vector<int> result;
    size_t start = s.find('[');
    size_t end   = s.find(']');
    if (start == std::string::npos || end == std::string::npos || end <= start + 1)
        return result;

    std::string inner = s.substr(start + 1, end - start - 1);
    std::istringstream ss(inner);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) {
            try {
                result.push_back(std::stoi(token));
            } catch (...) {
                std::fprintf(stderr,
                    "BedCache: skipping malformed integer token in cache file: \"%s\"\n",
                    token.c_str());
            }
        }
    }
    return result;
}

// Parse a JSON string value:  "key": "value"  → returns value (unescaped)
static bool parseStringValue(const std::string& line, const std::string& key,
                               std::string& out) {
    // Look for "key": "..."
    std::string needle = "\"" + key + "\"";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) return false;
    size_t colon = line.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;
    size_t q1 = line.find('"', colon + 1);
    if (q1 == std::string::npos) return false;
    size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;
    out = line.substr(q1 + 1, q2 - q1 - 1);
    return true;
}

// Parse "key": <number>  → fills out
static bool parseNumberValue(const std::string& line, const std::string& key,
                               double& out) {
    std::string needle = "\"" + key + "\"";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) return false;
    size_t colon = line.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;
    // find first digit, '-', or '.'
    size_t numStart = line.find_first_not_of(" \t", colon + 1);
    if (numStart == std::string::npos) return false;
    try {
        size_t consumed = 0;
        out = std::stod(line.substr(numStart), &consumed);
        return consumed > 0;
    } catch (...) {
        std::fprintf(stderr,
            "BedCache: skipping malformed number for key \"%s\" in cache file\n",
            key.c_str());
        return false;
    }
}

static bool parseIntValue(const std::string& line, const std::string& key,
                            int& out) {
    double d;
    if (!parseNumberValue(line, key, d)) return false;
    out = static_cast<int>(d);
    return true;
}

// Parse "key": true|false
static bool parseBoolValue(const std::string& line, const std::string& key,
                             bool& out) {
    std::string needle = "\"" + key + "\"";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) return false;
    size_t colon = line.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;
    size_t v = line.find_first_not_of(" \t", colon + 1);
    if (v == std::string::npos) return false;
    if (line.substr(v, 4) == "true")  { out = true;  return true; }
    if (line.substr(v, 5) == "false") { out = false; return true; }
    return false;
}

// ---- Slice record struct (internal) ----------------------------------------

struct SliceRecord {
    std::string filepath;       // basename only
    bool        detected       = false;
    int         n_bed_cands    = 0;
    std::vector<int> bed_ids;
    int         new_body0_id   = -1;
    std::vector<int> envelope_y;
    std::vector<int> bottom_y;  // column-wise bottom y (-1 = no bed)
};

// ---- Write a single slice record -------------------------------------------
static std::string serializeSlice(const SliceRecord& sr) {
    std::string s;
    s += "    {\n";
    s += "      \"filepath\": \""    + jsonEscape(sr.filepath) + "\",\n";
    s += "      \"detected\": "       + std::string(sr.detected ? "true" : "false") + ",\n";
    s += "      \"n_bed_candidates\": " + std::to_string(sr.n_bed_cands) + ",\n";
    s += "      \"bed_component_ids\": " + intVecToJson(sr.bed_ids) + ",\n";
    s += "      \"new_body0_component_id\": " + std::to_string(sr.new_body0_id) + ",\n";
    s += "      \"envelope_y\": " + intVecToJson(sr.envelope_y) + ",\n";
    s += "      \"bottom_y\": " + intVecToJson(sr.bottom_y) + "\n";
    s += "    }";
    return s;
}

// ---- Convert BedDetector::Result → SliceRecord ---------------------------
static SliceRecord resultToRecord(const std::string& basename,
                                   const BedDetector::Result& result)
{
    SliceRecord sr;
    sr.filepath     = basename;
    sr.detected     = result.detected;
    sr.n_bed_cands  = static_cast<int>(result.bed_candidates.size());
    for (const auto& cand : result.bed_candidates)
        sr.bed_ids.push_back(cand.component_id);
    sr.new_body0_id = result.new_body0_component_id;
    sr.envelope_y   = result.envelope.y_at_col;
    sr.bottom_y     = result.envelope.bottom_y_at_col;
    return sr;
}

// ---- Convert SliceRecord → BedDetector::Result (minimal fields) -----------
static BedDetector::Result recordToResult(const SliceRecord& sr) {
    BedDetector::Result result;
    result.detected     = sr.detected;
    result.n_layers     = sr.n_bed_cands;
    result.new_body0_component_id = sr.new_body0_id;

    // Restore bed_candidates (id only; metrics not cached)
    for (int id : sr.bed_ids) {
        BedDetector::BedCandidate cand;
        cand.component_id = id;
        result.bed_candidates.push_back(cand);
    }

    // Restore envelope
    result.envelope.y_at_col = sr.envelope_y;
    result.envelope.width    = static_cast<int>(sr.envelope_y.size());
    result.envelope.valid    = sr.detected;

    // Restore bottom_y; if the field is absent from the cache file, fill
    // with -1 to match width so callers can safely index.
    if (!sr.bottom_y.empty()) {
        result.envelope.bottom_y_at_col = sr.bottom_y;
    } else {
        result.envelope.bottom_y_at_col.assign(result.envelope.width, -1);
    }

    return result;
}

// ---- Read all slice records from an existing cache file --------------------
static bool readCacheFile(const std::string& path, double& file_beta,
                           int& file_version, std::string& file_specimen_dir,
                           std::vector<SliceRecord>& records)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    records.clear();
    file_beta = 0.0;
    file_version = 0;
    file_specimen_dir.clear();

    std::string line;
    bool in_slices = false;
    bool in_record = false;
    SliceRecord current;
    int brace_depth = 0;  // depth within slices array records

    while (std::getline(f, line)) {
        std::string t = trim(line);

        // Top-level fields
        if (!in_slices) {
            parseNumberValue(t, "beta",         file_beta);
            parseIntValue(t,    "version",      file_version);
            parseStringValue(t, "specimen_dir", file_specimen_dir);
            if (t.find("\"slices\"") != std::string::npos)
                in_slices = true;
            continue;
        }

        // Inside slices array
        if (!in_record) {
            if (t.find('{') != std::string::npos) {
                in_record = true;
                brace_depth = 1;
                current = SliceRecord{};
            }
            continue;
        }

        // Inside a record
        for (char c : t) {
            if (c == '{') ++brace_depth;
            else if (c == '}') --brace_depth;
        }

        // Parse fields within the record
        parseStringValue(t, "filepath",               current.filepath);
        parseBoolValue(t,   "detected",                current.detected);
        parseIntValue(t,    "n_bed_candidates",        current.n_bed_cands);
        parseIntValue(t,    "new_body0_component_id",  current.new_body0_id);

        if (t.find("\"bed_component_ids\"") != std::string::npos)
            current.bed_ids = parseIntArray(t.substr(t.find("\"bed_component_ids\"")));
        if (t.find("\"envelope_y\"") != std::string::npos)
            current.envelope_y = parseIntArray(t.substr(t.find("\"envelope_y\"")));
        if (t.find("\"bottom_y\"") != std::string::npos)
            current.bottom_y = parseIntArray(t.substr(t.find("\"bottom_y\"")));

        if (brace_depth <= 0) {
            records.push_back(current);
            in_record = false;
        }
    }

    return true;
}

// ---- Write all slice records to a cache file --------------------------------
static void writeCacheFile(const std::string& path, double beta, int version,
                            const std::string& specimen_dir,
                            const std::vector<SliceRecord>& records)
{
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("BedCache: cannot open for write: " + path);

    f << "{\n";
    f << "  \"version\": " << version << ",\n";
    f << "  \"beta\": "    << betaToStr(beta) << ",\n";
    f << "  \"specimen_dir\": \"" << jsonEscape(specimen_dir) << "\",\n";
    f << "  \"slices\": [\n";

    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        f << serializeSlice(records[i]);
        if (i + 1 < static_cast<int>(records.size())) f << ",";
        f << "\n";
    }

    f << "  ]\n";
    f << "}\n";
}

}  // anonymous namespace

// =============================================================================
// BedCache public API
// =============================================================================

std::string BedCache::makeCachePath(const std::string& specimen_dir,
                                     double beta, int version)
{
    char betaBuf[32];
    std::snprintf(betaBuf, sizeof(betaBuf), "%.3f", beta);

    fs::path dir = fs::path(specimen_dir) / ".skinpeeler";
    std::string filename = std::string("bed_cache_beta") + betaBuf
                         + "_v" + std::to_string(version) + ".json";
    return (dir / filename).string();
}

std::string BedCache::makeBasename(const std::string& filepath) {
    return fs::path(filepath).filename().string();
}

bool BedCache::exists(const std::string& specimen_dir, double beta, int version) {
    std::string path = makeCachePath(specimen_dir, beta, version);
    return fs::exists(fs::path(path));
}

bool BedCache::load(const std::string& specimen_dir, double beta, int version,
                    const std::string& slice_filepath,
                    BedDetector::Result& result_out)
{
    std::string path = makeCachePath(specimen_dir, beta, version);
    if (!fs::exists(fs::path(path))) return false;

    double file_beta = 0.0;
    int    file_ver  = 0;
    std::string file_specimen;
    std::vector<SliceRecord> records;

    if (!readCacheFile(path, file_beta, file_ver, file_specimen, records))
        return false;

    // Basic validation
    if (file_ver != version) return false;

    std::string basename = makeBasename(slice_filepath);

    for (const auto& sr : records) {
        if (sr.filepath == basename) {
            result_out = recordToResult(sr);
            return true;
        }
    }
    return false;  // slice not found in cache
}

void BedCache::save(const std::string& specimen_dir, double beta, int version,
                    const std::string& slice_filepath,
                    const BedDetector::Result& result)
{
    std::string path = makeCachePath(specimen_dir, beta, version);

    // Ensure .skinpeeler/ directory exists
    fs::path cache_dir = fs::path(path).parent_path();
    if (!fs::exists(cache_dir)) {
        std::error_code ec;
        fs::create_directories(cache_dir, ec);
        if (ec) {
            std::fprintf(stderr, "[BedCache] Warning: cannot create dir %s: %s\n",
                         cache_dir.string().c_str(), ec.message().c_str());
            return;
        }
    }

    // Read existing records
    double file_beta = 0.0;
    int    file_ver  = 0;
    std::string file_specimen;
    std::vector<SliceRecord> records;

    if (fs::exists(fs::path(path))) {
        readCacheFile(path, file_beta, file_ver, file_specimen, records);
    }

    // Upsert: replace or append
    std::string basename = makeBasename(slice_filepath);
    SliceRecord newRec   = resultToRecord(basename, result);

    bool found = false;
    for (auto& sr : records) {
        if (sr.filepath == basename) {
            sr    = newRec;
            found = true;
            break;
        }
    }
    if (!found) records.push_back(newRec);

    writeCacheFile(path, beta, version, specimen_dir, records);
}
