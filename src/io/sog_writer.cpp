#include "gf/io/sog.h"
#include "zip_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <webp/encode.h>
#include <zlib.h>

#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"
#include "gf/core/validate.h"

using json = nlohmann::json;

namespace gf {

namespace {

// --- Simple ZIP Writer ---

class SimpleZipWriter {
public:
  void AddFile(const std::string &name, const std::vector<uint8_t> &data) {
    FileEntry e;
    e.name = name;
    e.offset = static_cast<uint32_t>(buffer_.size());
    e.size = static_cast<uint32_t>(data.size());
    e.crc = crc32(0, data.data(), static_cast<uInt>(data.size()));

    zip::LocalFileHeader lh;
    lh.signature = zip::kLocalFileHeaderSig;
    lh.versionNeeded = 20;
    lh.flags = 0;
    lh.compression = 0; // 0 = stored
    lh.modTime = 0;
    lh.modDate = 0;
    lh.crc32 = e.crc;
    lh.compressedSize = e.size;
    lh.uncompressedSize = e.size;
    lh.fileNameLength = static_cast<uint16_t>(name.size());
    lh.extraFieldLength = 0;

    const uint8_t *lhp = reinterpret_cast<const uint8_t *>(&lh);
    buffer_.insert(buffer_.end(), lhp, lhp + sizeof(lh));
    buffer_.insert(buffer_.end(), name.begin(), name.end());
    buffer_.insert(buffer_.end(), data.begin(), data.end());

    entries_.push_back(e);
  }

  std::vector<uint8_t> Finalize() {
    uint32_t cdOffset = static_cast<uint32_t>(buffer_.size());
    for (const auto &e : entries_) {
      zip::CentralDirHeader ch;
      ch.signature = zip::kCentralDirHeaderSig;
      ch.versionMade = 20;
      ch.versionNeeded = 20;
      ch.flags = 0;
      ch.compression = 0;
      ch.modTime = 0;
      ch.modDate = 0;
      ch.crc32 = e.crc;
      ch.compressedSize = e.size;
      ch.uncompressedSize = e.size;
      ch.fileNameLength = static_cast<uint16_t>(e.name.size());
      ch.extraFieldLength = 0;
      ch.commentLength = 0;
      ch.diskStart = 0;
      ch.internalAttrs = 0;
      ch.externalAttrs = 0;
      ch.localHeaderOffset = e.offset;

      const uint8_t *chp = reinterpret_cast<const uint8_t *>(&ch);
      buffer_.insert(buffer_.end(), chp, chp + sizeof(ch));
      buffer_.insert(buffer_.end(), e.name.begin(), e.name.end());
    }
    uint32_t cdSize = static_cast<uint32_t>(buffer_.size()) - cdOffset;

    zip::EndOfCentralDir eocd;
    eocd.signature = zip::kEndOfCentralDirSig;
    eocd.diskNumber = 0;
    eocd.diskWithCentralDir = 0;
    eocd.numEntriesThisDisk = static_cast<uint16_t>(entries_.size());
    eocd.numEntriesTotal = static_cast<uint16_t>(entries_.size());
    eocd.centralDirSize = cdSize;
    eocd.centralDirOffset = cdOffset;
    eocd.commentLength = 0;

    const uint8_t *ep = reinterpret_cast<const uint8_t *>(&eocd);
    buffer_.insert(buffer_.end(), ep, ep + sizeof(eocd));

    return std::move(buffer_);
  }

private:
  struct FileEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t crc;
  };
  std::vector<FileEntry> entries_;
  std::vector<uint8_t> buffer_;
};

// --- First-Principles K-Means (1D) ---

std::vector<float> Generate1DCodebook(const std::vector<float> &data,
                                      int centers,
                                      std::vector<uint8_t> &indices) {
  if (data.empty()) {
    indices.clear();
    return std::vector<float>(static_cast<size_t>(centers), 0.0f);
  }

  const size_t n = data.size();
  std::vector<float> centroids(static_cast<size_t>(centers));

  // Linear initialization
  auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());
  float min_v = *min_it;
  float max_v = *max_it;
  float range = max_v - min_v;
  for (int i = 0; i < centers; ++i) {
    centroids[static_cast<size_t>(i)] =
        min_v + (static_cast<float>(i) /
                 static_cast<float>(centers > 1 ? centers - 1 : 1)) *
                    range;
  }

  indices.resize(n);
  std::vector<float> next_centroids(static_cast<size_t>(centers));
  std::vector<int> counts(static_cast<size_t>(centers));

  for (int iter = 0; iter < 10; ++iter) {
    std::fill(counts.begin(), counts.end(), 0);
    std::fill(next_centroids.begin(), next_centroids.end(), 0.0f);

    for (size_t i = 0; i < n; ++i) {
      float min_d = 1e30f;
      int best_k = 0;
      for (int k = 0; k < centers; ++k) {
        float d = std::abs(data[i] - centroids[static_cast<size_t>(k)]);
        if (d < min_d) {
          min_d = d;
          best_k = k;
        }
      }
      indices[i] = static_cast<uint8_t>(best_k);
      next_centroids[static_cast<size_t>(best_k)] += data[i];
      counts[static_cast<size_t>(best_k)]++;
    }

    for (int k = 0; k < centers; ++k) {
      if (counts[static_cast<size_t>(k)] > 0) {
        centroids[static_cast<size_t>(k)] =
            next_centroids[static_cast<size_t>(k)] /
            static_cast<float>(counts[static_cast<size_t>(k)]);
      }
    }
  }

  return centroids;
}

// --- Morton Encoding ---
// Reference: https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// Bit expansion: 10-bit -> 30-bit for Morton code calculation
inline uint32_t Part1By2(uint32_t x) {
  x &= 0x000003ff; // Keep only the lower 10 bits
  x = (x ^ (x << 16)) & 0xff0000ff;
  x = (x ^ (x << 8)) & 0x0300f00f;
  x = (x ^ (x << 4)) & 0x030c30c3;
  x = (x ^ (x << 2)) & 0x09249249;
  return x;
}

// Encode 3D coordinates into a Morton code (Z-order curve)
inline uint32_t EncodeMorton3(uint32_t x, uint32_t y, uint32_t z) {
  return (Part1By2(z) << 2) + (Part1By2(y) << 1) + Part1By2(x);
}

// Recursive Morton sort (internal implementation)
void SortMortonOrderRecursive(const std::vector<float> &positions,
                              std::vector<uint32_t> &indices, size_t start,
                              size_t end) {

  if (end - start <= 1)
    return;

  // 1. Calculate bounding box for current subset
  float mx = 1e30f, my = 1e30f, mz = 1e30f;
  float Mx = -1e30f, My = -1e30f, Mz = -1e30f;

  for (size_t i = start; i < end; ++i) {
    const float *p = &positions[indices[i] * 3];
    mx = std::min(mx, p[0]);
    Mx = std::max(Mx, p[0]);
    my = std::min(my, p[1]);
    My = std::max(My, p[1]);
    mz = std::min(mz, p[2]);
    Mz = std::max(Mz, p[2]);
  }

  float xlen = Mx - mx, ylen = My - my, zlen = Mz - mz;

  // Boundary check
  if (!std::isfinite(xlen) || !std::isfinite(ylen) || !std::isfinite(zlen))
    return;
  if (xlen == 0 && ylen == 0 && zlen == 0)
    return;

  // 2. Calculate quantization factors (10-bit precision)
  float xmul = (xlen > 1e-8f) ? 1024.0f / xlen : 0.0f;
  float ymul = (ylen > 1e-8f) ? 1024.0f / ylen : 0.0f;
  float zmul = (zlen > 1e-8f) ? 1024.0f / zlen : 0.0f;

  // 3. Compute Morton codes
  std::vector<uint32_t> morton(end - start);
  for (size_t i = start; i < end; ++i) {
    const float *p = &positions[indices[i] * 3];
    uint32_t ix = std::min(1023u, static_cast<uint32_t>((p[0] - mx) * xmul));
    uint32_t iy = std::min(1023u, static_cast<uint32_t>((p[1] - my) * ymul));
    uint32_t iz = std::min(1023u, static_cast<uint32_t>((p[2] - mz) * zmul));
    morton[i - start] = EncodeMorton3(ix, iy, iz);
  }

  // 4. Create order array for sorting
  std::vector<uint32_t> order(end - start);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&morton](uint32_t a, uint32_t b) {
    return morton[a] < morton[b];
  });

  // 5. Reorder indices
  std::vector<uint32_t> tmp(end - start);
  for (size_t i = 0; i < end - start; ++i) {
    tmp[i] = indices[start + order[i]];
  }
  for (size_t i = 0; i < end - start; ++i) {
    indices[start + i] = tmp[i];
  }

  // 6. Recursively process large buckets (same Morton code, count > 256)
  size_t s = 0;
  while (s < end - start) {
    size_t e = s + 1;
    while (e < end - start && morton[order[e]] == morton[order[s]]) {
      ++e;
    }
    if (e - s > 256) {
      SortMortonOrderRecursive(positions, indices, start + s, start + e);
    }
    s = e;
  }
}

// Generate Morton-sorted index array
std::vector<uint32_t> GenerateMortonOrder(const std::vector<float> &positions,
                                          int32_t numPoints) {

  std::vector<uint32_t> indices(static_cast<size_t>(numPoints));
  std::iota(indices.begin(), indices.end(), 0);
  SortMortonOrderRecursive(positions, indices, 0,
                           static_cast<size_t>(numPoints));
  return indices;
}

// --- SOG Encoding Helpers ---

inline float LogTransform(float v) {
  return (v < 0.0f) ? -std::log(std::abs(v) + 1.0f) : std::log(v + 1.0f);
}

std::vector<uint8_t> EncodeWebPLossless(const std::vector<uint8_t> &rgba,
                                        int width, int height) {
  uint8_t *output = nullptr;
  size_t size =
      WebPEncodeLosslessRGBA(rgba.data(), width, height, width * 4, &output);
  if (size == 0 || !output)
    return {};
  std::vector<uint8_t> result(output, output + size);
  WebPFree(output);
  return result;
}

void EncodeQuaternion(float w, float x, float y, float z, uint8_t out[4]) {
  float q[4] = {w, x, y, z};
  int max_idx = 0;
  float max_val = std::abs(q[0]);
  for (int i = 1; i < 4; ++i) {
    if (std::abs(q[i]) > max_val) {
      max_val = std::abs(q[i]);
      max_idx = i;
    }
  }
  if (q[max_idx] < 0.0f) {
    for (int i = 0; i < 4; ++i)
      q[i] = -q[i];
  }

  const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == max_idx)
      continue;
    float val = std::clamp((q[i] / inv_sqrt2 + 1.0f) * 0.5f, 0.0f, 1.0f);
    out[count++] = static_cast<uint8_t>(std::round(val * 255.0f));
  }
  out[3] = static_cast<uint8_t>(252 + max_idx);
}

class SogWriter : public IGaussWriter {
public:
  Expected<std::vector<uint8_t>> Write(const GaussianCloudIR &ir,
                                       const WriteOptions &options) override {
    if (ir.numPoints <= 0)
      return MakeError("SOG: Empty cloud");

    uint32_t count = static_cast<uint32_t>(ir.numPoints);
    int width =
        static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
    int height = (static_cast<int>(count) + width - 1) / width;
    size_t tex_size = static_cast<size_t>(width) * static_cast<size_t>(height);

    SimpleZipWriter zip;
    json meta_json;
    meta_json["version"] = 2;
    meta_json["count"] = count;
    meta_json["antialias"] = ir.meta.antialiased;

    // Morton sort: spatially adjacent points become texture-adjacent, improving compression
    std::vector<uint32_t> morton_indices =
        GenerateMortonOrder(ir.positions, ir.numPoints);

    // 1. Positions (Log + 16-bit)
    std::vector<float> log_pos(static_cast<size_t>(count) * 3);
    float mins[3] = {1e30f, 1e30f, 1e30f}, maxs[3] = {-1e30f, -1e30f, -1e30f};
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t idx = morton_indices[i];
      for (int d = 0; d < 3; ++d) {
        float v = LogTransform(ir.positions[static_cast<size_t>(idx) * 3 +
                                            static_cast<size_t>(d)]);
        log_pos[static_cast<size_t>(i) * 3 + static_cast<size_t>(d)] = v;
        mins[d] = std::min(mins[d], v);
        maxs[d] = std::max(maxs[d], v);
      }
    }
    meta_json["means"]["mins"] = {mins[0], mins[1], mins[2]};
    meta_json["means"]["maxs"] = {maxs[0], maxs[1], maxs[2]};
    meta_json["means"]["files"] = {"means_l.webp", "means_u.webp"};

    std::vector<uint8_t> means_l(tex_size * 4, 0), means_u(tex_size * 4, 0);
    for (uint32_t i = 0; i < count; ++i) {
      for (int d = 0; d < 3; ++d) {
        float range = maxs[d] - mins[d];
        float n = (range > 1e-8f) ? (log_pos[static_cast<size_t>(i) * 3 +
                                             static_cast<size_t>(d)] -
                                     mins[d]) /
                                        range
                                  : 0.0f;
        uint16_t q =
            static_cast<uint16_t>(std::clamp(n * 65535.0f, 0.0f, 65535.0f));
        means_l[static_cast<size_t>(i) * 4 + static_cast<size_t>(d)] =
            static_cast<uint8_t>(q & 0xFF);
        means_u[static_cast<size_t>(i) * 4 + static_cast<size_t>(d)] =
            static_cast<uint8_t>((q >> 8) & 0xFF);
      }
      means_l[static_cast<size_t>(i) * 4 + 3] = 255;
      means_u[static_cast<size_t>(i) * 4 + 3] = 255;
    }
    zip.AddFile("means_l.webp", EncodeWebPLossless(means_l, width, height));
    zip.AddFile("means_u.webp", EncodeWebPLossless(means_u, width, height));

    // 2. Quats
    std::vector<uint8_t> quats_rgba(tex_size * 4, 0);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t idx = morton_indices[i];
      EncodeQuaternion(ir.rotations[static_cast<size_t>(idx) * 4 + 0],
                       ir.rotations[static_cast<size_t>(idx) * 4 + 1],
                       ir.rotations[static_cast<size_t>(idx) * 4 + 2],
                       ir.rotations[static_cast<size_t>(idx) * 4 + 3],
                       &quats_rgba[static_cast<size_t>(i) * 4]);
    }
    meta_json["quats"]["files"] = {"quats.webp"};
    zip.AddFile("quats.webp", EncodeWebPLossless(quats_rgba, width, height));

    // 3. Scales (1D Codebook)
    std::vector<uint8_t> scale_indices;
    std::vector<float> scale_cb =
        Generate1DCodebook(ir.scales, 256, scale_indices);
    meta_json["scales"]["codebook"] = scale_cb;
    meta_json["scales"]["files"] = {"scales.webp"};
    std::vector<uint8_t> scales_rgba(tex_size * 4, 0);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t idx = morton_indices[i];
      scales_rgba[static_cast<size_t>(i) * 4 + 0] =
          scale_indices[static_cast<size_t>(idx) * 3 + 0];
      scales_rgba[static_cast<size_t>(i) * 4 + 1] =
          scale_indices[static_cast<size_t>(idx) * 3 + 1];
      scales_rgba[static_cast<size_t>(i) * 4 + 2] =
          scale_indices[static_cast<size_t>(idx) * 3 + 2];
      scales_rgba[static_cast<size_t>(i) * 4 + 3] = 255;
    }
    zip.AddFile("scales.webp", EncodeWebPLossless(scales_rgba, width, height));

    // 4. SH0 + Opacity (1D Codebook)
    std::vector<uint8_t> sh0_indices;
    std::vector<float> sh0_cb = Generate1DCodebook(ir.colors, 256, sh0_indices);
    meta_json["sh0"]["codebook"] = sh0_cb;
    meta_json["sh0"]["files"] = {"sh0.webp"};
    std::vector<uint8_t> sh0_rgba(tex_size * 4, 0);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t idx = morton_indices[i];
      sh0_rgba[static_cast<size_t>(i) * 4 + 0] =
          sh0_indices[static_cast<size_t>(idx) * 3 + 0];
      sh0_rgba[static_cast<size_t>(i) * 4 + 1] =
          sh0_indices[static_cast<size_t>(idx) * 3 + 1];
      sh0_rgba[static_cast<size_t>(i) * 4 + 2] =
          sh0_indices[static_cast<size_t>(idx) * 3 + 2];
      float op = 1.0f / (1.0f + std::exp(-ir.alphas[static_cast<size_t>(idx)]));
      sh0_rgba[static_cast<size_t>(i) * 4 + 3] =
          static_cast<uint8_t>(std::clamp(op * 255.0f, 0.0f, 255.0f));
    }
    zip.AddFile("sh0.webp", EncodeWebPLossless(sh0_rgba, width, height));

    std::string meta_str = meta_json.dump(2);
    std::vector<uint8_t> meta_bytes(meta_str.begin(), meta_str.end());
    zip.AddFile("meta.json", meta_bytes);

    return zip.Finalize();
  }
};

} // namespace

std::unique_ptr<IGaussWriter> MakeSogWriter() {
  return std::make_unique<SogWriter>();
}

} // namespace gf