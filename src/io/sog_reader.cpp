#include "gf/io/sog.h"
#include "zip_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <webp/decode.h>
#include <zlib.h>

#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"
#include "gf/core/validate.h"

using json = nlohmann::json;

namespace gf {

namespace {

class SimpleZipReader {
public:
  struct FileEntry {
    std::string filename;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint32_t localHeaderOffset;
    uint16_t compression;
  };

  bool Open(const uint8_t *data, size_t size) {
    data_ = data;
    size_ = size;
    return ParseZipDirectory();
  }

  std::vector<uint8_t> ExtractFile(const std::string &filename) const {
    for (const auto &entry : entries_) {
      if (entry.filename == filename) {
        return ExtractEntry(entry);
      }
    }
    return {};
  }

private:
  const uint8_t *data_ = nullptr;
  size_t size_ = 0;
  std::vector<FileEntry> entries_;

  bool ParseZipDirectory() {
    if (size_ < 22)
      return false;

    size_t eocdPos = size_ - 22;
    while (eocdPos > 0) {
      if (data_[eocdPos] == 0x50 && data_[eocdPos + 1] == 0x4b &&
          data_[eocdPos + 2] == 0x05 && data_[eocdPos + 3] == 0x06) {
        break;
      }
      eocdPos--;
    }

    if (eocdPos == 0 &&
        !(data_[0] == 0x50 && data_[1] == 0x4b && data_[2] == 0x05 &&
          data_[3] == 0x06)) {
      return false;
    }

    uint32_t centralDirOffset =
        *reinterpret_cast<const uint32_t *>(data_ + eocdPos + 16);
    uint16_t numEntries =
        *reinterpret_cast<const uint16_t *>(data_ + eocdPos + 10);

    size_t pos = static_cast<size_t>(centralDirOffset);
    for (size_t i = 0; i < static_cast<size_t>(numEntries) && pos < eocdPos; i++) {
      if (pos + 46 > size_)
        break;

      uint32_t sig = *reinterpret_cast<const uint32_t *>(data_ + pos);
      if (sig != zip::kCentralDirHeaderSig)
        break;

      FileEntry entry;
      entry.compression = *reinterpret_cast<const uint16_t *>(data_ + pos + 10);
      entry.compressedSize =
          *reinterpret_cast<const uint32_t *>(data_ + pos + 20);
      entry.uncompressedSize =
          *reinterpret_cast<const uint32_t *>(data_ + pos + 24);
      uint16_t nameLen = *reinterpret_cast<const uint16_t *>(data_ + pos + 28);
      uint16_t extraLen = *reinterpret_cast<const uint16_t *>(data_ + pos + 30);
      uint16_t commentLen =
          *reinterpret_cast<const uint16_t *>(data_ + pos + 32);
      entry.localHeaderOffset =
          *reinterpret_cast<const uint32_t *>(data_ + pos + 42);

      if (pos + 46 + nameLen > size_)
        break;

      entry.filename =
          std::string(reinterpret_cast<const char *>(data_ + pos + 46), static_cast<size_t>(nameLen));
      entries_.push_back(entry);

      pos += 46 + static_cast<size_t>(nameLen) + static_cast<size_t>(extraLen) + static_cast<size_t>(commentLen);
    }

    return !entries_.empty();
  }

  std::vector<uint8_t> ExtractEntry(const FileEntry &entry) const {
    if (static_cast<size_t>(entry.localHeaderOffset) + 30 > size_)
      return {};

    size_t pos = static_cast<size_t>(entry.localHeaderOffset);
    uint32_t sig = *reinterpret_cast<const uint32_t *>(data_ + pos);
    if (sig != zip::kLocalFileHeaderSig)
      return {};

    uint16_t nameLen = *reinterpret_cast<const uint16_t *>(data_ + pos + 26);
    uint16_t extraLen = *reinterpret_cast<const uint16_t *>(data_ + pos + 28);

    size_t dataOffset = pos + 30 + static_cast<size_t>(nameLen) + static_cast<size_t>(extraLen);
    if (dataOffset + static_cast<size_t>(entry.compressedSize) > size_)
      return {};

    const uint8_t *compressedData = data_ + dataOffset;

    if (entry.compression == 0) {
      return std::vector<uint8_t>(compressedData,
                                  compressedData + static_cast<size_t>(entry.uncompressedSize));
    } else if (entry.compression == 8) {
      std::vector<uint8_t> output(static_cast<size_t>(entry.uncompressedSize));

      z_stream strm = {};
      strm.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(compressedData));
      strm.avail_in = static_cast<uInt>(entry.compressedSize);
      strm.next_out = reinterpret_cast<Bytef *>(output.data());
      strm.avail_out = static_cast<uInt>(entry.uncompressedSize);

      if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return {};

      int ret = inflate(&strm, Z_FINISH);
      inflateEnd(&strm);

      if (ret != Z_STREAM_END)
        return {};

      return output;
    }

    return {};
  }
};

// SOG Helper functions
inline float InvLogTransform(float v) {
  float a = std::abs(v);
  float e = std::exp(a) - 1.0f;
  return v < 0.0f ? -e : e;
}

inline float SigmoidInv(float y) {
  float e = std::clamp(y, 1e-6f, 1.0f - 1e-6f);
  return std::log(e / (1.0f - e));
}

struct WebPImage {
  std::vector<uint8_t> rgba;
  uint32_t width = 0;
  uint32_t height = 0;
};

bool DecodeWebP(const std::vector<uint8_t> &webpData, WebPImage &output) {
  if (webpData.empty()) return false;
  int width = 0, height = 0;
  if (!WebPGetInfo(webpData.data(), webpData.size(), &width, &height)) {
    return false;
  }

  output.width = static_cast<uint32_t>(width);
  output.height = static_cast<uint32_t>(height);
  output.rgba.resize(static_cast<size_t>(width) * height * 4);

  uint8_t *result = WebPDecodeRGBAInto(webpData.data(), webpData.size(),
                                       output.rgba.data(), output.rgba.size(),
                                       width * 4);
  return result != nullptr;
}

struct SogMeta {
  uint32_t version = 0;
  uint32_t count = 0;
  struct {
    std::vector<float> mins;
    std::vector<float> maxs;
    std::vector<std::string> files;
  } means;
  struct {
    std::vector<float> codebook;
    std::vector<std::string> files;
  } scales;
  struct {
    std::vector<std::string> files;
  } quats;
  struct {
    std::vector<float> codebook;
    std::vector<std::string> files;
  } sh0;
  struct {
    uint32_t count = 0;
    uint32_t bands = 0;
    std::vector<float> codebook;
    std::vector<std::string> files;
  } shN;
};

bool ParseMeta(const std::vector<uint8_t> &jsonData, SogMeta &meta) {
  try {
    json j = json::parse(jsonData.begin(), jsonData.end());
    meta.version = j.value("version", 0u);
    meta.count = j.value("count", 0u);

    if (j.contains("means")) {
      auto &means = j["means"];
      meta.means.mins = means.value("mins", std::vector<float>{});
      meta.means.maxs = means.value("maxs", std::vector<float>{});
      meta.means.files = means.value("files", std::vector<std::string>{});
    }

    if (j.contains("scales")) {
      auto &scales = j["scales"];
      meta.scales.codebook = scales.value("codebook", std::vector<float>{});
      meta.scales.files = scales.value("files", std::vector<std::string>{});
    }

    if (j.contains("quats")) {
      meta.quats.files = j["quats"].value("files", std::vector<std::string>{});
    }

    if (j.contains("sh0")) {
      auto &sh0 = j["sh0"];
      meta.sh0.codebook = sh0.value("codebook", std::vector<float>{});
      meta.sh0.files = sh0.value("files", std::vector<std::string>{});
    }

    if (j.contains("shN")) {
      auto &shN = j["shN"];
      meta.shN.count = shN.value("count", 0u);
      meta.shN.bands = shN.value("bands", 0u);
      meta.shN.codebook = shN.value("codebook", std::vector<float>{});
      meta.shN.files = shN.value("files", std::vector<std::string>{});
    }
    return true;
  } catch (...) {
    return false;
  }
}

class SogReader : public IGaussReader {
public:
  Expected<GaussianCloudIR> Read(const uint8_t *data, size_t size,
                                 const ReadOptions &options) override {
    SimpleZipReader zip;
    if (!zip.Open(data, size)) {
      return Expected<GaussianCloudIR>(MakeError("SOG: Failed to open ZIP"));
    }

    std::vector<uint8_t> metaData = zip.ExtractFile("meta.json");
    if (metaData.empty()) {
      return Expected<GaussianCloudIR>(
          MakeError("SOG: meta.json not found in archive"));
    }

    SogMeta meta;
    if (!ParseMeta(metaData, meta)) {
      return Expected<GaussianCloudIR>(MakeError("SOG: Failed to parse meta.json"));
    }

    if (meta.version < 2) {
      return Expected<GaussianCloudIR>(
          MakeError("SOG: Version < 2 not supported"));
    }

    GaussianCloudIR ir;
    ir.numPoints = static_cast<int32_t>(meta.count);
    ir.meta.sourceFormat = "sog";
    ir.meta.handedness = Handedness::kRight;
    ir.meta.up = UpAxis::kY;
    ir.meta.color = ColorSpace::kLinear;

    // Decode Positions
    if (meta.means.files.size() >= 2) {
      WebPImage meansL, meansU;
      if (DecodeWebP(zip.ExtractFile(meta.means.files[0]), meansL) &&
          DecodeWebP(zip.ExtractFile(meta.means.files[1]), meansU)) {
        ir.positions.resize(static_cast<size_t>(meta.count) * 3);
        for (uint32_t i = 0; i < meta.count; ++i) {
          uint32_t off = i * 4;
          uint16_t xVal = meansL.rgba[off + 0] | (static_cast<uint16_t>(meansU.rgba[off + 0]) << 8);
          uint16_t yVal = meansL.rgba[off + 1] | (static_cast<uint16_t>(meansU.rgba[off + 1]) << 8);
          uint16_t zVal = meansL.rgba[off + 2] | (static_cast<uint16_t>(meansU.rgba[off + 2]) << 8);

          float x = meta.means.mins[0] + (xVal / 65535.0f) * (meta.means.maxs[0] - meta.means.mins[0]);
          float y = meta.means.mins[1] + (yVal / 65535.0f) * (meta.means.maxs[1] - meta.means.mins[1]);
          float z = meta.means.mins[2] + (zVal / 65535.0f) * (meta.means.maxs[2] - meta.means.mins[2]);

          ir.positions[i * 3 + 0] = InvLogTransform(x);
          ir.positions[i * 3 + 1] = InvLogTransform(y);
          ir.positions[i * 3 + 2] = InvLogTransform(z);
        }
      }
    }

    // Decode Quaternions
    if (!meta.quats.files.empty()) {
      WebPImage quats;
      if (DecodeWebP(zip.ExtractFile(meta.quats.files[0]), quats)) {
        ir.rotations.resize(static_cast<size_t>(meta.count) * 4);
        const float sqrt2 = std::sqrt(2.0f);
        for (uint32_t i = 0; i < meta.count; ++i) {
          uint32_t off = i * 4;
          uint8_t px = quats.rgba[off + 0];
          uint8_t py = quats.rgba[off + 1];
          uint8_t pz = quats.rgba[off + 2];
          uint8_t tag = quats.rgba[off + 3];

          if (tag < 252) {
            ir.rotations[i * 4 + 0] = 1.0f;
            ir.rotations[i * 4 + 1] = 0.0f;
            ir.rotations[i * 4 + 2] = 0.0f;
            ir.rotations[i * 4 + 3] = 0.0f;
            continue;
          }

          int mode = tag - 252;
          float a = ((px / 255.0f) - 0.5f) * sqrt2;
          float b = ((py / 255.0f) - 0.5f) * sqrt2;
          float c = ((pz / 255.0f) - 0.5f) * sqrt2;
          float d = std::sqrt(std::max(0.0f, 1.0f - (a * a + b * b + c * c)));

          float x, y, z, w;
          switch (mode) {
          case 0: x = a; y = b; z = c; w = d; break; // w was max
          case 1: x = d; y = b; z = c; w = a; break; // x was max
          case 2: x = b; y = d; z = c; w = a; break; // y was max
          case 3: x = b; y = c; z = d; w = a; break; // z was max
          default: x = 0; y = 0; z = 0; w = 1; break;
          }
          ir.rotations[i * 4 + 0] = w;
          ir.rotations[i * 4 + 1] = x;
          ir.rotations[i * 4 + 2] = y;
          ir.rotations[i * 4 + 3] = z;
        }
      }
    }

    // Decode Scales
    if (!meta.scales.files.empty() && !meta.scales.codebook.empty()) {
      WebPImage scales;
      if (DecodeWebP(zip.ExtractFile(meta.scales.files[0]), scales)) {
        ir.scales.resize(static_cast<size_t>(meta.count) * 3);
        for (uint32_t i = 0; i < meta.count; ++i) {
          ir.scales[i * 3 + 0] = meta.scales.codebook[scales.rgba[i * 4 + 0]];
          ir.scales[i * 3 + 1] = meta.scales.codebook[scales.rgba[i * 4 + 1]];
          ir.scales[i * 3 + 2] = meta.scales.codebook[scales.rgba[i * 4 + 2]];
        }
      }
    }

    // Decode SH0 + Opacity
    if (!meta.sh0.files.empty() && !meta.sh0.codebook.empty()) {
      WebPImage sh0;
      if (DecodeWebP(zip.ExtractFile(meta.sh0.files[0]), sh0)) {
        ir.colors.resize(static_cast<size_t>(meta.count) * 3);
        ir.alphas.resize(meta.count);
        for (uint32_t i = 0; i < meta.count; ++i) {
          ir.colors[i * 3 + 0] = meta.sh0.codebook[sh0.rgba[i * 4 + 0]];
          ir.colors[i * 3 + 1] = meta.sh0.codebook[sh0.rgba[i * 4 + 1]];
          ir.colors[i * 3 + 2] = meta.sh0.codebook[sh0.rgba[i * 4 + 2]];
          ir.alphas[i] = SigmoidInv(sh0.rgba[i * 4 + 3] / 255.0f);
        }
      }
    }

    // Decode SHN
    if (meta.shN.bands > 0 && meta.shN.files.size() >= 2 && !meta.shN.codebook.empty()) {
      WebPImage centroids, labels;
      if (DecodeWebP(zip.ExtractFile(meta.shN.files[0]), centroids) &&
          DecodeWebP(zip.ExtractFile(meta.shN.files[1]), labels)) {
        uint32_t coeffs[] = {0, 3, 8, 15};
        uint32_t shCoeffs = coeffs[std::min(meta.shN.bands, 3u)];
        ir.sh.resize(static_cast<size_t>(meta.count) * shCoeffs * 3);
        ir.meta.shDegree = static_cast<int>(meta.shN.bands);

        for (uint32_t i = 0; i < meta.count; ++i) {
          uint16_t paletteIdx = labels.rgba[i * 4 + 0] | (static_cast<uint16_t>(labels.rgba[i * 4 + 1]) << 8);
          if (paletteIdx >= meta.shN.count) continue;

          for (uint32_t j = 0; j < shCoeffs; ++j) {
            uint32_t cx = (paletteIdx % 64) * shCoeffs + j;
            uint32_t cy = paletteIdx / 64;
            uint32_t off = (cy * centroids.width + cx) * 4;

            // Convert from INRIA (per-channel groups) to interleaved RGB
            ir.sh[i * shCoeffs * 3 + j * 3 + 0] = meta.shN.codebook[centroids.rgba[off + 0]];
            ir.sh[i * shCoeffs * 3 + j * 3 + 1] = meta.shN.codebook[centroids.rgba[off + 1]];
            ir.sh[i * shCoeffs * 3 + j * 3 + 2] = meta.shN.codebook[centroids.rgba[off + 2]];
          }
        }
      }
    }

    auto err = ValidateBasic(ir, options.strict);
    if (!err.message.empty() && options.strict) return Expected<GaussianCloudIR>(err);
    return Expected<GaussianCloudIR>(std::move(ir));
  }
};

} // namespace

std::unique_ptr<IGaussReader> MakeSogReader() {
  return std::make_unique<SogReader>();
}

} // namespace gf
