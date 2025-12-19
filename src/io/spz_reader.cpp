#include "gf/io/reader.h"
#include "gf/io/spz.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "load-spz.h"
#include "splat-types.h"

#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"
#include "gf/core/metadata.h"
#include "gf/core/validate.h"

namespace gf {

namespace {

GaussianCloudIR ToIR(const spz::GaussianCloud &g) {
  GaussianCloudIR ir;
  ir.numPoints = g.numPoints;
  ir.meta.shDegree = g.shDegree;
  ir.meta.antialiased = g.antialiased;
  ir.meta.sourceFormat = "spz";
  ir.positions = g.positions;
  ir.scales = g.scales;
  ir.alphas = g.alphas;
  ir.colors = g.colors;
  ir.sh = g.sh;

  ir.rotations.resize(g.rotations.size());
  for (size_t i = 0; i < static_cast<size_t>(g.numPoints); ++i) {
    float x = g.rotations[i * 4 + 0];
    float y = g.rotations[i * 4 + 1];
    float z = g.rotations[i * 4 + 2];
    float w = g.rotations[i * 4 + 3];

    ir.rotations[i * 4 + 0] = w;
    ir.rotations[i * 4 + 1] = x;
    ir.rotations[i * 4 + 2] = y;
    ir.rotations[i * 4 + 3] = z;
  }
  return ir;
}

} // namespace

class SpzReader : public IGaussReader {
public:
  Expected<GaussianCloudIR> Read(const uint8_t *data, size_t size,
                                 const ReadOptions &options) override {
    try {
      if (data == nullptr || size == 0) {
        return Expected<GaussianCloudIR>(
            MakeError("spz read failed: empty input"));
      }

      // Use memory buffer version of loadSpz directly
      spz::UnpackOptions unpack;
      spz::GaussianCloud g =
          spz::loadSpz(data, static_cast<int32_t>(size), unpack);

      GaussianCloudIR ir = ToIR(g);
      const auto err = ValidateBasic(ir, options.strict);
      if (!err.message.empty() && options.strict)
        return Expected<GaussianCloudIR>(err);
      return Expected<GaussianCloudIR>(std::move(ir));
    } catch (const std::exception &e) {
      return Expected<GaussianCloudIR>(
          MakeError(std::string("spz read failed: ") + e.what()));
    }
  }
};

std::unique_ptr<IGaussReader> MakeSpzReader() {
  return std::make_unique<SpzReader>();
}

} // namespace gf
