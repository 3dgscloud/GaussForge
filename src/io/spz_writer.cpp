#include "gf/io/spz.h"

#include <vector>

#include "load-spz.h"
#include "splat-types.h"

#include "gf/core/errors.h"
#include "gf/core/gauss_ir.h"
#include "gf/core/validate.h"

namespace gf {

namespace {

spz::GaussianCloud ToSpz(const GaussianCloudIR &ir) {
  spz::GaussianCloud g;
  g.numPoints = ir.numPoints;
  g.shDegree = ir.meta.shDegree;
  g.antialiased = ir.meta.antialiased;
  g.positions = ir.positions;
  g.scales = ir.scales;
  g.alphas = ir.alphas;
  g.colors = ir.colors;
  g.sh = ir.sh;

  g.rotations.resize(ir.rotations.size());
  for (int i = 0; i < ir.numPoints; ++i) {
    float w = ir.rotations[i * 4 + 0];
    float x = ir.rotations[i * 4 + 1];
    float y = ir.rotations[i * 4 + 2];
    float z = ir.rotations[i * 4 + 3];

    g.rotations[i * 4 + 0] = x;
    g.rotations[i * 4 + 1] = y;
    g.rotations[i * 4 + 2] = z;
    g.rotations[i * 4 + 3] = w;
  }
  return g;
}

} // namespace

class SpzWriter : public IGaussWriter {
public:
  Expected<std::vector<uint8_t>> Write(const GaussianCloudIR &ir,
                                       const WriteOptions &options) override {
    const auto err = ValidateBasic(ir, options.strict);
    if (!err.message.empty() && options.strict)
      return Expected<std::vector<uint8_t>>(err);

    spz::GaussianCloud g = ToSpz(ir);
    spz::PackOptions pack;
    std::vector<uint8_t> result;
    bool ok = spz::saveSpz(g, pack, &result);
    if (!ok) {
      return Expected<std::vector<uint8_t>>(MakeError("spz write failed"));
    }

    return Expected<std::vector<uint8_t>>(std::move(result));
  }
};

std::unique_ptr<IGaussWriter> MakeSpzWriter() {
  return std::make_unique<SpzWriter>();
}

} // namespace gf
