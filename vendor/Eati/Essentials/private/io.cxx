#include "EatiEssentials/io.hxx"
#include <fstream>
#include <stdexcept>

namespace Essentials::IO
{

std::string readFile(const stdf::path& path) {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.open(path);

    std::string content{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>() };
    file.close();
    return content;
}

std::vector<std::byte> readFileRaw(const stdf::path &path) {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.open(path, std::ios::binary | std::ios::ate);

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> data(size);
    file.read(reinterpret_cast<char *>(data.data()), size);
    file.close();
    return data;
}

}
