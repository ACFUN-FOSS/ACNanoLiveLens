#ifndef NANOLIVELENS_CORE_ASSETS_HXX
#define NANOLIVELENS_CORE_ASSETS_HXX

stdf::path getAssetsDir();
std::string_view readTextAssetCached(const stdf::path &path);

#endif //NANOLIVELENS_CORE_ASSETS_HXX
