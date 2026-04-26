#include "mapio.h"

#include <fstream>

bool saveMap(const std::filesystem::path& path, const std::vector<uint8_t>& data, uint8_t w, uint8_t h) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.put(static_cast<char>(w));
    out.put(static_cast<char>(h));
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(w) * h);
    return out.good();
}

bool loadMap(const std::filesystem::path& path, std::vector<uint8_t>& data, uint8_t& w, uint8_t& h) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    w = static_cast<uint8_t>(in.get());
    h = static_cast<uint8_t>(in.get());
    data.resize(static_cast<size_t>(w) * h);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(w) * h);
    return in.gcount() == static_cast<std::streamsize>(w) * h;
}
