#include "iop.hpp"

#include "mka/content_vfs.hpp"

#include <cctype>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ps2::iop {

std::int32_t load_module(const std::string &path, const std::string &args,
                         std::function<void(int, int)> done) {
    std::vector<std::uint8_t> data;
    std::string lower = path;
    for (char &ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (lower.rfind("rom0:", 0) != 0) mka::content::read_file(path, data);
    if (data.empty() && lower.rfind("rom0:", 0) != 0)
        std::fprintf(stderr, "[native-vfs-stop] cannot read native module %s\n", path.c_str());
    return load_module_buffer(data, path, args, std::move(done));
}

} // namespace ps2::iop
