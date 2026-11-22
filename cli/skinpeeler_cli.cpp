// =============================================================================
// skinpeeler_cli — batch command-line program for the SKINPEELER engine
//
//   skinpeeler_cli shells <dicom_dir> --out <dir>
//       (--beta B | --beta-range MIN MAX STEP)
//       [--range START:END] [--masks] [--bed-removal on|off] [--overwrite]
//   skinpeeler_cli peel <dicom_dir> --out <dir> --beta B [--overwrite]
//   skinpeeler_cli select-beta <sweep_out_dir>... --rule RULE|all
//       [--valid START:END] [--candidates MIN MAX] [--param NAME=VALUE]...
//       [--out <dir>] [--overwrite]
//   skinpeeler_cli --version | --help
//
// UI layer only: argument parsing, progress lines, CSV / PNG / JSON writing.
// Every computed value comes from the engine (skinpeeler::Session, CTImage).
// No Qt.
//
// Exit codes: 0 = all slices ok, 1 = finished but at least one slice failed,
//             2 = usage error or fatal error (outputs stay as *.part).
// =============================================================================

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "config/Settings.h"
#include "engine/Session.h"
#include "models/BetaSelection.h"
#include "models/CTImage.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#ifndef SKINPEELER_GIT_COMMIT
#define SKINPEELER_GIT_COMMIT "unknown"
#endif
#ifndef SKINPEELER_GIT_DIRTY
#define SKINPEELER_GIT_DIRTY "unknown"
#endif
#ifndef SKINPEELER_V_GIT_COMMIT
#define SKINPEELER_V_GIT_COMMIT "unknown"
#endif
#ifndef SKINPEELER_V_GIT_DIRTY
#define SKINPEELER_V_GIT_DIRTY "unknown"
#endif

namespace fs = std::filesystem;
using skinpeeler::Session;

namespace {

constexpr int EXIT_OK          = 0;
constexpr int EXIT_SLICE_FAIL  = 1;
constexpr int EXIT_FATAL       = 2;

struct UsageError : std::runtime_error { using std::runtime_error::runtime_error; };
struct FatalError : std::runtime_error { using std::runtime_error::runtime_error; };

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------
std::string fmt_double(double v)
{
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

// Shortest decimal text that parses back to exactly the same double
// (0.66 → "0.66", not "0.66000000000000003").
std::string shortest_double(double v)
{
    char buf[40];
    for (int prec = 1; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    return buf;
}

// Directory label for a beta: two decimals when that is exact ("0.70"),
// otherwise the shortest exact text ("0.665").
std::string beta_dir_label(double beta)
{
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.2f", beta);
    if (std::strtod(buf, nullptr) == beta) return buf;
    return shortest_double(beta);
}

std::string csv_field(const std::string& s)
{
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else if (c == '\r' || c == '\n') out += ' ';
        else out += c;
    }
    out += '"';
    return out;
}

std::string json_string(const std::string& s)
{
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

// JSON has no inf/nan literals: non-finite numbers are written as strings.
std::string json_number(double v)
{
    if (std::isfinite(v)) return fmt_double(v);
    return json_string(fmt_double(v));
}

const char* json_bool(bool b) { return b ? "true" : "false"; }

std::string utc_now_iso8601()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    return buf;
}

// Resident set size from /proc/self/status, in MB. -1 where unavailable
// (non-Linux builds).
double read_proc_status_mb(const char* key)
{
#if defined(__linux__)
    std::ifstream in("/proc/self/status");
    std::string line;
    const std::string prefix = std::string(key) + ":";
    while (std::getline(in, line)) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            const double kb = std::strtod(line.c_str() + prefix.size(), nullptr);
            return kb / 1024.0;
        }
    }
    return -1.0;
#else
    (void)key;
    return -1.0;
#endif
}

double current_rss_mb() { return read_proc_status_mb("VmRSS"); }
double peak_rss_mb()    { return read_proc_status_mb("VmHWM"); }

std::string rss_text(double mb)
{
    if (mb < 0) return "n/a";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0fMB", mb);
    return buf;
}

double seconds_since(const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

double parse_double(const std::string& text, const char* what)
{
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (text.empty() || end == nullptr || *end != '\0' || !std::isfinite(v))
        throw UsageError(std::string("invalid number for ") + what + ": '" + text + "'");
    return v;
}

int parse_int(const std::string& text, const char* what)
{
    char* end = nullptr;
    const long v = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || end == nullptr || *end != '\0')
        throw UsageError(std::string("invalid integer for ") + what + ": '" + text + "'");
    return static_cast<int>(v);
}

void check_beta(double beta)
{
    if (!(beta >= 0.0 && beta <= 1.0))
        throw UsageError("beta must be within [0, 1]: " + shortest_double(beta));
}

// Writes `text` to `path` (truncating). Throws FatalError on any failure.
void write_text_file(const fs::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw FatalError("cannot open for writing: " + path.string());
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.flush();
    if (!out) throw FatalError("write failed: " + path.string());
}

void rename_or_throw(const fs::path& from, const fs::path& to)
{
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) throw FatalError("cannot rename " + from.string() + " -> " + to.string() +
                             ": " + ec.message());
}

// 8-bit single-channel PNG (0 / 255), row-major, no flipping.
void write_mask_png(const fs::path& path, const std::vector<uint8_t>& mask, int width, int height)
{
    std::vector<unsigned char> pixels(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) pixels[i] = mask[i] ? 255 : 0;

    std::string encoded;
    const int ok = stbi_write_png_to_func(
        [](void* ctx, void* data, int size) {
            static_cast<std::string*>(ctx)->append(static_cast<const char*>(data),
                                                   static_cast<size_t>(size));
        },
        &encoded, width, height, 1, pixels.data(), width);
    if (!ok) throw FatalError("PNG encoding failed: " + path.string());
    write_text_file(path, encoded);
}

// -----------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------
struct Options {
    std::string command;          // "shells" | "peel" | "select-beta"
    std::string dicom_dir;
    // select-beta
    std::vector<std::string> sweep_dirs;
    std::string rule;
    bool   has_valid = false;
    int    valid_start = 0;
    int    valid_end   = -1;      // -1 = no upper bound
    bool   has_candidates = false;
    double cand_min = 0, cand_max = 0;
    std::vector<std::pair<std::string, double>> params;   // --param name=value
    std::string out_dir;
    std::vector<double> betas;
    bool   beta_is_range = false;
    double beta_min = 0, beta_max = 0, beta_step = 0;
    bool   has_range = false;
    int    range_start = 0;
    int    range_end   = -1;      // -1 = to the end
    bool   masks       = false;
    bool   bed_removal = false;
    bool   overwrite   = false;
    std::string command_line;
};

void print_help(std::ostream& os)
{
    os <<
"skinpeeler_cli — batch command-line program for the SKINPEELER engine\n"
"\n"
"Usage:\n"
"  skinpeeler_cli shells <dicom_dir> --out <dir>\n"
"      (--beta B | --beta-range MIN MAX STEP)\n"
"      [--range START:END] [--masks] [--bed-removal on|off] [--overwrite]\n"
"  skinpeeler_cli peel <dicom_dir> --out <dir> --beta B [--overwrite]\n"
"  skinpeeler_cli select-beta <sweep_out_dir>... --rule RULE|all\n"
"      [--valid START:END] [--candidates MIN MAX] [--param NAME=VALUE]...\n"
"      [--out <dir>] [--overwrite]\n"
"  skinpeeler_cli --version\n"
"  skinpeeler_cli --help\n"
"\n"
"shells: per-slice shell statistics and skin cut, one slice in memory at a time.\n"
"  --beta B              one beta value in [0, 1]\n"
"  --beta-range A B S    betas A, A+S, ... up to B (one Voronoi structure per\n"
"                        slice is reused for all betas)\n"
"  --range START:END     slice index range in sorted slice order, END exclusive\n"
"                        (either side may be omitted). Intensity normalization\n"
"                        always uses the global maximum of the FULL series.\n"
"  --masks               also write, per slice and beta, two 8-bit PNGs:\n"
"                        masks/b<beta>/<file>_skin.png  air-excluded skin mask\n"
"                        masks/b<beta>/<file>_air.png   air mask\n"
"  --bed-removal on|off  bed removal (default off). The bed cache is written\n"
"                        under <out>/bed_cache, never into <dicom_dir>.\n"
"  --overwrite           replace existing outputs in <out>\n"
"  Outputs: slices.csv, shells.csv, manifest.json (written as *.part and\n"
"  renamed when the range has been processed).\n"
"\n"
"peel: load the whole series, peel every slice at its skin cut, and write the\n"
"  peeled DICOM series (peeled-<file>) plus manifest.json into <out>.\n"
"\n"
"select-beta: per-specimen beta selection from the slices.csv of one or more\n"
"  `shells` output directories of the SAME specimen (e.g. chunked ranges).\n"
"  --rule RULE           A1 A2 A3 A4 B1 B2 B3 B4 C1 C2 C3a C4, or `all`\n"
"                        (ties go to the smaller beta). A1 A2 A4 B1 B3 also\n"
"                        read shells.csv of each directory.\n"
"  --param NAME=VALUE    threshold override: a1_threshold (0.98), a4_threshold\n"
"                        (0.9741), a2_window (0.02), a3_window (0.01),\n"
"                        c1_area_fraction (0.9), c1_percentile (90),\n"
"                        c1_reference_beta (largest beta present),\n"
"                        c2_epsilon (0.01), c4_max_change (1)\n"
"  --valid START:END     use only slices START <= slice_index < END\n"
"  --candidates MIN MAX  use only betas MIN <= beta <= MAX (default: all present)\n"
"  --out <dir>           where to write beta_selection.csv / .json, or with\n"
"                        `all`: beta_selection_all.csv / .json and\n"
"                        beta_selection_<rule>.csv (default: the first\n"
"                        <sweep_out_dir>)\n"
"\n"
"Exit codes: 0 all slices ok; 1 at least one slice failed; 2 usage/fatal error.\n";
}

void print_version(std::ostream& os)
{
    os << "skinpeeler_cli " << APP_VERSION
       << " (engine commit " << SKINPEELER_GIT_COMMIT
       << ", dirty=" << SKINPEELER_GIT_DIRTY
       << "; geometry library commit " << SKINPEELER_V_GIT_COMMIT
       << ", dirty=" << SKINPEELER_V_GIT_DIRTY
       << "; cutting algorithm "
       << CTImage::cuttingAlgorithmName(DEFAULT_CUTTING_ALGORITHM) << ")\n";
}

std::vector<double> expand_beta_range(double bmin, double bmax, double step)
{
    if (!(step > 0.0)) throw UsageError("--beta-range STEP must be > 0");
    if (bmin > bmax)   throw UsageError("--beta-range MIN must be <= MAX");
    check_beta(bmin);
    check_beta(bmax);
    const long n = static_cast<long>(std::floor((bmax - bmin) / step + 1e-9)) + 1;
    if (n > 10000) throw UsageError("--beta-range yields more than 10000 betas");
    std::vector<double> betas;
    betas.reserve(static_cast<size_t>(n));
    for (long i = 0; i < n; ++i) {
        // Round to 1e-6 so that 0.60 + 6*0.01 is exactly the double 0.66.
        const double b = std::round((bmin + static_cast<double>(i) * step) * 1e6) / 1e6;
        betas.push_back(std::min(b, 1.0));
    }
    return betas;
}

Options parse_args(int argc, char* argv[])
{
    Options opt;
    {
        std::ostringstream cl;
        for (int i = 0; i < argc; ++i) { if (i) cl << ' '; cl << argv[i]; }
        opt.command_line = cl.str();
    }
    if (argc < 2) throw UsageError("missing command");
    opt.command = argv[1];
    if (opt.command != "shells" && opt.command != "peel" && opt.command != "select-beta")
        throw UsageError("unknown command: " + opt.command);
    const bool is_select = (opt.command == "select-beta");

    bool have_beta = false;
    auto need = [&](int& i, int count, const char* flag) {
        if (i + count >= argc)
            throw UsageError(std::string(flag) + " needs " + std::to_string(count) + " value(s)");
    };

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out") {
            need(i, 1, "--out");
            opt.out_dir = argv[++i];
        } else if (is_select && a == "--rule") {
            need(i, 1, "--rule");
            opt.rule = argv[++i];
        } else if (is_select && a == "--valid") {
            need(i, 1, "--valid");
            const std::string r = argv[++i];
            const size_t colon = r.find(':');
            if (colon == std::string::npos)
                throw UsageError("--valid expects START:END, got '" + r + "'");
            const std::string s = r.substr(0, colon), e = r.substr(colon + 1);
            opt.valid_start = s.empty() ? 0 : parse_int(s, "--valid START");
            opt.valid_end   = e.empty() ? -1 : parse_int(e, "--valid END");
            if (opt.valid_start < 0 || (!e.empty() && opt.valid_end < 0))
                throw UsageError("--valid values must be >= 0");
            opt.has_valid = true;
        } else if (is_select && a == "--candidates") {
            need(i, 2, "--candidates");
            opt.cand_min = parse_double(argv[++i], "--candidates MIN");
            opt.cand_max = parse_double(argv[++i], "--candidates MAX");
            if (opt.cand_min > opt.cand_max)
                throw UsageError("--candidates MIN must be <= MAX");
            opt.has_candidates = true;
        } else if (is_select && a == "--param") {
            need(i, 1, "--param");
            const std::string kv = argv[++i];
            const size_t eq = kv.find('=');
            if (eq == std::string::npos)
                throw UsageError("--param expects name=value, got '" + kv + "'");
            opt.params.emplace_back(kv.substr(0, eq),
                                    parse_double(kv.substr(eq + 1), "--param value"));
        } else if (is_select && a == "--overwrite") {
            opt.overwrite = true;
        } else if (is_select && !a.empty() && a[0] == '-') {
            throw UsageError("unknown option for select-beta: " + a);
        } else if (is_select) {
            opt.sweep_dirs.push_back(a);
        } else if (a == "--beta") {
            need(i, 1, "--beta");
            if (have_beta) throw UsageError("give exactly one of --beta / --beta-range");
            const double b = parse_double(argv[++i], "--beta");
            check_beta(b);
            opt.betas = {b};
            have_beta = true;
        } else if (a == "--beta-range") {
            need(i, 3, "--beta-range");
            if (have_beta) throw UsageError("give exactly one of --beta / --beta-range");
            opt.beta_min  = parse_double(argv[++i], "--beta-range MIN");
            opt.beta_max  = parse_double(argv[++i], "--beta-range MAX");
            opt.beta_step = parse_double(argv[++i], "--beta-range STEP");
            opt.betas = expand_beta_range(opt.beta_min, opt.beta_max, opt.beta_step);
            opt.beta_is_range = true;
            have_beta = true;
        } else if (a == "--range") {
            need(i, 1, "--range");
            const std::string r = argv[++i];
            const size_t colon = r.find(':');
            if (colon == std::string::npos)
                throw UsageError("--range expects START:END, got '" + r + "'");
            const std::string s = r.substr(0, colon), e = r.substr(colon + 1);
            opt.range_start = s.empty() ? 0 : parse_int(s, "--range START");
            opt.range_end   = e.empty() ? -1 : parse_int(e, "--range END");
            if (opt.range_start < 0 || (!e.empty() && opt.range_end < 0))
                throw UsageError("--range values must be >= 0");
            opt.has_range = true;
        } else if (a == "--masks") {
            opt.masks = true;
        } else if (a == "--bed-removal") {
            need(i, 1, "--bed-removal");
            const std::string v = argv[++i];
            if (v == "on") opt.bed_removal = true;
            else if (v == "off") opt.bed_removal = false;
            else throw UsageError("--bed-removal expects on|off, got '" + v + "'");
        } else if (a == "--overwrite") {
            opt.overwrite = true;
        } else if (!a.empty() && a[0] == '-') {
            throw UsageError("unknown option: " + a);
        } else if (opt.dicom_dir.empty()) {
            opt.dicom_dir = a;
        } else {
            throw UsageError("unexpected argument: " + a);
        }
    }

    if (is_select) {
        if (opt.sweep_dirs.empty()) throw UsageError("missing <sweep_out_dir>");
        if (opt.rule.empty())       throw UsageError("missing --rule");
        if (opt.out_dir.empty())    opt.out_dir = opt.sweep_dirs.front();
        return opt;
    }
    if (opt.dicom_dir.empty()) throw UsageError("missing <dicom_dir>");
    if (opt.out_dir.empty())   throw UsageError("missing --out <dir>");
    if (!have_beta)            throw UsageError("missing --beta" +
                                   std::string(opt.command == "shells" ? " or --beta-range" : ""));
    if (opt.command == "peel") {
        if (opt.beta_is_range) throw UsageError("peel takes --beta, not --beta-range");
        if (opt.has_range)     throw UsageError("peel does not take --range");
        if (opt.masks)         throw UsageError("peel does not take --masks");
        if (opt.bed_removal)   throw UsageError("peel does not take --bed-removal");
    }
    return opt;
}

// -----------------------------------------------------------------------------
// Manifest pieces shared by both commands
// -----------------------------------------------------------------------------
std::string manifest_tool(const Options& opt)
{
    std::ostringstream m;
    m << "  \"tool\": \"skinpeeler_cli\",\n"
      << "  \"version\": " << json_string(APP_VERSION) << ",\n"
      << "  \"command\": " << json_string(opt.command) << ",\n"
      << "  \"command_line\": " << json_string(opt.command_line) << ",\n"
      << "  \"git_commit\": " << json_string(SKINPEELER_GIT_COMMIT) << ",\n"
      << "  \"git_dirty\": " << json_string(SKINPEELER_GIT_DIRTY) << ",\n"
      << "  \"geometry_library_git_commit\": " << json_string(SKINPEELER_V_GIT_COMMIT) << ",\n"
      << "  \"geometry_library_git_dirty\": " << json_string(SKINPEELER_V_GIT_DIRTY) << ",\n"
      << "  \"git_note\": \"recorded when the build directory was configured by CMake\",\n";
    return m.str();
}

std::string manifest_common(const Options& opt)
{
    std::ostringstream m;
    m << manifest_tool(opt)
      << "  \"cutting_algorithm\": "
      << json_string(CTImage::cuttingAlgorithmName(DEFAULT_CUTTING_ALGORITHM)) << ",\n"
      << "  \"settings\": {\n"
      << "    \"EXCLUDE_OUTERMOST_SHELL\": " << json_bool(EXCLUDE_OUTERMOST_SHELL) << ",\n"
      << "    \"ADAPTIVE_SHELL_MIN\": " << ADAPTIVE_SHELL_MIN << ",\n"
      << "    \"ADAPTIVE_SHELL_MAX\": " << ADAPTIVE_SHELL_MAX << ",\n"
      << "    \"ADAPTIVE_TIME_LIMIT_SECONDS\": " << json_number(ADAPTIVE_TIME_LIMIT_SECONDS) << ",\n"
      << "    \"maximize_shells\": true,\n"
      << "    \"SIGMA_THRESHOLD\": " << json_number(SIGMA_THRESHOLD) << ",\n"
      << "    \"DISTANCE_JUMP_SIGMA\": " << json_number(DISTANCE_JUMP_SIGMA) << ",\n"
      << "    \"DARKER_PIXEL_BIGGER_DISK\": " << json_bool(DARKER_PIXEL_BIGGER_DISK) << ",\n"
      << "    \"selection_uses_outer_loop_circularity\": "
      << json_bool(CTImage::selectionUsesOuterLoopCircularity()) << ",\n"
      << "    \"COORDINATE_OFFSET\": " << json_number(COORDINATE_OFFSET) << ",\n"
      << "    \"IMAGE_MAX_VALUE\": " << IMAGE_MAX_VALUE << "\n"
      << "  },\n"
      << "  \"dicom_dir\": " << json_string(opt.dicom_dir) << ",\n"
      << "  \"out_dir\": " << json_string(opt.out_dir) << ",\n";
    return m.str();
}

std::string json_beta_list(const std::vector<double>& betas)
{
    std::string s = "[";
    for (size_t i = 0; i < betas.size(); ++i) {
        if (i) s += ", ";
        s += shortest_double(betas[i]);
    }
    s += "]";
    return s;
}

// Refuse to mix a new run into existing outputs unless --overwrite.
void prepare_outputs(const fs::path& out_dir, const std::vector<std::string>& names, bool overwrite)
{
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec || !fs::is_directory(out_dir))
        throw FatalError("cannot create output directory: " + out_dir.string());

    for (const auto& name : names) {
        for (const fs::path p : {out_dir / name, out_dir / (name + ".part")}) {
            if (!fs::exists(p)) continue;
            if (!overwrite)
                throw FatalError("output already exists: " + p.string() +
                                 " (use another --out or pass --overwrite)");
            fs::remove(p, ec);
            if (ec) throw FatalError("cannot remove existing output: " + p.string());
        }
    }
}

// =============================================================================
// shells
// =============================================================================
int run_shells(const Options& opt)
{
    const auto t_start = std::chrono::steady_clock::now();
    const std::string start_time = utc_now_iso8601();
    const fs::path out_dir(opt.out_dir);

    prepare_outputs(out_dir, {"slices.csv", "shells.csv", "manifest.json"}, opt.overwrite);
    if (opt.masks && fs::exists(out_dir / "masks") && !opt.overwrite)
        throw FatalError("output already exists: " + (out_dir / "masks").string() +
                         " (use another --out or pass --overwrite)");

    const fs::path slices_part   = out_dir / "slices.csv.part";
    const fs::path shells_part   = out_dir / "shells.csv.part";
    const fs::path manifest_part = out_dir / "manifest.json.part";

    // ---- First pass over the FULL series: slice order + global maximum -------
    std::cerr << "[skinpeeler_cli] scanning series: " << opt.dicom_dir << "\n";
    const auto t_scan = std::chrono::steady_clock::now();
    const Session::SeriesScan scan = Session::scan_series(
        opt.dicom_dir,
        [](const std::string& msg) { std::cerr << "[skinpeeler_cli] WARNING: " << msg << "\n"; });
    const double scan_seconds = seconds_since(t_scan);

    const int n_files  = static_cast<int>(scan.files.size());
    const int n_slices = scan.slice_count();
    if (n_slices == 0)
        throw FatalError("no readable DICOM files in: " + opt.dicom_dir);

    const int range_start = opt.range_start;
    const int range_end   = (opt.range_end < 0) ? n_slices : std::min(opt.range_end, n_slices);
    if (range_start >= range_end)
        throw UsageError("--range selects no slice (series has " + std::to_string(n_slices) +
                         " readable slices)");
    const int n_selected = range_end - range_start;

    std::cerr << "[skinpeeler_cli] " << n_files << " .dcm file(s), " << n_slices
              << " readable, global max " << fmt_double(scan.global_max)
              << ", first pass " << scan_seconds << " s, rss=" << rss_text(current_rss_mb()) << "\n"
              << "[skinpeeler_cli] slices [" << range_start << ", " << range_end << "), "
              << opt.betas.size() << " beta(s), bed removal " << (opt.bed_removal ? "on" : "off")
              << ", masks " << (opt.masks ? "on" : "off") << "\n";

    // ---- Manifest (written once now as "running", rewritten at the end) ------
    int slices_ok = 0, slices_failed = 0;
    long evals_ok = 0, evals_failed = 0, evals_timed_out = 0, evals_no_body = 0;

    auto build_manifest = [&](const std::string& status, const std::string& end_time) {
        std::ostringstream m;
        m << "{\n" << manifest_common(opt)
          << "  \"status\": " << json_string(status) << ",\n"
          << "  \"start_time\": " << json_string(start_time) << ",\n"
          << "  \"end_time\": " << (end_time.empty() ? "null" : json_string(end_time)) << ",\n"
          << "  \"elapsed_seconds\": " << json_number(seconds_since(t_start)) << ",\n"
          << "  \"first_pass_seconds\": " << json_number(scan_seconds) << ",\n"
          << "  \"peak_rss_mb\": " << json_number(peak_rss_mb()) << ",\n"
          << "  \"bed_removal\": " << json_bool(opt.bed_removal) << ",\n"
          << "  \"masks\": " << json_bool(opt.masks) << ",\n"
          << "  \"betas\": " << json_beta_list(opt.betas) << ",\n";
        if (opt.beta_is_range)
            m << "  \"beta_range\": {\"min\": " << shortest_double(opt.beta_min)
              << ", \"max\": " << shortest_double(opt.beta_max)
              << ", \"step\": " << shortest_double(opt.beta_step) << "},\n";
        m << "  \"range\": {\"start\": " << range_start << ", \"end\": " << range_end
          << ", \"requested\": " << json_bool(opt.has_range) << "},\n"
          << "  \"normalization\": {\"global_max\": " << json_number(scan.global_max)
          << ", \"scope\": \"all readable slices of dicom_dir (independent of range)\"},\n"
          << "  \"counts\": {\n"
          << "    \"files\": " << n_files << ",\n"
          << "    \"unreadable_files\": " << (n_files - n_slices) << ",\n"
          << "    \"slices_in_series\": " << n_slices << ",\n"
          << "    \"slices_selected\": " << n_selected << ",\n"
          << "    \"slices_ok\": " << slices_ok << ",\n"
          << "    \"slices_failed\": " << slices_failed << ",\n"
          << "    \"evaluations_ok\": " << evals_ok << ",\n"
          << "    \"evaluations_failed\": " << evals_failed << ",\n"
          << "    \"evaluations_timed_out\": " << evals_timed_out << ",\n"
          << "    \"evaluations_no_body\": " << evals_no_body << "\n"
          << "  },\n"
          << "  \"files\": [\n";
        for (int i = 0; i < n_files; ++i) {
            const auto& f = scan.files[i];
            m << "    {\"file\": " << json_string(fs::path(f.path).filename().string())
              << ", \"size_bytes\": " << f.size_bytes
              << ", \"readable\": " << json_bool(f.readable)
              << ", \"slice_index\": " << f.slice_index
              << ", \"selected\": "
              << json_bool(f.readable && f.slice_index >= range_start && f.slice_index < range_end);
            if (!f.readable) m << ", \"error\": " << json_string(f.error);
            m << "}" << (i + 1 < n_files ? "," : "") << "\n";
        }
        m << "  ]\n}\n";
        return m.str();
    };
    write_text_file(manifest_part, build_manifest("running", ""));

    // ---- CSV outputs ---------------------------------------------------------
    std::ofstream slices_csv(slices_part, std::ios::binary | std::ios::trunc);
    std::ofstream shells_csv(shells_part, std::ios::binary | std::ios::trunc);
    if (!slices_csv) throw FatalError("cannot open for writing: " + slices_part.string());
    if (!shells_csv) throw FatalError("cannot open for writing: " + shells_part.string());

    slices_csv << "slice_index,file,beta,n_components,area,outer_circularity,n_shells,cut,"
                  "cut_rule,changepoint_found,tau,air_threshold,n_skin_pixels,n_air_pixels,"
                  "b2_contrast,judgeable,n_inner_loops,status\n";
    shells_csv << "slice_index,file,beta,shell_index,mu,sigma,n_vertices,d2,is_skin\n";
    slices_csv.flush();
    shells_csv.flush();
    if (!slices_csv || !shells_csv) throw FatalError("write failed in: " + out_dir.string());

    // Mask directories (one per beta)
    std::vector<fs::path> mask_dirs;
    if (opt.masks) {
        for (double b : opt.betas) {
            fs::path dir = out_dir / "masks" / ("b" + beta_dir_label(b));
            std::error_code ec;
            fs::create_directories(dir, ec);
            if (ec) throw FatalError("cannot create directory: " + dir.string());
            mask_dirs.push_back(std::move(dir));
        }
    }

    const std::string bed_cache_dir = (out_dir / "bed_cache").string();
    if (opt.bed_removal) {
        std::error_code ec;
        fs::create_directories(bed_cache_dir, ec);
        if (ec) throw FatalError("cannot create directory: " + bed_cache_dir);
    }

    // ---- Second pass: load / compute / write / release, slice by slice -------
    for (int idx = range_start; idx < range_end; ++idx) {
        const auto t_slice = std::chrono::steady_clock::now();
        const Session::SeriesFile& entry = scan.slice(idx);
        const std::string file = fs::path(entry.path).filename().string();
        const std::string stem = fs::path(entry.path).stem().string();
        const std::string row_prefix = std::to_string(idx) + "," + csv_field(file) + ",";

        std::string slice_rows, shell_rows;   // buffered, written once per slice
        int  n_ok = 0, n_failed = 0, n_timed_out = 0, n_no_body = 0;
        std::string first_error;

        auto failed_row = [&](double beta, const std::string& why) {
            slice_rows += row_prefix + shortest_double(beta) +
                          ",,,,,,,,,,,,,,," + csv_field("failed: " + why) + "\n";
            ++n_failed;
            if (first_error.empty()) first_error = why;
        };

        try {
            // The whole slice (pixels + Voronoi structure) lives only inside
            // this block.
            std::unique_ptr<CTImage> img = Session::load_series_slice(scan, idx, opt.betas.front());
            img->setBedRemoval(opt.bed_removal);
            if (opt.bed_removal) img->setSpecimenDir(bed_cache_dir);

            size_t beta_pos = 0;
            img->sweepBetas(opt.betas, [&](const CTImage::BetaEvaluation& ev) {
                const size_t bi = beta_pos++;
                const double beta = opt.betas[bi];
                if (!ev.ok) { failed_row(beta, ev.error); return; }

                const CTImage::NoAirMasks masks = img->noAirMasks(ev.cut);

                const bool no_body = (ev.component.nComponents == 0) ||
                                     ev.shells.empty() || ev.shells[0].nVertices == 0;
                std::string status = "ok";
                if (ev.timedOut)  { status = "timed_out"; ++n_timed_out; }
                else if (no_body) { status = "no_body";   ++n_no_body; }
                else              { ++n_ok; }

                slice_rows += row_prefix + shortest_double(beta) + "," +
                    std::to_string(ev.component.nComponents) + "," +
                    fmt_double(ev.component.area) + "," +
                    fmt_double(ev.component.outerCircularity) + "," +
                    std::to_string(ev.shells.size()) + "," +
                    std::to_string(ev.cut) + "," +
                    CTImage::cutRuleName(ev.cutRule) + "," +
                    (ev.fit.valid ? "1" : "0") + "," +
                    (ev.fit.valid ? fmt_double(ev.fit.tau) : std::string()) + "," +
                    fmt_double(masks.airThreshold) + "," +
                    std::to_string(masks.nSkinPixels) + "," +
                    std::to_string(masks.nAirPixels) + "," +
                    fmt_double(ev.b2Contrast) + "," +
                    (ev.judgeable ? "1" : "0") + "," +
                    std::to_string(ev.component.nInnerLoops) + "," +
                    status + "\n";

                for (size_t si = 0; si < ev.shells.size(); ++si) {
                    const auto& sh = ev.shells[si];
                    shell_rows += row_prefix + shortest_double(beta) + "," +
                        std::to_string(si) + "," +
                        (sh.hasStatistics ? fmt_double(sh.mu) : std::string()) + "," +
                        (sh.hasStatistics ? fmt_double(sh.sigma) : std::string()) + "," +
                        std::to_string(sh.nVertices) + "," +
                        (sh.hasMahal ? fmt_double(sh.mahalD2) : std::string()) + "," +
                        (static_cast<int>(si) <= ev.cut ? "1" : "0") + "\n";
                }

                if (opt.masks) {
                    write_mask_png(mask_dirs[bi] / (stem + "_skin.png"), masks.skin,
                                   img->width(), img->height());
                    write_mask_png(mask_dirs[bi] / (stem + "_air.png"), masks.air,
                                   img->width(), img->height());
                }
            });
        } catch (const FatalError&) {
            throw;   // output could not be written: stop the run
        } catch (const std::exception& e) {
            // Slice could not be loaded or its Voronoi structure could not be
            // built: every beta not yet reported for this slice is failed.
            const size_t reported = static_cast<size_t>(n_ok + n_failed + n_timed_out + n_no_body);
            for (size_t bi = reported; bi < opt.betas.size(); ++bi)
                failed_row(opt.betas[bi], e.what());
        }

        // One write + one flush per slice.
        slices_csv.write(slice_rows.data(), static_cast<std::streamsize>(slice_rows.size()));
        shells_csv.write(shell_rows.data(), static_cast<std::streamsize>(shell_rows.size()));
        slices_csv.flush();
        shells_csv.flush();
        if (!slices_csv || !shells_csv)
            throw FatalError("write failed in " + out_dir.string() + " (disk full?)");

        evals_ok        += n_ok;
        evals_failed    += n_failed;
        evals_timed_out += n_timed_out;
        evals_no_body   += n_no_body;
        const bool slice_failed = (n_failed > 0 || n_timed_out > 0);
        if (slice_failed) ++slices_failed; else ++slices_ok;

        std::cerr << "[" << (idx - range_start + 1) << "/" << n_selected << "] slice " << idx
                  << " " << file << " betas=" << opt.betas.size()
                  << " ok=" << n_ok << " no_body=" << n_no_body
                  << " timed_out=" << n_timed_out << " failed=" << n_failed
                  << " " << seconds_since(t_slice) << "s rss=" << rss_text(current_rss_mb());
        if (!first_error.empty()) std::cerr << " ERROR: " << first_error;
        std::cerr << "\n";
    }

    // ---- Range done: close, rename *.part → final ----------------------------
    slices_csv.close();
    shells_csv.close();
    if (slices_csv.fail() || shells_csv.fail())
        throw FatalError("closing output failed in " + out_dir.string());

    const std::string status = slices_failed > 0 ? "completed_with_failures" : "completed";
    write_text_file(manifest_part, build_manifest(status, utc_now_iso8601()));

    rename_or_throw(slices_part,   out_dir / "slices.csv");
    rename_or_throw(shells_part,   out_dir / "shells.csv");
    rename_or_throw(manifest_part, out_dir / "manifest.json");

    std::cerr << "[skinpeeler_cli] " << status << ": " << slices_ok << " slice(s) ok, "
              << slices_failed << " failed, " << seconds_since(t_start) << " s, peak rss="
              << rss_text(peak_rss_mb()) << "\n"
              << "[skinpeeler_cli] outputs in " << out_dir.string() << "\n";

    return slices_failed > 0 ? EXIT_SLICE_FAIL : EXIT_OK;
}

// =============================================================================
// peel
// =============================================================================
int run_peel(const Options& opt)
{
    const auto t_start = std::chrono::steady_clock::now();
    const std::string start_time = utc_now_iso8601();
    const fs::path out_dir(opt.out_dir);
    const double beta = opt.betas.front();

    prepare_outputs(out_dir, {"manifest.json"}, opt.overwrite);
    if (!opt.overwrite) {
        for (const auto& e : fs::directory_iterator(out_dir)) {
            const std::string name = e.path().filename().string();
            if (name.rfind(PEELED_FILE_PREFIX, 0) == 0)
                throw FatalError("output already exists: " + e.path().string() +
                                 " (use another --out or pass --overwrite)");
        }
    }
    const fs::path manifest_part = out_dir / "manifest.json.part";

    // Whole-series flow: load everything, peel
    // every slice at its cut, save through the session's save path.
    std::cerr << "[skinpeeler_cli] loading series: " << opt.dicom_dir << "\n";
    Session session;
    session.load_directory(opt.dicom_dir, beta);
    const int n = session.slice_count();

    // Report files the session skipped as unreadable.
    std::vector<std::string> skipped;
    {
        std::set<std::string> loaded;
        for (int i = 0; i < n; ++i) loaded.insert(session.slice_info(i).filename);
        for (const auto& e : fs::directory_iterator(opt.dicom_dir)) {
            if (!e.is_regular_file()) continue;
            std::string ext = e.path().extension().string();
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext != DICOM_EXTENSION) continue;
            const std::string name = e.path().filename().string();
            if (!loaded.count(name)) skipped.push_back(name);
        }
        std::sort(skipped.begin(), skipped.end());
        for (const auto& name : skipped)
            std::cerr << "[skinpeeler_cli] WARNING: skipping unreadable file " << name << "\n";
    }
    std::cerr << "[skinpeeler_cli] " << n << " slice(s) loaded, beta " << shortest_double(beta)
              << ", rss=" << rss_text(current_rss_mb()) << "\n";

    struct Failure { int index; std::string file; std::string error; };
    std::vector<Failure> failures;
    int peeled = 0;
    for (int i = 0; i < n; ++i) {
        const auto t_slice = std::chrono::steady_clock::now();
        const std::string file = session.slice_info(i).filename;
        std::string error;
        try {
            session.peel_slice(i, PeelRegion::WholeBody);
            ++peeled;
        } catch (const std::exception& e) {
            error = e.what();
            session.image(i).releaseVoronoi();
            failures.push_back({i, file, error});
        }
        const auto info = session.slice_info(i);
        std::cerr << "[" << (i + 1) << "/" << n << "] slice " << i << " " << file
                  << " shells=" << info.shell_count << " cut=" << info.peel_index
                  << (info.timed_out ? " TIMED_OUT" : "")
                  << " " << seconds_since(t_slice) << "s rss=" << rss_text(current_rss_mb());
        if (!error.empty()) std::cerr << " ERROR: " << error;
        std::cerr << "\n";
    }

    const int saved = session.save_dicom(opt.out_dir);
    const bool all_ok = failures.empty() && saved == n;

    std::ostringstream m;
    m << "{\n" << manifest_common(opt)
      << "  \"status\": " << json_string(all_ok ? "completed" : "completed_with_failures") << ",\n"
      << "  \"start_time\": " << json_string(start_time) << ",\n"
      << "  \"end_time\": " << json_string(utc_now_iso8601()) << ",\n"
      << "  \"elapsed_seconds\": " << json_number(seconds_since(t_start)) << ",\n"
      << "  \"peak_rss_mb\": " << json_number(peak_rss_mb()) << ",\n"
      << "  \"bed_removal\": false,\n"
      << "  \"betas\": " << json_beta_list(opt.betas) << ",\n"
      << "  \"counts\": {\"slices_loaded\": " << n
      << ", \"unreadable_files\": " << skipped.size()
      << ", \"slices_peeled\": " << peeled
      << ", \"slices_failed\": " << failures.size()
      << ", \"files_saved\": " << saved << "},\n"
      << "  \"unreadable_files\": [";
    for (size_t i = 0; i < skipped.size(); ++i)
        m << (i ? ", " : "") << json_string(skipped[i]);
    m << "],\n  \"failed_slices\": [";
    for (size_t i = 0; i < failures.size(); ++i)
        m << (i ? ", " : "") << "{\"slice_index\": " << failures[i].index
          << ", \"file\": " << json_string(failures[i].file)
          << ", \"error\": " << json_string(failures[i].error) << "}";
    m << "]\n}\n";
    write_text_file(manifest_part, m.str());
    rename_or_throw(manifest_part, out_dir / "manifest.json");

    std::cerr << "[skinpeeler_cli] peeled " << peeled << "/" << n << " slice(s), saved " << saved
              << " file(s) to " << out_dir.string() << ", " << failures.size() << " failed, "
              << seconds_since(t_start) << " s, peak rss=" << rss_text(peak_rss_mb()) << "\n";

    return all_ok ? EXIT_OK : EXIT_SLICE_FAIL;
}

// =============================================================================
// select-beta
// =============================================================================

// One CSV record → fields (RFC 4180 quoting; records written by `shells`
// never contain line breaks inside a field).
std::vector<std::string> split_csv_line(const std::string& line)
{
    std::vector<std::string> fields;
    std::string cur;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else quoted = false;
            } else {
                cur += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

// Reads a CSV file record by record (header first) and hands the fields of
// each data record to `on_row`. Throws FatalError on a malformed record.
template <typename OnHeader, typename OnRow>
long read_csv_file(const fs::path& csv_path, OnHeader on_header, OnRow on_row)
{
    std::ifstream in(csv_path);
    if (!in) {
        std::string hint;
        if (fs::exists(fs::path(csv_path.string() + ".part")))
            hint = " (only the .part file exists: that run did not complete)";
        throw FatalError("cannot read " + csv_path.string() + hint);
    }
    std::string line;
    if (!std::getline(in, line)) throw FatalError("empty file: " + csv_path.string());
    const std::vector<std::string> header = split_csv_line(line);
    on_header(header);

    long rows = 0, line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;
        const std::vector<std::string> f = split_csv_line(line);
        if (f.size() != header.size())
            throw FatalError(csv_path.string() + ":" + std::to_string(line_no) + ": expected " +
                             std::to_string(header.size()) + " fields, got " +
                             std::to_string(f.size()));
        on_row(f);
        ++rows;
    }
    return rows;
}

int csv_column(const std::vector<std::string>& header, const char* name,
               const fs::path& csv_path, bool required = true)
{
    for (size_t i = 0; i < header.size(); ++i)
        if (header[i] == name) return static_cast<int>(i);
    if (required)
        throw FatalError(csv_path.string() + ": missing column '" + name +
                         "' (written by an older skinpeeler_cli?)");
    return -1;
}

// "" / "nan" -> NaN, "inf" / "-inf" -> infinities (strtod).
double csv_double(const std::string& text)
{
    if (text.empty()) return std::numeric_limits<double>::quiet_NaN();
    return std::strtod(text.c_str(), nullptr);
}

bool rule_needs_shells(BetaSelection::Rule rule)
{
    using BetaSelection::Rule;
    return rule == Rule::A1 || rule == Rule::A2 || rule == Rule::A4 ||
           rule == Rule::B1 || rule == Rule::B3;
}

std::string selection_table_csv(const BetaSelection::Selection& sel)
{
    std::string csv = "rule,beta,n_valid,n_judgeable,judgeable_share,n_cut_ge2,cut_ge2_share,"
                      "n_used," + std::string(BetaSelection::scoreName(sel.rule)) + ",selected\n";
    for (const auto& row : sel.table) {
        const bool is_sel = sel.selected && row.beta == sel.beta;
        csv += std::string(BetaSelection::ruleName(sel.rule)) + "," + shortest_double(row.beta) +
               "," + std::to_string(row.nValid) + "," + std::to_string(row.nJudgeable) + "," +
               fmt_double(row.judgeableShare) + "," + std::to_string(row.nCutGe2) + "," +
               fmt_double(row.cutGe2Share) + "," + std::to_string(row.nUsed) + "," +
               fmt_double(row.score) + "," + (is_sel ? "1" : "0") + "\n";
    }
    return csv;
}

std::string selection_json(const BetaSelection::Selection& sel, const std::string& indent)
{
    std::ostringstream j;
    j << indent << "{\n"
      << indent << "  \"rule\": " << json_string(BetaSelection::ruleName(sel.rule)) << ",\n"
      << indent << "  \"score_name\": " << json_string(BetaSelection::scoreName(sel.rule)) << ",\n"
      << indent << "  \"selected\": " << json_bool(sel.selected) << ",\n"
      << indent << "  \"selected_beta\": "
      << (sel.selected ? shortest_double(sel.beta) : "null") << ",\n"
      << indent << "  \"score_at_selected\": "
      << (sel.selected ? json_number(sel.scoreAtSelected) : "null") << ",\n"
      << indent << "  \"reason\": " << json_string(sel.reason) << ",\n"
      << indent << "  \"diagnostics\": {";
    for (size_t i = 0; i < sel.diagnostics.size(); ++i)
        j << (i ? ", " : "") << json_string(sel.diagnostics[i].first) << ": "
          << json_number(sel.diagnostics[i].second);
    j << "},\n" << indent << "  \"table\": [\n";
    for (size_t i = 0; i < sel.table.size(); ++i) {
        const auto& row = sel.table[i];
        j << indent << "    {\"beta\": " << shortest_double(row.beta)
          << ", \"n_valid\": " << row.nValid
          << ", \"n_judgeable\": " << row.nJudgeable
          << ", \"judgeable_share\": " << json_number(row.judgeableShare)
          << ", \"n_cut_ge2\": " << row.nCutGe2
          << ", \"cut_ge2_share\": " << json_number(row.cutGe2Share)
          << ", \"n_used\": " << row.nUsed
          << ", \"score\": " << json_number(row.score) << "}"
          << (i + 1 < sel.table.size() ? "," : "") << "\n";
    }
    j << indent << "  ]\n" << indent << "}";
    return j.str();
}

void print_selection_table(const BetaSelection::Selection& sel)
{
    std::printf("rule %s (%s)\n", BetaSelection::ruleName(sel.rule),
                BetaSelection::scoreName(sel.rule));
    std::printf("%8s %8s %11s %15s %10s %14s %7s %14s %s\n", "beta", "n_valid", "n_judgeable",
                "judgeable_share", "n_cut_ge2", "cut_ge2_share", "n_used", "score", "selected");
    for (const auto& row : sel.table) {
        const bool is_sel = sel.selected && row.beta == sel.beta;
        std::printf("%8s %8d %11d %15.4f %10d %14.4f %7d %14.6f %s\n",
                    shortest_double(row.beta).c_str(), row.nValid, row.nJudgeable,
                    row.judgeableShare, row.nCutGe2, row.cutGe2Share, row.nUsed, row.score,
                    is_sel ? "*" : "");
    }
    for (const auto& d : sel.diagnostics)
        std::printf("  %s = %s\n", d.first.c_str(), fmt_double(d.second).c_str());
    if (sel.selected) std::printf("selected beta: %s\n", shortest_double(sel.beta).c_str());
    else              std::printf("selected beta: not decidable (%s)\n", sel.reason.c_str());
}

int run_select_beta(const Options& opt)
{
    // ---- Rules ---------------------------------------------------------------
    const bool all_rules = (opt.rule == "all" || opt.rule == "ALL");
    std::vector<BetaSelection::Rule> rules;
    if (all_rules) {
        rules = BetaSelection::allRules();
    } else {
        BetaSelection::Rule rule;
        if (!BetaSelection::parseRule(opt.rule, rule))
            throw UsageError("unknown rule: " + opt.rule +
                             " (A1 A2 A3 A4 B1 B2 B3 B4 C1 C2 C3a C4 all)");
        rules.push_back(rule);
    }
    bool need_shells = false;
    for (auto r : rules) need_shells = need_shells || rule_needs_shells(r);

    BetaSelection::Params params;
    for (const auto& kv : opt.params)
        if (!BetaSelection::setParam(params, kv.first, kv.second))
            throw UsageError("unknown --param name: " + kv.first);

    const fs::path out_dir(opt.out_dir);
    std::vector<std::string> out_names;
    if (all_rules) {
        out_names = {"beta_selection_all.csv", "beta_selection_all.json"};
        for (auto r : rules)
            out_names.push_back(std::string("beta_selection_") + BetaSelection::ruleName(r) + ".csv");
    } else {
        out_names = {"beta_selection.csv", "beta_selection.json"};
    }
    prepare_outputs(out_dir, out_names, opt.overwrite);

    // ---- Read slices.csv (and shells.csv) of every sweep directory -----------
    std::vector<BetaSelection::SliceRecord> records;
    std::map<std::pair<int, double>, size_t> index_of;     // (slice, beta) -> records index
    std::map<std::pair<int, double>, std::string> source;  // (slice, beta) -> source dir
    std::map<int, std::string> file_of_slice;
    struct Input { std::string dir; long rows = 0; long failed_rows = 0; long shell_rows = 0; };
    std::vector<Input> inputs;
    bool have_inner_loops = true;
    const int first_fit_shell = ShellIndex::kOffset + 1;   // S2

    for (const auto& dir : opt.sweep_dirs) {
        Input input;
        input.dir = dir;

        const fs::path csv_path = fs::path(dir) / "slices.csv";
        int c_slice = -1, c_file = -1, c_beta = -1, c_cut = -1, c_cp = -1, c_b2 = -1,
            c_judge = -1, c_status = -1, c_ncomp = -1, c_area = -1, c_nshells = -1, c_inner = -1;
        input.rows = read_csv_file(
            csv_path,
            [&](const std::vector<std::string>& header) {
                c_slice   = csv_column(header, "slice_index", csv_path);
                c_file    = csv_column(header, "file", csv_path);
                c_beta    = csv_column(header, "beta", csv_path);
                c_cut     = csv_column(header, "cut", csv_path);
                c_cp      = csv_column(header, "changepoint_found", csv_path);
                c_b2      = csv_column(header, "b2_contrast", csv_path);
                c_judge   = csv_column(header, "judgeable", csv_path);
                c_status  = csv_column(header, "status", csv_path);
                c_ncomp   = csv_column(header, "n_components", csv_path);
                c_area    = csv_column(header, "area", csv_path);
                c_nshells = csv_column(header, "n_shells", csv_path);
                c_inner   = csv_column(header, "n_inner_loops", csv_path, false);
                if (c_inner < 0) have_inner_loops = false;
            },
            [&](const std::vector<std::string>& f) {
                BetaSelection::SliceRecord rec;
                rec.sliceIndex = parse_int(f[c_slice], "slice_index");
                rec.beta       = parse_double(f[c_beta], "beta");
                const bool failed = (f[c_status].rfind("failed", 0) == 0) || f[c_cut].empty();
                if (failed) {
                    // A failed evaluation is a valid slice that could not be judged.
                    rec.evaluated = false;
                    ++input.failed_rows;
                } else {
                    rec.judgeable        = (f[c_judge] == "1");
                    rec.changepointFound = (f[c_cp] == "1");
                    rec.cut              = parse_int(f[c_cut], "cut");
                    rec.b2Contrast       = csv_double(f[c_b2]);
                    rec.nComponents      = parse_int(f[c_ncomp], "n_components");
                    rec.area             = csv_double(f[c_area]);
                    rec.nShells          = parse_int(f[c_nshells], "n_shells");
                    if (c_inner >= 0 && !f[c_inner].empty())
                        rec.nInnerLoops = parse_int(f[c_inner], "n_inner_loops");
                }

                const auto key = std::make_pair(rec.sliceIndex, rec.beta);
                const auto dup = source.find(key);
                if (dup != source.end())
                    throw FatalError("slice " + std::to_string(rec.sliceIndex) + " beta " +
                                     shortest_double(rec.beta) + " appears twice (" + dup->second +
                                     " and " + dir + "): overlapping sweep directories");
                source.emplace(key, dir);

                // Same slice index must be the same file in every directory
                // (guards against mixing specimens).
                const auto fit = file_of_slice.find(rec.sliceIndex);
                if (fit == file_of_slice.end()) file_of_slice.emplace(rec.sliceIndex, f[c_file]);
                else if (fit->second != f[c_file])
                    throw FatalError("slice " + std::to_string(rec.sliceIndex) + " is file '" +
                                     fit->second + "' in one directory and '" + f[c_file] +
                                     "' in " + dir + ": not the same specimen/series");

                index_of.emplace(key, records.size());
                records.push_back(std::move(rec));
            });

        if (need_shells) {
            // shells.csv holds the shells of one (slice, beta) as consecutive
            // rows: collect a group, hand it to the engine, drop it.
            const fs::path shells_path = fs::path(dir) / "shells.csv";
            int s_slice = -1, s_beta = -1, s_idx = -1, s_mu = -1, s_sigma = -1, s_nv = -1, s_d2 = -1;
            std::vector<BetaSelection::ShellRow> group;
            std::pair<int, double> group_key(-1, 0.0);
            std::set<std::pair<int, double>> done;
            auto flush_group = [&]() {
                if (group.empty()) return;
                const auto it = index_of.find(group_key);
                if (it == index_of.end())
                    throw FatalError(shells_path.string() + ": slice " +
                                     std::to_string(group_key.first) + " beta " +
                                     shortest_double(group_key.second) + " is not in slices.csv");
                if (!done.insert(group_key).second)
                    throw FatalError(shells_path.string() + ": rows of slice " +
                                     std::to_string(group_key.first) + " beta " +
                                     shortest_double(group_key.second) + " are not consecutive");
                std::sort(group.begin(), group.end(),
                          [](const BetaSelection::ShellRow& a, const BetaSelection::ShellRow& b) {
                              return a.shellIndex < b.shellIndex;
                          });
                for (size_t i = 0; i < group.size(); ++i)
                    if (group[i].shellIndex != static_cast<int>(i))
                        throw FatalError(shells_path.string() + ": shell indices of slice " +
                                         std::to_string(group_key.first) + " beta " +
                                         shortest_double(group_key.second) + " are not 0..n-1");
                BetaSelection::fillFromShells(records[it->second], group, first_fit_shell);
                group.clear();
            };
            input.shell_rows = read_csv_file(
                shells_path,
                [&](const std::vector<std::string>& header) {
                    s_slice = csv_column(header, "slice_index", shells_path);
                    s_beta  = csv_column(header, "beta", shells_path);
                    s_idx   = csv_column(header, "shell_index", shells_path);
                    s_mu    = csv_column(header, "mu", shells_path);
                    s_sigma = csv_column(header, "sigma", shells_path);
                    s_nv    = csv_column(header, "n_vertices", shells_path);
                    s_d2    = csv_column(header, "d2", shells_path);
                },
                [&](const std::vector<std::string>& f) {
                    const std::pair<int, double> key(parse_int(f[s_slice], "slice_index"),
                                                     parse_double(f[s_beta], "beta"));
                    if (key != group_key) { flush_group(); group_key = key; }
                    BetaSelection::ShellRow row;
                    row.shellIndex = parse_int(f[s_idx], "shell_index");
                    row.mu         = csv_double(f[s_mu]);
                    row.sigma      = csv_double(f[s_sigma]);
                    row.nVertices  = parse_int(f[s_nv], "n_vertices");
                    row.d2         = csv_double(f[s_d2]);
                    group.push_back(row);
                });
            flush_group();
        }
        std::cerr << "[skinpeeler_cli] read " << dir << ": " << input.rows << " slice row(s), "
                  << input.shell_rows << " shell row(s)\n";
        inputs.push_back(input);
    }
    if (records.empty()) throw FatalError("no rows in the given slices.csv file(s)");

    // ---- Query ---------------------------------------------------------------
    BetaSelection::Query query;
    query.params = params;
    if (opt.has_valid) {
        query.restrictSlices = true;
        for (const auto& rec : records)
            if (rec.sliceIndex >= opt.valid_start &&
                (opt.valid_end < 0 || rec.sliceIndex < opt.valid_end))
                query.validSlices.insert(rec.sliceIndex);
    }
    if (opt.has_candidates) {
        query.restrictBetas = true;
        query.betaMin = opt.cand_min;
        query.betaMax = opt.cand_max;
    }

    std::vector<BetaSelection::Selection> selections;
    for (auto r : rules) {
        query.rule = r;
        selections.push_back(BetaSelection::select(records, query));
    }
    if (selections.front().table.empty())
        throw FatalError("no slice/beta left after --valid / --candidates");

    // ---- Output --------------------------------------------------------------
    std::ostringstream head;
    head << "{\n" << manifest_tool(opt)
         << "  \"time\": " << json_string(utc_now_iso8601()) << ",\n"
         << "  \"tie_break\": \"smaller beta\",\n"
         << "  \"params\": {";
    {
        const auto plist = BetaSelection::listParams(params);
        for (size_t i = 0; i < plist.size(); ++i)
            head << (i ? ", " : "") << json_string(plist[i].first) << ": "
                 << (std::isnan(plist[i].second) ? std::string("null")
                                                 : json_number(plist[i].second));
    }
    head << "},\n  \"valid\": ";
    if (opt.has_valid)
        head << "{\"start\": " << opt.valid_start << ", \"end\": "
             << (opt.valid_end < 0 ? std::string("null") : std::to_string(opt.valid_end)) << "},\n";
    else
        head << "null,\n";
    head << "  \"candidates\": ";
    if (opt.has_candidates)
        head << "{\"min\": " << shortest_double(opt.cand_min) << ", \"max\": "
             << shortest_double(opt.cand_max) << "},\n";
    else
        head << "null,\n";
    head << "  \"n_inner_loops_column_present\": " << json_bool(have_inner_loops) << ",\n"
         << "  \"inputs\": [";
    for (size_t i = 0; i < inputs.size(); ++i)
        head << (i ? ", " : "") << "{\"dir\": " << json_string(inputs[i].dir)
             << ", \"rows\": " << inputs[i].rows
             << ", \"failed_rows\": " << inputs[i].failed_rows
             << ", \"shell_rows\": " << inputs[i].shell_rows << "}";
    head << "],\n";

    if (!all_rules) {
        const BetaSelection::Selection& sel = selections.front();
        print_selection_table(sel);

        std::ostringstream j;
        j << head.str()
          << "  \"rule\": " << json_string(BetaSelection::ruleName(sel.rule)) << ",\n"
          << "  \"score_name\": " << json_string(BetaSelection::scoreName(sel.rule)) << ",\n"
          << "  \"selected\": " << json_bool(sel.selected) << ",\n"
          << "  \"selected_beta\": " << (sel.selected ? shortest_double(sel.beta) : "null") << ",\n"
          << "  \"result\":\n" << selection_json(sel, "  ") << "\n}\n";

        write_text_file(out_dir / "beta_selection.csv.part", selection_table_csv(sel));
        write_text_file(out_dir / "beta_selection.json.part", j.str());
        rename_or_throw(out_dir / "beta_selection.csv.part",  out_dir / "beta_selection.csv");
        rename_or_throw(out_dir / "beta_selection.json.part", out_dir / "beta_selection.json");
        std::cerr << "[skinpeeler_cli] wrote " << (out_dir / "beta_selection.csv").string()
                  << " and beta_selection.json\n";
        return sel.selected ? EXIT_OK : EXIT_SLICE_FAIL;
    }

    // --rule all: one summary table + one per-beta table per rule
    std::string summary = "rule,selected_beta,score_name,score_at_selected,reason\n";
    std::printf("%-5s %-13s %-38s %-16s %s\n", "rule", "selected_beta", "score_name",
                "score_at_selected", "reason");
    std::ostringstream j;
    j << head.str() << "  \"rules\": [\n";
    for (size_t i = 0; i < selections.size(); ++i) {
        const auto& sel = selections[i];
        const std::string name = BetaSelection::ruleName(sel.rule);
        const std::string beta_text  = sel.selected ? shortest_double(sel.beta) : "";
        const std::string score_text = sel.selected ? fmt_double(sel.scoreAtSelected) : "";
        const std::string reason = sel.selected ? "" : "not decidable: " + sel.reason;
        summary += name + "," + beta_text + "," + BetaSelection::scoreName(sel.rule) + "," +
                   score_text + "," + csv_field(reason) + "\n";
        std::printf("%-5s %-13s %-38s %-16s %s\n", name.c_str(),
                    sel.selected ? beta_text.c_str() : "-", BetaSelection::scoreName(sel.rule),
                    sel.selected ? shortest_double(sel.scoreAtSelected).c_str() : "-",
                    reason.c_str());

        const std::string file = "beta_selection_" + name + ".csv";
        write_text_file(out_dir / (file + ".part"), selection_table_csv(sel));
        rename_or_throw(out_dir / (file + ".part"), out_dir / file);
        j << selection_json(sel, "    ") << (i + 1 < selections.size() ? "," : "") << "\n";
    }
    j << "  ]\n}\n";
    write_text_file(out_dir / "beta_selection_all.csv.part", summary);
    write_text_file(out_dir / "beta_selection_all.json.part", j.str());
    rename_or_throw(out_dir / "beta_selection_all.csv.part",  out_dir / "beta_selection_all.csv");
    rename_or_throw(out_dir / "beta_selection_all.json.part", out_dir / "beta_selection_all.json");
    std::cerr << "[skinpeeler_cli] wrote beta_selection_all.csv / .json and "
              << selections.size() << " per-rule tables to " << out_dir.string() << "\n";
    return EXIT_OK;
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc >= 2) {
        const std::string a = argv[1];
        if (a == "--help" || a == "-h") { print_help(std::cout); return EXIT_OK; }
        if (a == "--version")           { print_version(std::cout); return EXIT_OK; }
    }

    try {
        const Options opt = parse_args(argc, argv);
        if (opt.command == "shells") return run_shells(opt);
        if (opt.command == "peel")   return run_peel(opt);
        return run_select_beta(opt);
    } catch (const UsageError& e) {
        std::cerr << "skinpeeler_cli: " << e.what() << "\n\n";
        print_help(std::cerr);
        return EXIT_FATAL;
    } catch (const std::exception& e) {
        std::cerr << "[skinpeeler_cli] FATAL: " << e.what() << "\n";
        return EXIT_FATAL;
    }
}
