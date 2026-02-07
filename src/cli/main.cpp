#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "gf/core/gauss_ir.h"
#include "gf/core/model_info.h"
#include "gf/core/validate.h"
#include "gf/core/version.h"
#include "gf/io/reader.h"
#include "gf/io/registry.h"
#include "gf/io/writer.h"

namespace {

void PrintModelInfo(const gf::ModelInfo &info) {
  std::cout << "=== Gaussian Model Information ===\n\n";

  // Basic info
  std::cout << "Basic Info:\n";
  std::cout << "  Points:          " << info.numPoints << "\n";
  if (info.fileSize > 0) {
    std::cout << "  File Size:       " << gf::FormatBytes(info.fileSize) << "\n";
  }
  if (!info.sourceFormat.empty()) {
    std::cout << "  Source Format:   " << info.sourceFormat << "\n";
  }
  std::cout << "\n";

  // Rendering properties (only show meaningful info)
  std::cout << "Rendering Properties:\n";
  std::cout << "  SH Degree:       " << info.shDegree << "\n";
  if (info.antialiased) {
    std::cout << "  Antialiased:     Yes\n";
  }
  std::cout << "\n";

  // Geometry statistics
  if (info.numPoints > 0) {
    std::cout << "Position Bounds:\n";
    std::cout << "  X:  [" << info.bounds.minX << ", " << info.bounds.maxX
              << "]\n";
    std::cout << "  Y:  [" << info.bounds.minY << ", " << info.bounds.maxY
              << "]\n";
    std::cout << "  Z:  [" << info.bounds.minZ << ", " << info.bounds.maxZ
              << "]\n";
    std::cout << "\n";
  }

  // Scale statistics
  if (info.scaleStats.count > 0) {
    std::cout << "Scale Statistics:\n";
    std::cout << "  Min:  " << info.scaleStats.min << "\n";
    std::cout << "  Max:  " << info.scaleStats.max << "\n";
    std::cout << "  Avg:  " << info.scaleStats.avg << "\n";
    std::cout << "\n";
  }

  // Alpha statistics
  if (info.alphaStats.count > 0) {
    std::cout << "Alpha Statistics:\n";
    std::cout << "  Min:  " << info.alphaStats.min << "\n";
    std::cout << "  Max:  " << info.alphaStats.max << "\n";
    std::cout << "  Avg:  " << info.alphaStats.avg << "\n";
    std::cout << "\n";
  }

  // Data size breakdown
  std::cout << "Data Size Breakdown:\n";
  std::cout << "  Positions:       " << gf::FormatBytes(info.positionsSize)
            << "\n";
  std::cout << "  Scales:          " << gf::FormatBytes(info.scalesSize)
            << "\n";
  std::cout << "  Rotations:       " << gf::FormatBytes(info.rotationsSize)
            << "\n";
  std::cout << "  Alphas:          " << gf::FormatBytes(info.alphasSize)
            << "\n";
  std::cout << "  Colors:          " << gf::FormatBytes(info.colorsSize)
            << "\n";
  std::cout << "  SH Coeffs:       " << gf::FormatBytes(info.shSize) << "\n";
  for (const auto &[name, size] : info.extraAttrs) {
    std::cout << "  Extra " << name << ":       " << gf::FormatBytes(size)
              << "\n";
  }
  std::cout << "  Total:           " << gf::FormatBytes(info.totalSize)
            << " (in memory)\n";
}

std::string GetExt(const std::string &path) {
  // Special handling for .compressed.ply double-suffix format to avoid
  // truncating to just ply
  constexpr std::string_view kCompressed = ".compressed.ply";
  if (path.size() >= kCompressed.size() &&
      path.compare(path.size() - kCompressed.size(), kCompressed.size(),
                   kCompressed) == 0) {
    return std::string{kCompressed};
  }

  const auto pos = path.find_last_of('.');
  if (pos == std::string::npos)
    return "";
  return path.substr(pos + 1);
}

} // namespace

int main(int argc, char **argv) {
  // Handle --version flag
  if (argc >= 2 && std::string(argv[1]) == "--version") {
    std::cout << "gfconvert version " << GAUSS_FORGE_VERSION_STRING << "\n";
    return 0;
  }

  // Handle --info flag
  if (argc >= 3 && std::string(argv[1]) == "--info") {
    const std::string in_path = argv[2];
    std::string in_ext = GetExt(in_path);

    // Check for optional --format flag
    for (int i = 3; i + 1 < argc; i += 2) {
      const std::string flag = argv[i];
      const std::string val = argv[i + 1];
      if (flag == "--format") {
        in_ext = val;
      } else {
        std::cerr << "Unknown parameter for --info: " << flag << "\n";
        return 1;
      }
    }

    gf::IORegistry registry;
    auto *reader = registry.ReaderForExt(in_ext);
    if (!reader) {
      std::cerr << "Reader not found for input format: " << in_ext << "\n";
      return 1;
    }

    // Read input file
    std::ifstream in_file(in_path, std::ios::binary | std::ios::ate);
    if (!in_file.good()) {
      std::cerr << "Failed to open input file: " << in_path << "\n";
      return 1;
    }

    size_t in_size = in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    std::vector<uint8_t> in_data(in_size);
    in_file.read(reinterpret_cast<char *>(in_data.data()), in_size);
    in_file.close();

    if (!in_file.good()) {
      std::cerr << "Failed to read input file: " << in_path << "\n";
      return 1;
    }

    // Parse data
    gf::ReadOptions read_opt;
    auto ir_or = reader->Read(in_data.data(), in_data.size(), read_opt);
    if (!ir_or) {
      std::cerr << "Read failed: " << ir_or.error().message << "\n";
      return 1;
    }

    auto ir = std::move(ir_or.value());
    const auto err = gf::ValidateBasic(ir, /*strict=*/false);
    if (!err.message.empty()) {
      std::cerr << "Validation warning: " << err.message << "\n";
    }

    // Get and print model info
    gf::ModelInfo info = gf::GetModelInfo(ir, in_size);
    PrintModelInfo(info);
    return 0;
  }

  if (argc < 3) {
    std::cerr << "Usage: gfconvert <input> <output> "
                 "[--in-format ext] [--out-format ext]\n";
    std::cerr << "       gfconvert --info <input> [--format ext]\n";
    std::cerr << "       gfconvert --version\n";
    return 1;
  }

  const std::string in_path = argv[1];
  const std::string out_path = argv[2];
  std::string in_ext = GetExt(in_path);
  std::string out_ext = GetExt(out_path);

  for (int i = 3; i + 1 < argc; i += 2) {
    const std::string flag = argv[i];
    const std::string val = argv[i + 1];
    if (flag == "--in-format") {
      in_ext = val;
    } else if (flag == "--out-format") {
      out_ext = val;
    } else {
      std::cerr << "Unknown parameter: " << flag << "\n";
      return 1;
    }
  }

  gf::IORegistry registry; // Automatically registers built-in formats

  auto *reader = registry.ReaderForExt(in_ext);
  if (!reader) {
    std::cerr << "Reader not found for input format: " << in_ext << "\n";
    return 1;
  }
  auto *writer = registry.WriterForExt(out_ext);
  if (!writer) {
    std::cerr << "Writer not found for output format: " << out_ext << "\n";
    return 1;
  }

  // Read input file into memory
  std::ifstream in_file(in_path, std::ios::binary | std::ios::ate);
  if (!in_file.good()) {
    std::cerr << "Failed to open input file: " << in_path << "\n";
    return 1;
  }

  size_t in_size = in_file.tellg();
  in_file.seekg(0, std::ios::beg);

  std::vector<uint8_t> in_data(in_size);
  in_file.read(reinterpret_cast<char *>(in_data.data()), in_size);
  in_file.close();

  if (!in_file.good()) {
    std::cerr << "Failed to read input file: " << in_path << "\n";
    return 1;
  }

  // Parse data using reader
  gf::ReadOptions read_opt;
  auto ir_or = reader->Read(in_data.data(), in_data.size(), read_opt);
  if (!ir_or) {
    std::cerr << "Read failed: " << ir_or.error().message << "\n";
    return 1;
  }

  auto ir = std::move(ir_or.value());
  const auto err = gf::ValidateBasic(ir, /*strict=*/false);
  if (!err.message.empty()) {
    std::cerr << "Validation warning/error: " << err.message << "\n";
    // Continue in non-strict mode
  }

  // Serialize data using writer
  gf::WriteOptions write_opt;
  auto out_data_or = writer->Write(ir, write_opt);
  if (!out_data_or) {
    std::cerr << "Write failed: " << out_data_or.error().message << "\n";
    return 1;
  }

  // Write data to output file
  std::ofstream out_file(out_path, std::ios::binary);
  if (!out_file.good()) {
    std::cerr << "Failed to open output file: " << out_path << "\n";
    return 1;
  }

  auto out_data = std::move(out_data_or.value());
  out_file.write(reinterpret_cast<const char *>(out_data.data()),
                 out_data.size());
  out_file.close();

  if (!out_file.good()) {
    std::cerr << "Failed to write output file: " << out_path << "\n";
    return 1;
  }

  std::cout << "Conversion completed: " << in_path << " -> " << out_path
            << "\n";
  return 0;
}
