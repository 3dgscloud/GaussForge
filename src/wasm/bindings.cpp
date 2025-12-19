#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include <emscripten/val.h>
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gf/core/gauss_ir.h"
#include "gf/core/validate.h"
#include "gf/io/registry.h"

#ifdef __EMSCRIPTEN__
using namespace emscripten;

namespace {

/**
 * Core optimization: Use typed_memory_view for single-pass fast memory copy
 */
val vectorToUint8Array(const std::vector<uint8_t> &vec) {
  if (vec.empty())
    return val::global("Uint8Array").new_(0);
  // Copy WASM memory view to JS-managed memory via slice()
  return val(typed_memory_view(vec.size(), vec.data())).call<val>("slice");
}

val vectorToFloat32Array(const std::vector<float> &vec) {
  if (vec.empty())
    return val::global("Float32Array").new_(0);
  return val(typed_memory_view(vec.size(), vec.data())).call<val>("slice");
}

/**
 * High-performance IR -> JS conversion
 */
val gaussIRToJS(const gf::GaussianCloudIR &ir) {
  val result = val::object();
  result.set("numPoints", ir.numPoints);

  // Batch map core properties
  result.set("positions", vectorToFloat32Array(ir.positions));
  result.set("scales", vectorToFloat32Array(ir.scales));
  result.set("rotations", vectorToFloat32Array(ir.rotations));
  result.set("alphas", vectorToFloat32Array(ir.alphas));
  result.set("colors", vectorToFloat32Array(ir.colors));
  result.set("sh", vectorToFloat32Array(ir.sh));

  // Handle extras
  val extras = val::object();
  for (const auto &pair : ir.extras) {
    extras.set(pair.first, vectorToFloat32Array(pair.second));
  }
  result.set("extras", extras);

  // Handle metadata
  val meta = val::object();
  meta.set("shDegree", ir.meta.shDegree);
  meta.set("sourceFormat", ir.meta.sourceFormat);
  result.set("meta", meta);

  return result;
}

/**
 * High-performance JS -> IR conversion
 */
gf::GaussianCloudIR jsToGaussIR(val jsIR) {
  gf::GaussianCloudIR ir;
  ir.numPoints = jsIR["numPoints"].as<int32_t>();

  auto fill = [&](val source, std::vector<float> &dest) {
    if (!source.isUndefined() && !source.isNull()) {
      dest = vecFromJSArray<float>(source); // Embind high-speed conversion
    }
  };

  fill(jsIR["positions"], ir.positions);
  fill(jsIR["scales"], ir.scales);
  fill(jsIR["rotations"], ir.rotations);
  fill(jsIR["alphas"], ir.alphas);
  fill(jsIR["colors"], ir.colors);
  fill(jsIR["sh"], ir.sh);

  val ex = jsIR["extras"];
  if (!ex.isUndefined() && !ex.isNull()) {
    val keys = val::global("Object").call<val>("keys", ex);
    unsigned int len = keys["length"].as<unsigned int>();
    for (unsigned int i = 0; i < len; ++i) {
      std::string key = keys[i].as<std::string>();
      ir.extras[key] = vecFromJSArray<float>(ex[key]);
    }
  }

  val m = jsIR["meta"];
  if (!m.isUndefined() && !m.isNull()) {
    ir.meta.shDegree = m["shDegree"].as<int32_t>();
    if (m.hasOwnProperty("sourceFormat")) {
      ir.meta.sourceFormat = m["sourceFormat"].as<std::string>();
    }
  }
  return ir;
}

} // namespace

/**
 * Exported WASM management class
 */
class GaussForgeWASM {
public:
  GaussForgeWASM() : registry_(std::make_unique<gf::IORegistry>()) {}

  val read(val jsData, const std::string &format, bool strict = false) {
    try {
      std::vector<uint8_t> data = vecFromJSArray<uint8_t>(jsData);
      auto *reader = registry_->ReaderForExt(format);
      if (!reader)
        return err("No reader for " + format);

      auto ir_or = reader->Read(data.data(), data.size(), {strict});
      if (!ir_or)
        return err(ir_or.error().message);

      auto validation = gf::ValidateBasic(ir_or.value(), strict);
      if (!validation.message.empty() && strict)
        return err(validation.message);

      val res = val::object();
      res.set("data", gaussIRToJS(ir_or.value()));
      if (!validation.message.empty())
        res.set("warning", validation.message);
      return res;
    } catch (const std::exception &e) {
      return err(e.what());
    }
  }

  val write(val jsIR, const std::string &format, bool strict = false) {
    try {
      auto *writer = registry_->WriterForExt(format);
      if (!writer)
        return err("No writer for " + format);

      auto data_or = writer->Write(jsToGaussIR(jsIR), {strict});
      if (!data_or)
        return err(data_or.error().message);

      val res = val::object();
      res.set("data", vectorToUint8Array(data_or.value()));
      return res;
    } catch (const std::exception &e) {
      return err(e.what());
    }
  }

  val convert(val jsData, const std::string &inF, const std::string &outF,
              bool strict = false) {
    try {
      auto *reader = registry_->ReaderForExt(inF);
      auto *writer = registry_->WriterForExt(outF);
      if (!reader || !writer)
        return err("Format handler not found");

      std::vector<uint8_t> data = vecFromJSArray<uint8_t>(jsData);
      auto ir_or = reader->Read(data.data(), data.size(), {strict});
      if (!ir_or)
        return err(ir_or.error().message);

      auto out_or = writer->Write(ir_or.value(), {strict});
      if (!out_or)
        return err(out_or.error().message);

      val res = val::object();
      res.set("data", vectorToUint8Array(out_or.value()));
      return res;
    } catch (const std::exception &e) {
      return err(e.what());
    }
  }

  val getSupportedFormats() {
    val f = val::array();
    for (auto &s : {"ply", "compressed.ply", "splat", "ksplat", "spz"})
      f.call<void>("push", val(s));
    return f;
  }

private:
  std::unique_ptr<gf::IORegistry> registry_;
  val err(const std::string &m) {
    val e = val::object();
    e.set("error", m);
    return e;
  }
};

EMSCRIPTEN_BINDINGS(gauss_forge) {
  class_<GaussForgeWASM>("GaussForgeWASM")
      .constructor<>()
      .function("read", &GaussForgeWASM::read)
      .function("write", &GaussForgeWASM::write)
      .function("convert", &GaussForgeWASM::convert)
      .function("getSupportedFormats", &GaussForgeWASM::getSupportedFormats);
}
#endif