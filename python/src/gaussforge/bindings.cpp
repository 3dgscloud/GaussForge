#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/unordered_map.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gf/core/gauss_ir.h"
#include "gf/core/model_info.h"
#include "gf/core/validate.h"
#include "gf/core/version.h"
#include "gf/io/registry.h"

namespace nb = nanobind;

namespace {

/**
 * Convert GaussianCloudIR to Python dict
 * Returns bytes for float arrays (zero-copy friendly)
 */
nb::dict gaussIRToPy(const gf::GaussianCloudIR &ir) {
    nb::dict result;

    result["numPoints"] = ir.numPoints;

    // Convert float vectors to bytes for efficient transfer
    auto floatVecToBytes = [](const std::vector<float> &vec) -> nb::bytes {
        if (vec.empty())
            return nb::bytes();
        return nb::bytes(reinterpret_cast<const char *>(vec.data()),
                         vec.size() * sizeof(float));
    };

    result["positions"] = floatVecToBytes(ir.positions);
    result["scales"] = floatVecToBytes(ir.scales);
    result["rotations"] = floatVecToBytes(ir.rotations);
    result["alphas"] = floatVecToBytes(ir.alphas);
    result["colors"] = floatVecToBytes(ir.colors);
    result["sh"] = floatVecToBytes(ir.sh);

    // Handle extras
    nb::dict extras;
    for (const auto &pair : ir.extras) {
        extras[pair.first.c_str()] = floatVecToBytes(pair.second);
    }
    result["extras"] = extras;

    // Handle metadata
    nb::dict meta;
    meta["shDegree"] = ir.meta.shDegree;
    meta["sourceFormat"] = ir.meta.sourceFormat;
    result["meta"] = meta;

    return result;
}

/**
 * Convert Python dict to GaussianCloudIR
 */
gf::GaussianCloudIR pyToGaussIR(nb::dict pyIR) {
    gf::GaussianCloudIR ir;
    ir.numPoints = nb::cast<int32_t>(pyIR["numPoints"]);

    auto fill = [&](const char *key, std::vector<float> &dest) {
        if (pyIR.contains(key)) {
            nb::bytes data = nb::cast<nb::bytes>(pyIR[key]);
            if (data.size() > 0) {
                const float *ptr = reinterpret_cast<const float *>(data.c_str());
                size_t count = data.size() / sizeof(float);
                dest.assign(ptr, ptr + count);
            }
        }
    };

    fill("positions", ir.positions);
    fill("scales", ir.scales);
    fill("rotations", ir.rotations);
    fill("alphas", ir.alphas);
    fill("colors", ir.colors);
    fill("sh", ir.sh);

    if (pyIR.contains("extras")) {
        nb::dict ex = nb::cast<nb::dict>(pyIR["extras"]);
        for (auto item : ex) {
            std::string key = nb::cast<std::string>(item.first);
            nb::bytes data = nb::cast<nb::bytes>(item.second);
            if (data.size() > 0) {
                const float *ptr = reinterpret_cast<const float *>(data.c_str());
                size_t count = data.size() / sizeof(float);
                ir.extras[key].assign(ptr, ptr + count);
            }
        }
    }

    if (pyIR.contains("meta")) {
        nb::dict m = nb::cast<nb::dict>(pyIR["meta"]);
        if (m.contains("shDegree")) {
            ir.meta.shDegree = nb::cast<int32_t>(m["shDegree"]);
        }
        if (m.contains("sourceFormat")) {
            ir.meta.sourceFormat = nb::cast<std::string>(m["sourceFormat"]);
        }
    }

    return ir;
}

/**
 * Convert ModelInfo to Python dict
 */
nb::dict modelInfoToPy(const gf::ModelInfo &info) {
    nb::dict result;

    // Basic info
    nb::dict basic;
    basic["numPoints"] = info.numPoints;
    if (info.fileSize > 0)
        basic["fileSize"] = static_cast<double>(info.fileSize);
    if (!info.sourceFormat.empty())
        basic["sourceFormat"] = info.sourceFormat;
    result["basic"] = basic;

    // Rendering properties
    nb::dict rendering;
    rendering["shDegree"] = info.shDegree;
    rendering["antialiased"] = info.antialiased;
    result["rendering"] = rendering;

    // Metadata
    nb::dict meta;
    meta["handedness"] = gf::HandednessToString(info.handedness);
    meta["upAxis"] = gf::UpAxisToString(info.upAxis);
    meta["unit"] = gf::LengthUnitToString(info.unit);
    meta["colorSpace"] = gf::ColorSpaceToString(info.colorSpace);
    result["meta"] = meta;

    // Geometry statistics
    if (info.numPoints > 0) {
        nb::dict bounds;
        bounds["x"] = nb::make_tuple(info.bounds.minX, info.bounds.maxX);
        bounds["y"] = nb::make_tuple(info.bounds.minY, info.bounds.maxY);
        bounds["z"] = nb::make_tuple(info.bounds.minZ, info.bounds.maxZ);
        result["bounds"] = bounds;
    }

    // Scale statistics
    if (info.scaleStats.count > 0) {
        nb::dict scaleStats;
        scaleStats["min"] = info.scaleStats.min;
        scaleStats["max"] = info.scaleStats.max;
        scaleStats["avg"] = info.scaleStats.avg;
        result["scaleStats"] = scaleStats;
    }

    // Alpha statistics
    if (info.alphaStats.count > 0) {
        nb::dict alphaStats;
        alphaStats["min"] = info.alphaStats.min;
        alphaStats["max"] = info.alphaStats.max;
        alphaStats["avg"] = info.alphaStats.avg;
        result["alphaStats"] = alphaStats;
    }

    // Data size breakdown
    nb::dict sizes;
    sizes["positions"] = gf::FormatBytes(info.positionsSize);
    sizes["scales"] = gf::FormatBytes(info.scalesSize);
    sizes["rotations"] = gf::FormatBytes(info.rotationsSize);
    sizes["alphas"] = gf::FormatBytes(info.alphasSize);
    sizes["colors"] = gf::FormatBytes(info.colorsSize);
    sizes["sh"] = gf::FormatBytes(info.shSize);
    sizes["total"] = gf::FormatBytes(info.totalSize);
    result["sizes"] = sizes;

    // Extra attributes
    if (!info.extraAttrs.empty()) {
        nb::dict extras;
        for (const auto &[name, size] : info.extraAttrs) {
            extras[name.c_str()] = gf::FormatBytes(size);
        }
        result["extraAttrs"] = extras;
    }

    return result;
}

} // namespace

/**
 * GaussForge Python binding class
 */
class GaussForgePy {
public:
    GaussForgePy() : registry_(std::make_unique<gf::IORegistry>()) {}

    nb::dict read(nb::bytes pyData, const std::string &format,
                  bool strict = false) {
        try {
            const uint8_t *data =
                reinterpret_cast<const uint8_t *>(pyData.c_str());
            size_t size = pyData.size();

            auto *reader = registry_->ReaderForExt(format);
            if (!reader)
                return err("No reader for " + format);

            auto ir_or = reader->Read(data, size, {strict});
            if (!ir_or)
                return err(ir_or.error().message);

            auto validation = gf::ValidateBasic(ir_or.value(), strict);
            if (!validation.message.empty() && strict)
                return err(validation.message);

            nb::dict res;
            res["data"] = gaussIRToPy(ir_or.value());
            if (!validation.message.empty())
                res["warning"] = validation.message;
            return res;
        } catch (const std::exception &e) {
            return err(e.what());
        }
    }

    nb::dict write(nb::dict pyIR, const std::string &format,
                   bool strict = false) {
        try {
            auto *writer = registry_->WriterForExt(format);
            if (!writer)
                return err("No writer for " + format);

            auto data_or = writer->Write(pyToGaussIR(pyIR), {strict});
            if (!data_or)
                return err(data_or.error().message);

            nb::dict res;
            res["data"] = nb::bytes(
                reinterpret_cast<const char *>(data_or.value().data()),
                data_or.value().size());
            return res;
        } catch (const std::exception &e) {
            return err(e.what());
        }
    }

    nb::dict convert(nb::bytes pyData, const std::string &inFormat,
                     const std::string &outFormat, bool strict = false) {
        try {
            auto *reader = registry_->ReaderForExt(inFormat);
            auto *writer = registry_->WriterForExt(outFormat);
            if (!reader || !writer)
                return err("Format handler not found");

            const uint8_t *data =
                reinterpret_cast<const uint8_t *>(pyData.c_str());
            size_t size = pyData.size();

            auto ir_or = reader->Read(data, size, {strict});
            if (!ir_or)
                return err(ir_or.error().message);

            auto out_or = writer->Write(ir_or.value(), {strict});
            if (!out_or)
                return err(out_or.error().message);

            nb::dict res;
            res["data"] = nb::bytes(
                reinterpret_cast<const char *>(out_or.value().data()),
                out_or.value().size());
            return res;
        } catch (const std::exception &e) {
            return err(e.what());
        }
    }

    nb::list getSupportedFormats() {
        nb::list formats;
        for (auto &s : {"ply", "compressed.ply", "splat", "ksplat", "spz", "sog"})
            formats.append(s);
        return formats;
    }

    nb::dict getModelInfo(nb::bytes pyData, const std::string &format,
                          size_t fileSize = 0) {
        try {
            const uint8_t *data =
                reinterpret_cast<const uint8_t *>(pyData.c_str());
            size_t size = pyData.size();

            auto *reader = registry_->ReaderForExt(format);
            if (!reader)
                return err("No reader for " + format);

            auto ir_or = reader->Read(data, size, {/*strict=*/false});
            if (!ir_or)
                return err(ir_or.error().message);

            gf::ModelInfo info = gf::GetModelInfo(ir_or.value(), fileSize);

            nb::dict res;
            res["data"] = modelInfoToPy(info);
            return res;
        } catch (const std::exception &e) {
            return err(e.what());
        }
    }

    std::string getVersion() { return GAUSS_FORGE_VERSION_STRING; }

private:
    std::unique_ptr<gf::IORegistry> registry_;

    nb::dict err(const std::string &m) {
        nb::dict e;
        e["error"] = m;
        return e;
    }
};

NB_MODULE(_core, m) {
    m.doc() = "GaussForge - High-performance Gaussian Splatting format "
              "conversion library";

    nb::class_<GaussForgePy>(
        m, "GaussForge",
        "Main class for Gaussian Splatting format conversion")
        .def(nb::init<>(), "Create a new GaussForge instance")
        .def("read", &GaussForgePy::read, nb::arg("data"), nb::arg("format"),
             nb::arg("strict") = false,
             "Read Gaussian data from bytes. Returns dict with 'data' or "
             "'error'.")
        .def("write", &GaussForgePy::write, nb::arg("ir"), nb::arg("format"),
             nb::arg("strict") = false,
             "Write Gaussian IR to bytes. Returns dict with 'data' or 'error'.")
        .def("convert", &GaussForgePy::convert, nb::arg("data"),
             nb::arg("in_format"), nb::arg("out_format"),
             nb::arg("strict") = false,
             "Convert between formats. Returns dict with 'data' or 'error'.")
        .def("get_supported_formats", &GaussForgePy::getSupportedFormats,
             "Get list of supported format names.")
        .def("get_model_info", &GaussForgePy::getModelInfo, nb::arg("data"),
             nb::arg("format"), nb::arg("file_size") = 0,
             "Get detailed model information. Returns dict with 'data' or "
             "'error'.")
        .def("get_version", &GaussForgePy::getVersion,
             "Get library version string.");

    // Module-level convenience function
    m.def(
        "get_version", []() { return std::string(GAUSS_FORGE_VERSION_STRING); },
        "Get library version string.");
}
