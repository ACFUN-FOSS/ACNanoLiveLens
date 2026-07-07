#include "Core/excp_to_show_to_user.hxx"

ExcpToShowToUser::~ExcpToShowToUser() = default;

std::string_view ExcpToShowToUser::humanFriendlyMsg() const noexcept {
	return what();
}