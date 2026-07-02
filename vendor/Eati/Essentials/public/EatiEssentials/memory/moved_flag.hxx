/*
 * Copyright (c) 2025 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_MOVED_FLAG_HXX
#define ESS_MOVED_FLAG_HXX

namespace Essentials::Memory
{

class MovedFlag {
public:
	MovedFlag() noexcept;
	MovedFlag(const MovedFlag &) noexcept;
	MovedFlag(MovedFlag &&) noexcept;
	MovedFlag &operator=(const MovedFlag &) noexcept;
	MovedFlag &operator=(MovedFlag &&) noexcept;
	operator bool() noexcept;
	~MovedFlag() = default;
	[[nodiscard]] bool moved() const noexcept;
	void reset() noexcept;

private:
	bool moved_;
};

}

#endif // !ESS_MOVED_FLAG_HXX
