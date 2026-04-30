#ifndef NANOLIVELENS_CORE_ACLIVE_BACKEND_MSG_HXX
#define NANOLIVELENS_CORE_ACLIVE_BACKEND_MSG_HXX

struct AcLiveBackendMsgMeta
{
	unsigned short type;
	std::string requestID;
};

// 扫描二维码登陆 返回登陆二维码
struct QrCodeLoginResp
{
	std::chrono::steady_clock::duration expireTime;
	std::string imagedata;
};

// 用户扫描了登陆二维码的响应
struct QrCFodeScannedResp
{ };

// 登陆二维码过期或者用户取消登陆的响应
struct QrCodeLoginExpiredResp
{ };


// 用户成功登陆的响应
struct QrCodeLoginSuccessResp
{
	struct TokenInfo
	{
		std::string userID;
		std::string securityKey;
		std::string serviceToken;
		std::string deviceID;
	} tokenInfo;
};



#endif // !Guard
