#ifndef NANOLIVELENS_CORE_EXCP_TO_SHOW_TO_USER_HXX
#define NANOLIVELENS_CORE_EXCP_TO_SHOW_TO_USER_HXX


class ExcpToShowToUser : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
	~ExcpToShowToUser() override;

	virtual std::string_view humanFriendlyMsg() const noexcept;
};

#endif