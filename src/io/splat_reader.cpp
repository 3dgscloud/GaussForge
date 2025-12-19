
#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"
#include "gf/core/validate.h"
#include "gf/io/reader.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gf {

namespace {

constexpr int BYTES_PER_SPLAT = 32;
constexpr float SH_C0 = 0.28209479177387814f;
constexpr float MAX_LOGIT = 10.0f;

// Read little-endian float32
float ReadFloat32LE(const uint8_t *data) {
  uint32_t bits = static_cast<uint32_t>(data[0]) |
                  (static_cast<uint32_t>(data[1]) << 8) |
                  (static_cast<uint32_t>(data[2]) << 16) |
                  (static_cast<uint32_t>(data[3]) << 24);
  return *reinterpret_cast<const float *>(&bits);
}

} // namespace

class SplatReader : public IGaussReader {
public:
  Expected<GaussianCloudIR> Read(const uint8_t *data, size_t size,
                                 const ReadOptions &options) override {
    try {
      if (data == nullptr || size == 0) {
        return Expected<GaussianCloudIR>(
            MakeError("splat read failed: empty input"));
      }

      // Check if file size is a multiple of 32 bytes
      if (size % BYTES_PER_SPLAT != 0) {
        return Expected<GaussianCloudIR>(
            MakeError("splat read failed: file size is not a multiple of 32 "
                      "bytes"));
      }

      const size_t numSplats = size / BYTES_PER_SPLAT;
      if (numSplats == 0) {
        return Expected<GaussianCloudIR>(
            MakeError("splat read failed: file is empty"));
      }

      // Initialize IR
      GaussianCloudIR ir;
      ir.numPoints = static_cast<int32_t>(numSplats);
      ir.meta.shDegree = 0; // .splat format doesn't have higher-order SH
      ir.meta.sourceFormat = "splat";

      ir.positions.reserve(numSplats * 3);
      ir.scales.reserve(numSplats * 3);
      ir.rotations.reserve(numSplats * 4);
      ir.alphas.reserve(numSplats);
      ir.colors.reserve(numSplats * 3);

      // Process data in chunks for better performance
      const size_t chunkSize = 1024;
      const size_t numChunks = (numSplats + chunkSize - 1) / chunkSize;

      for (size_t c = 0; c < numChunks; ++c) {
        const size_t numRows = std::min(chunkSize, numSplats - c * chunkSize);
        const size_t bytesToRead = numRows * BYTES_PER_SPLAT;
        const size_t chunkStart = c * chunkSize * BYTES_PER_SPLAT;

        if (chunkStart + bytesToRead > size) {
          return Expected<GaussianCloudIR>(
              MakeError("splat read failed: insufficient data"));
        }

        // Parse each splat in the chunk
        for (size_t r = 0; r < numRows; ++r) {
          const size_t offset = chunkStart + r * BYTES_PER_SPLAT;
          const uint8_t *splatBytes = &data[offset];

          // Read position (3 × float32, offset 0-11)
          const float x = ReadFloat32LE(&splatBytes[0]);
          const float y = ReadFloat32LE(&splatBytes[4]);
          const float z = ReadFloat32LE(&splatBytes[8]);

          // Read scale (3 × float32, offset 12-23)
          const float scaleX = ReadFloat32LE(&splatBytes[12]);
          const float scaleY = ReadFloat32LE(&splatBytes[16]);
          const float scaleZ = ReadFloat32LE(&splatBytes[20]);

          // Read color and opacity (4 × uint8, offset 24-27)
          const uint8_t red = splatBytes[24];
          const uint8_t green = splatBytes[25];
          const uint8_t blue = splatBytes[26];
          const uint8_t opacity = splatBytes[27];

          // Read rotation quaternion (4 × uint8, offset 28-31)
          const uint8_t rot0 = splatBytes[28];
          const uint8_t rot1 = splatBytes[29];
          const uint8_t rot2 = splatBytes[30];
          const uint8_t rot3 = splatBytes[31];

          // Store position
          ir.positions.push_back(x);
          ir.positions.push_back(y);
          ir.positions.push_back(z);

          // Store scale (convert from linear to log scale)
          ir.scales.push_back(scaleX > 0.0f ? std::log(scaleX) : -10.0f);
          ir.scales.push_back(scaleY > 0.0f ? std::log(scaleY) : -10.0f);
          ir.scales.push_back(scaleZ > 0.0f ? std::log(scaleZ) : -10.0f);

          // Store color (convert from uint8 back to spherical harmonics)
          ir.colors.push_back((red / 255.0f - 0.5f) / SH_C0);
          ir.colors.push_back((green / 255.0f - 0.5f) / SH_C0);
          ir.colors.push_back((blue / 255.0f - 0.5f) / SH_C0);

          // Store opacity (convert from uint8 to float and apply inverse
          // sigmoid)
          // Match supersplat-main formula but clamp to finite range to avoid
          // infinities at 0/255 which can cause rendering issues.
          if (opacity == 0) {
            ir.alphas.push_back(-MAX_LOGIT);
          } else if (opacity == 255) {
            ir.alphas.push_back(MAX_LOGIT);
          } else {
            // -Math.log(255 / opacity - 1) in JS
            float alpha =
                -std::log(255.0f / static_cast<float>(opacity) - 1.0f);
            ir.alphas.push_back(std::clamp(alpha, -MAX_LOGIT, MAX_LOGIT));
          }

          // Store rotation quaternion (convert from uint8 [0,255] to float
          // [-1,1] and normalize)
          // .splat format stores quaternion as [w, x, y, z] (same as IR format)
          // Match supersplat-main: (value - 128) / 128
          const float rotW = (static_cast<float>(rot0) - 128.0f) / 128.0f;
          const float rotX = (static_cast<float>(rot1) - 128.0f) / 128.0f;
          const float rotY = (static_cast<float>(rot2) - 128.0f) / 128.0f;
          const float rotZ = (static_cast<float>(rot3) - 128.0f) / 128.0f;

          // Normalize quaternion
          const float length =
              std::sqrt(rotX * rotX + rotY * rotY + rotZ * rotZ + rotW * rotW);
          if (length > 0.0f) {
            const float invLength = 1.0f / length;
            // Store in IR format as [w, x, y, z]
            ir.rotations.push_back(rotW * invLength); // w
            ir.rotations.push_back(rotX * invLength); // x
            ir.rotations.push_back(rotY * invLength); // y
            ir.rotations.push_back(rotZ * invLength); // z
          } else {
            // Default to identity quaternion if invalid
            ir.rotations.push_back(1.0f); // w
            ir.rotations.push_back(0.0f); // x
            ir.rotations.push_back(0.0f); // y
            ir.rotations.push_back(0.0f); // z
          }
        }
      }

      const auto err = ValidateBasic(ir, options.strict);
      if (!err.message.empty() && options.strict) {
        return Expected<GaussianCloudIR>(err);
      }

      return Expected<GaussianCloudIR>(std::move(ir));
    } catch (const std::exception &e) {
      return Expected<GaussianCloudIR>(
          MakeError(std::string("splat read failed: ") + e.what()));
    }
  }
};

std::unique_ptr<IGaussReader> MakeSplatReader() {
  return std::make_unique<SplatReader>();
}

} // namespace gf
