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

}
