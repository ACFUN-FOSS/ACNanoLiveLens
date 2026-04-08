#ifndef NANOLIVELENS_MSG_BOX_HXX
#define NANOLIVELENS_MSG_BOX_HXX

class MsgBox
{
public:
	enum class Type
	{
		EERR,
		EWARN,
		EINFO,
	};

	MsgBox(Type type, std::string_view text);
	~MsgBox();

	MsgBox(const MsgBox &) = delete;
	MsgBox(MsgBox &&) = delete;
	MsgBox &operator=(const MsgBox &) = delete;
	MsgBox &operator=(MsgBox &&) = delete;

	static void popupOKMsgBox(Type type, std::string_view text);

private:
	void showModal();

	class Impl;
	stdx::pimpl::unique_ptr<Impl> pImpl;
};

#endif
