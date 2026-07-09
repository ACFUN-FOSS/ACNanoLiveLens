#include "assets.hxx"

using namespace Essentials::IO;

std::string_view readTextAssetCached(const stdf::path &path) {
	static std::mutex mutex;
	static std::unordered_map<stdf::path, std::string> cache;

	std::lock_guard lock{ mutex };
	if (auto it = cache.find(path); it != cache.end()) {
		return std::string_view{ it->second };
	}

	return std::string_view{ cache.emplace(path, readFile(path)).first->second };
}

stdf::path getAssetsDir() {
#ifdef NLLENS_ASSETS_DIR_OVERRIDE_FULL_PATH
	// 使用编译时定义的路径作为 assets 文件夹
    stdf::path assetsDir = NLLENS_ASSETS_DIR_OVERRIDE_FULL_PATH;
    std::println("Using assets dir from NLLENS_ASSETS_DIR_OVERRIDE: {}", assetsDir.string());
    NLS_ASSERT(stdf::is_directory(assetsDir) && "不能从指定的路径找到资产目录。");
    return assetsDir;
#else
    // 从应用程序目录下寻找 assets 文件夹
#error Not implemented.
#endif
}
