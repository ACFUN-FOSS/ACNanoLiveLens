#ifndef NANOLIVELENS_UTILS_HXX
#define NANOLIVELENS_UTILS_HXX


std::string toDbgString(const std::string_view str);
//std::string toDbgString(const bool b);
std::string toDbgString(const Rml::Event &event);
std::string toDbgString(Rml::Vector2i pos);


template <typename T>
concept CanToDebugString = requires(T v) {
	toDbgString(v);
};

template <CanToDebugString... T>
void dbgLog(const T&... args) {
	((std::cout << toDbgString(args)), ...);
	std::cout << std::endl;
}

int randomInt(int min, int max);

#endif
