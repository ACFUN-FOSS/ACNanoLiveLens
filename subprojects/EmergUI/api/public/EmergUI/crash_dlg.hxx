#ifndef NANOLIVELENS_EMERG_UI_API_CRASH_DLG_HXX
#define NANOLIVELENS_EMERG_UI_API_CRASH_DLG_HXX
#include <string>

enum class CrashReason
{
	UNHANDLED_EXCP,
	ILLEGAL_OP,
	ASSERT_FAIL
};

struct CrashDlgData
{
	CrashReason crashReason;
	std::string msg;
};


#endif
