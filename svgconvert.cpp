
#include "BitmapIcon.h"

#include "lib/args.hxx"
#include "lib/tinyformat.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"
#define NANOSVG_IMPLEMENTATION
#include "lib/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "lib/nanosvgrast.h"

#include <set>
#include <vector>
#include <iostream>
#include <fstream>

#include <cstdint>

static std::string toUpperCase(const std::string &s) {
    std::string result = s;
    for (auto &c : result) {
        c = toupper(c);
    }
    return result;
}

template<size_t Size>
class BitEncoder {
public:
    BitEncoder(std::vector<uint8_t> &data) : _data(data) {}

    void encode(uint8_t value) {
        value &= Mask;
        _buf |= value << _shift;
        _shift += Size;
        if (_shift >= 8) {
            _data.emplace_back(_buf);
            _buf = 0;
            _shift = 0;
        }
    }

    void commit() {
        if (_shift != 0) {
            _data.emplace_back(_buf);
        }
        _buf = 0;
        _shift = 0;
    }
private:
    static_assert(8 % Size == 0, "Invalid encoding size");
    static const uint8_t Mask = (1 << Size) - 1;
    std::vector<uint8_t> &_data;
    uint8_t _buf = 0;
    size_t _shift = 0;
};

template<size_t Size>
class BitDecoder {
public:
    BitDecoder(const std::vector<uint8_t> &data) : _data(data), _read(_data.data()) {}

    uint8_t decode() {
        uint8_t value = (*_read >> _shift) & Mask;
        _shift += Size;
        if (_shift >= 8) {
            ++_read;
            _shift = 0;
        }
        return value;
    }

    bool finished() const {
        return _read >= (_data.data() + _data.size());
    }

private:
    static_assert(8 % Size == 0, "Invalid decoding size");
    static const uint8_t Mask = (1 << Size) - 1;
    const std::vector<uint8_t> &_data;
    const uint8_t *_read;
    size_t _shift = 0;
};


class Converter {
public:
    Converter(
        const std::string &filename,
        const std::string &name,
        int width,
        int height,
        int bpp
    ) :
        _filename(filename),
        _name(name),
        _width(width),
        _height(height),
        _bpp(bpp)
    {}

    template<size_t N>
    void encodeBitmap(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst) {
        BitEncoder<N> encoder(dst);
        for (const auto &pixel : src) {
            encoder.encode(pixel >> (8 - N));
        }
        encoder.commit();    
    }

    void operator()() {
        NSVGimage *svg = nsvgParseFromFile(_filename.c_str(), "px", std::max(_width, _height));

        // printf("size: %f x %f\n", svg->width, svg->height);
        // // Use...
        // for (NSVGshape *shape = svg->shapes; shape != NULL; shape = shape->next) {
        //     printf("shape\n");
        //     for (NSVGpath *path = shape->paths; path != NULL; path = path->next) {
        //         printf("path\n");
        //     }
        // }

        NSVGrasterizer *rast = nsvgCreateRasterizer();

        std::vector<uint8_t> bitmap(_width * _height * 4);

        float scale = std::min(_width / svg->width, _height / svg->height);

        nsvgRasterize(rast, svg, 0, 0, scale, bitmap.data(), _width, _height, _width * 4);

        nsvgDeleteRasterizer(rast);

        // extract first channel
        for (int i = 0; i < _width * _height; ++i) {
            bitmap[i] = bitmap[i * 4 + 3];
        }
        bitmap.resize(_width * _height);

        switch (_bpp) {
        case 1: encodeBitmap<1>(bitmap, _bitmap); break;
        case 2: encodeBitmap<2>(bitmap, _bitmap); break;
        case 4: encodeBitmap<4>(bitmap, _bitmap); break;
        case 8: encodeBitmap<8>(bitmap, _bitmap); break;
        }
    }

    std::string header() const {
        std::string guard = "__" + toUpperCase(_name) + "_H__";

        std::string result;
        result += tfm::format("#ifndef %s\n", guard);
        result += tfm::format("#define %s\n", guard);
        result += "\n";
        result += "#include \"BitmapIcon.h\"\n";
        result += "\n";

        result += tfm::format("static uint8_t %s_bitmap[] = {\n", _name);
        for (size_t i = 0; i < _bitmap.size(); ++i) {
            if (i % 16 == 0) {
                result += "    ";
            }
            result += tfm::format("0x%02x", _bitmap[i]);
            if (i == _bitmap.size() - 1) {
                break;
            }
            result += ", ";
            if (i % 16 == 15) {
                result += "\n";
            }
        }
        result += tfm::format("\n};\n");
        result += "\n";

        result += tfm::format("static BitmapIcon %s = {\n", _name);
        result += tfm::format("    %d, %d, %d, %s_bitmap\n", _width, _height, _bpp, _name);
        result += "};\n";

        result += "\n";
        result += "#endif // " + guard + "\n";

        return result;
    }

    template<size_t N>
    void decodeBitmap(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst) {
        BitDecoder<N> decoder(src);
        int scale = 255 / ((1 << N) - 1);
        while (!decoder.finished()) {
            dst.emplace_back(decoder.decode() * scale);
        }
    }

    void renderBitmap(const std::string &filename) {
        std::vector<uint8_t> bitmap;

        switch (_bpp) {
        case 1: decodeBitmap<1>(_bitmap, bitmap); break;
        case 2: decodeBitmap<2>(_bitmap, bitmap); break;
        case 4: decodeBitmap<4>(_bitmap, bitmap); break;
        case 8: decodeBitmap<8>(_bitmap, bitmap); break;
        }

        std::cout << _bitmap.size() << std::endl;
        std::cout << bitmap.size() << std::endl;

        stbi_write_bmp(filename.c_str(), _width, _height, 1, bitmap.data());
    }

private:
    std::string _filename;
    std::string _name;
    int _width;
    int _height;
    int _bpp;

    std::vector<uint8_t> _bitmap;
};


int main(int argc, char **argv) {
    args::ArgumentParser parser("SVG to bitmap converter", "");
    args::ValueFlag<int> width(parser, "width", "Image width", { 'w', "width" }, 32);
    args::ValueFlag<int> height(parser, "height", "Image height", { 'h', "height" }, 32);
    args::ValueFlag<int> bpp(parser, "bpp", "Bits per pixel (1, 2, 4, 8)", { 'b', "bpp" }, 8);
    args::ValueFlag<std::string> format(parser, "format", "Output format (h, png, bmp)", { 'f', "format" }, "h");
    args::HelpFlag help(parser, "help", "Display help", { 'h', "help" });
    args::Positional<std::string> svg(parser, "svg", "SVG file");
    args::Positional<std::string> name(parser, "name", "Output name");

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    if (!svg) {
        std::cout << "Provide a SVG file!" << std::endl;
        return 1;
    }
    if (!name) {
        std::cout << "Provide a output name" << std::endl;
        return 1;
    }
    if (std::set<int>({ 1, 2, 4, 8 }).count(args::get(bpp)) == 0) {
        std::cout << "Invalid bits per pixel!. Use  1, 2, 4 or 8." << std::endl;
        return 1;
    }
    if (std::set<std::string>({ "h", "png", "bmp" }).count(args::get(format)) == 0) {
        std::cout << "Invalid output format '" << args::get(format) << "'. Use 'h', 'png' or 'bmp'." << std::endl;
        return 1;
    }

    Converter converter(args::get(svg), args::get(name), args::get(width), args::get(height), args::get(bpp));
    converter();

    auto header = converter.header();
    std::cout << header;

    std::ofstream ofs(args::get(name) + ".h");
    ofs << header;
    ofs.close();

    converter.renderBitmap(args::get(name) + ".bmp");

    return 0;
}