#ifndef NANOLIVELENS_ASSETS_HXX
#define NANOLIVELENS_ASSETS_HXX

stdf::path getAssetsDir();
std::string_view readTextAssetCached(const stdf::path &path);

#endif //NANOLIVELENS_ASSETS_HXX
