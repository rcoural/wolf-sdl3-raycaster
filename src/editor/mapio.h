#pragma once

// Binary .lvl format: [u8 width][u8 height][width*height bytes of tile ids].

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

bool saveMap(const std::filesystem::path& path, const std::vector<uint8_t>& data, uint8_t w, uint8_t h);
bool loadMap(const std::filesystem::path& path, std::vector<uint8_t>& data, uint8_t& w, uint8_t& h);
