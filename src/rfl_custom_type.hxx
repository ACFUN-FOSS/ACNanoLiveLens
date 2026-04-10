namespace rfl {
template <>
struct Reflector<std::chrono::system_clock::time_point> {
  // 定义反射类型为 long long，这样 reflect-cpp 会将其解析为 JSON 数字
  using ReflType = long long;

  // 将 ReflType (long long) 转换为目标类型 (time_point)
  static std::chrono::system_clock::time_point to(const ReflType& v) noexcept {
    // 假设 JSON 中是毫秒时间戳
    return std::chrono::system_clock::time_point{ std::chrono::milliseconds{ v } };
  }

  // 将目标类型 (time_point) 转换为 ReflType (long long)
  static ReflType from(const std::chrono::system_clock::time_point& v) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            v.time_since_epoch()
		).count();
  }
};
}
