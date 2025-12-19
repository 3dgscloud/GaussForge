#include "gf/io/ply.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"
#include "gf/core/metadata.h"
#include "gf/core/validate.h"
#include "gf/io/reader.h"

namespace gf {

namespace {

int degreeForDim(int dim) {
  if (dim < 3)
    return 0;
  if (dim < 8)
    return 1;
  if (dim < 15)
    return 2;
  return 3;
}

// Read a line from memory buffer (skip comments)
bool getlineSkipComment(const uint8_t *&data, size_t &remaining,
                        std::string &line) {
  while (remaining > 0) {
    // Find line end
    size_t lineEnd = 0;
    bool found = false;
    for (size_t i = 0; i < remaining; ++i) {
      if (data[i] == '\n') {
        lineEnd = i;
        found = true;
        break;
      }
    }
    if (!found) {
      // No newline found, use all remaining data
      lineEnd = remaining;
    }

    // Extract line content
    line.assign(reinterpret_cast<const char *>(data), lineEnd);
    data += lineEnd + (found ? 1 : 0);
    remaining -= lineEnd + (found ? 1 : 0);

    // Remove leading whitespace
    auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      line.clear();
      continue;
    }
    std::string trimmed = line.substr(start);
    // Skip comment lines
    if (trimmed.rfind("comment", 0) == 0) {
      line.clear();
      continue;
    }
    line = trimmed;
    return true;
  }
  return false;
}

} // namespace

class PlyReader : public IGaussReader {
public:
  Expected<GaussianCloudIR> Read(const uint8_t *data, size_t size,
                                 const ReadOptions &options) override {
    try {
      if (data == nullptr || size == 0) {
        return Expected<GaussianCloudIR>(
            MakeError("ply read failed: empty input"));
      }

      const uint8_t *current = data;
      size_t remaining = size;

      std::string line;
      if (!getlineSkipComment(current, remaining, line) || line != "ply") {
        return Expected<GaussianCloudIR>(MakeError("ply read failed: not ply"));
      }
      if (!getlineSkipComment(current, remaining, line) ||
          line != "format binary_little_endian 1.0") {
        return Expected<GaussianCloudIR>(
            MakeError("ply read failed: unsupported format"));
      }
      if (!getlineSkipComment(current, remaining, line) ||
          line.find("element vertex ") != 0) {
        return Expected<GaussianCloudIR>(
            MakeError("ply read failed: missing vertex count"));
      }
      int numPoints = std::stoi(line.substr(std::strlen("element vertex ")));
      if (numPoints <= 0) {
        return Expected<GaussianCloudIR>(
            MakeError("ply read failed: invalid vertex count"));
      }

      std::unordered_map<std::string, int> fields;
      int propIdx = 0;
      for (;;) {
        if (!getlineSkipComment(current, remaining, line)) {
          return Expected<GaussianCloudIR>(
              MakeError("ply read failed: EOF in header"));
        }
        if (line == "end_header")
          break;
        const std::string prefix = "property float ";
        if (line.rfind(prefix, 0) != 0) {
          return Expected<GaussianCloudIR>(
              MakeError("ply read failed: unsupported property type"));
        }
        std::string name = line.substr(prefix.size());
        fields[name] = propIdx++;
      }

      auto idx = [&fields](const std::string &name) -> int {
        auto it = fields.find(name);
        return it == fields.end() ? -1 : it->second;
      };

      std::vector<int> posIdx = {idx("x"), idx("y"), idx("z")};
      std::vector<int> scaleIdx = {idx("scale_0"), idx("scale_1"),
                                   idx("scale_2")};
      std::vector<int> rotIdx = {idx("rot_0"), idx("rot_1"), idx("rot_2"),
                                 idx("rot_3")};
      int alphaIdx = idx("opacity");
      std::vector<int> colorIdx = {idx("f_dc_0"), idx("f_dc_1"), idx("f_dc_2")};

      for (int v : posIdx)
        if (v < 0)
          return Expected<GaussianCloudIR>(
              MakeError("missing position fields"));
      for (int v : scaleIdx)
        if (v < 0)
          return Expected<GaussianCloudIR>(MakeError("missing scale fields"));
      for (int v : rotIdx)
        if (v < 0)
          return Expected<GaussianCloudIR>(MakeError("missing rot fields"));
      for (int v : colorIdx)
        if (v < 0)
          return Expected<GaussianCloudIR>(MakeError("missing color fields"));
      if (alphaIdx < 0)
        return Expected<GaussianCloudIR>(MakeError("missing opacity field"));

      std::vector<int> shIdx;
      for (int i = 0;; ++i) {
        int v = idx("f_rest_" + std::to_string(i));
        if (v < 0)
          break;
        shIdx.push_back(v);
      }
      int shDim = static_cast<int>(shIdx.size() / 3);

      // Read binary data
      const size_t dataSize =
          static_cast<size_t>(numPoints) * fields.size() * sizeof(float);
      if (remaining < dataSize) {
        return Expected<GaussianCloudIR>(
            MakeError("ply read failed: insufficient data"));
      }

      std::vector<float> values(static_cast<size_t>(numPoints) * fields.size());
      std::memcpy(values.data(), current, dataSize);
      current += dataSize;
      remaining -= dataSize;

      GaussianCloudIR ir;
      ir.numPoints = numPoints;
      ir.meta.shDegree = degreeForDim(shDim);
      ir.meta.sourceFormat = "ply";
      ir.positions.reserve(numPoints * 3);
      ir.scales.reserve(numPoints * 3);
      ir.rotations.reserve(numPoints * 4);
      ir.alphas.reserve(numPoints);
      ir.colors.reserve(numPoints * 3);
      ir.sh.reserve(numPoints * shDim * 3);

      const size_t stride = fields.size();
      for (int i = 0; i < numPoints; ++i) {
        const float *base = &values[static_cast<size_t>(i) * stride];
        for (int j = 0; j < 3; ++j)
          ir.positions.push_back(base[posIdx[j]]);
        for (int j = 0; j < 3; ++j)
          ir.scales.push_back(base[scaleIdx[j]]);
        for (int j = 0; j < 4; ++j)
          ir.rotations.push_back(base[rotIdx[j]]);
        ir.alphas.push_back(base[alphaIdx]);
        for (int j = 0; j < 3; ++j)
          ir.colors.push_back(base[colorIdx[j]]);

        // Store SH coefficients in interleaved RGB order per coefficient.
        for (int j = 0; j < shDim; ++j) {
          ir.sh.push_back(base[shIdx[j]]);             // coeff j, R
          ir.sh.push_back(base[shIdx[j + shDim]]);     // coeff j, G
          ir.sh.push_back(base[shIdx[j + 2 * shDim]]); // coeff j, B
        }
      }

      const auto err = ValidateBasic(ir, options.strict);
      if (!err.message.empty() && options.strict)
        return Expected<GaussianCloudIR>(err);
      return Expected<GaussianCloudIR>(std::move(ir));
    } catch (const std::exception &e) {
      return Expected<GaussianCloudIR>(
          MakeError(std::string("ply read failed: ") + e.what()));
    }
  }
};

std::unique_ptr<IGaussReader> MakePlyReader() {
  return std::make_unique<PlyReader>();
}

} // namespace gf
