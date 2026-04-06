#ifndef NANOLIVELENS_ACLIVE_BACKEND_HXX
#define NANOLIVELENS_ACLIVE_BACKEND_HXX

namespace ACLive
{

struct EmptyData { };

struct AcliveBackendRespMeta
{
	std::string requestID;
	int result;
	std::optional<std::string> error;
};

template <int TYPE, typename T>
struct AcliveBackendResp
{
	using data_type = T;
	AcliveBackendRespMeta meta;
	T data;

	static constexpr int type = TYPE;
};

struct HeartbeatReq
{
	static constexpr int type = 1;
};

struct LoginReq
{
	static constexpr int type = 2;
	std::string account;
	std::string password;
};

struct LoginRespBody
{
	std::string token;
};

using LoginResp = AcliveBackendResp<2, LoginRespBody>;

struct SetClientIdReq
{
	static constexpr int type = 4;
	std::string clientId;
};

struct SetClientIdRespBody
{
};

using SetClientIdResp = AcliveBackendResp<4, EmptyData>;

struct SetTokenReq
{
	static constexpr int type = 5;
	std::string token;
};

struct SetTokenRespBody
{
};

using SetTokenResp = AcliveBackendResp<5, EmptyData>;

struct QrCodeLoginReq
{
	static constexpr int type = 7;
};

struct QrCodeLoginRespBody
{
	std::string imageData;
	int64_t expireTime;
};

using QrCodeLoginResp = AcliveBackendResp<7, QrCodeLoginRespBody>;

struct QrCodeScannedRespBody
{
};

using QrCodeScannedResp = AcliveBackendResp<8, EmptyData>;

struct QrCodeLoginTerminatedRespBody
{
};

using QrCodeLoginTerminatedResp = AcliveBackendResp<9, EmptyData>;

struct GetDanmakuReq
{
	static constexpr int type = 100;
	int64_t liveId;
};

struct GetDanmakuRespBody
{
};

using GetDanmakuResp = AcliveBackendResp<100, EmptyData>;

struct StopGetDanmakuReq
{
	static constexpr int type = 101;
	int64_t liveId;
};

struct StopGetDanmakuRespBody
{
};

using StopGetDanmakuResp = AcliveBackendResp<101, EmptyData>;

struct LiveRoomAudienceListReq
{
	static constexpr int type = 102;
	int64_t liveId;
};

struct LiveRoomAudienceListRespBody
{
	std::vector<int64_t> uids;
};

using LiveRoomAudienceListResp = AcliveBackendResp<102, LiveRoomAudienceListRespBody>;

struct GiftContributionRankReq
{
	static constexpr int type = 103;
	int64_t liveId;
};

struct GiftContributionRankRespBody
{
	struct RankItem
	{
		int64_t uid;
		std::string nickname;
		int64_t score;
	};
	std::vector<RankItem> ranks;
};

using GiftContributionRankResp = AcliveBackendResp<103, GiftContributionRankRespBody>;

struct LiveSummaryReq
{
	static constexpr int type = 104;
	int64_t liveId;
};

struct LiveSummaryRespBody
{
	int64_t watchCount;
	int64_t likeCount;
	int64_t bananaCount;
};

using LiveSummaryResp = AcliveBackendResp<104, LiveSummaryRespBody>;

struct GrabRedPacketResultReq
{
	static constexpr int type = 105;
	int64_t liveId;
	int64_t packetId;
};

struct GrabRedPacketResultRespBody
{
	int64_t amount;
	std::string currency;
};

using GrabRedPacketResultResp = AcliveBackendResp<105, GrabRedPacketResultRespBody>;

struct LiveReplayReq
{
	static constexpr int type = 106;
	int64_t liveId;
};

struct LiveReplayRespBody
{
	std::string replayUrl;
};

using LiveReplayResp = AcliveBackendResp<106, LiveReplayRespBody>;

struct AllGiftListReq
{
	static constexpr int type = 107;
};

struct AllGiftListRespBody
{
	struct GiftInfo
	{
		int64_t giftId;
		std::string name;
		std::string iconUrl;
		int64_t price;
	};
	std::vector<GiftInfo> gifts;
};

using AllGiftListResp = AcliveBackendResp<107, AllGiftListRespBody>;

struct AccountWalletReq
{
	static constexpr int type = 108;
};

struct AccountWalletRespBody
{
	int64_t balance;
	std::string currency;
};

using AccountWalletResp = AcliveBackendResp<108, AccountWalletRespBody>;

struct UserLiveInfoReq
{
	static constexpr int type = 109;
	int64_t uid;
};

struct UserLiveInfoRespBody
{
	std::string title;
	std::string coverUrl;
	int64_t liveId;
	bool isLiving;
};

using UserLiveInfoResp = AcliveBackendResp<109, UserLiveInfoRespBody>;

struct LiveRoomListReq
{
	static constexpr int type = 110;
};

struct LiveRoomListRespBody
{
	struct LiveRoomInfo
	{
		int64_t liveId;
		std::string title;
		std::string coverUrl;
		int64_t uid;
		std::string nickname;
	};
	std::vector<LiveRoomInfo> rooms;
};

using LiveRoomListResp = AcliveBackendResp<110, LiveRoomListRespBody>;

struct UploadImageReq
{
	static constexpr int type = 111;
	std::string imageData;
};

struct UploadImageRespBody
{
	std::string imageUrl;
};

using UploadImageResp = AcliveBackendResp<111, UploadImageRespBody>;

struct LiveStatisticsReq
{
	static constexpr int type = 112;
	int64_t liveId;
};

struct LiveStatisticsRespBody
{
	int64_t viewCount;
	int64_t likeCount;
	int64_t shareCount;
};

using LiveStatisticsResp = AcliveBackendResp<112, LiveStatisticsRespBody>;

struct LiveScheduleListReq
{
	static constexpr int type = 113;
};

struct LiveScheduleListRespBody
{
	struct ScheduleInfo
	{
		int64_t scheduleId;
		std::string title;
		int64_t startTime;
	};
	std::vector<ScheduleInfo> schedules;
};

using LiveScheduleListResp = AcliveBackendResp<113, LiveScheduleListRespBody>;

struct LiveRoomGiftListReq
{
	static constexpr int type = 114;
	int64_t liveId;
};

struct LiveRoomGiftListRespBody
{
	struct GiftRecord
	{
		int64_t uid;
		std::string nickname;
		int64_t giftId;
		std::string giftName;
		int count;
	};
	std::vector<GiftRecord> records;
};

using LiveRoomGiftListResp = AcliveBackendResp<114, LiveRoomGiftListRespBody>;

struct UserInfoReq
{
	static constexpr int type = 115;
	int64_t uid;
};

struct UserInfoRespBody
{
	int64_t uid;
	std::string nickname;
	std::string avatarUrl;
	int64_t level;
};

using UserInfoResp = AcliveBackendResp<115, UserInfoRespBody>;

struct LiveClipInfoReq
{
	static constexpr int type = 116;
	int64_t liveId;
};

struct LiveClipInfoRespBody
{
	std::vector<std::string> clipUrls;
};

using LiveClipInfoResp = AcliveBackendResp<116, LiveClipInfoRespBody>;

struct ModeratorListReq
{
	static constexpr int type = 117;
};

struct ModeratorListRespBody
{
	std::vector<int64_t> uids;
};

using ModeratorListResp = AcliveBackendResp<117, ModeratorListRespBody>;

struct AddModeratorReq
{
	static constexpr int type = 118;
	int64_t uid;
};

struct AddModeratorRespBody
{
};

using AddModeratorResp = AcliveBackendResp<118, EmptyData>;

struct RemoveModeratorReq
{
	static constexpr int type = 119;
	int64_t uid;
};

struct RemoveModeratorRespBody
{
};

using RemoveModeratorResp = AcliveBackendResp<119, EmptyData>;

struct AnchorKickRecordReq
{
	static constexpr int type = 120;
	int64_t liveId;
};

struct AnchorKickRecordRespBody
{
	struct KickRecord
	{
		int64_t uid;
		std::string nickname;
		std::string reason;
		int64_t time;
	};
	std::vector<KickRecord> records;
};

using AnchorKickRecordResp = AcliveBackendResp<120, AnchorKickRecordRespBody>;

struct ModeratorKickReq
{
	static constexpr int type = 121;
	int64_t liveId;
	int64_t uid;
	std::string reason;
};

struct ModeratorKickRespBody
{
};

using ModeratorKickResp = AcliveBackendResp<121, EmptyData>;

struct AnchorKickReq
{
	static constexpr int type = 122;
	int64_t liveId;
	int64_t uid;
	std::string reason;
};

struct AnchorKickRespBody
{
};

using AnchorKickResp = AcliveBackendResp<122, EmptyData>;

struct UserGuardBadgeReq
{
	static constexpr int type = 123;
	int64_t uid;
};

struct UserGuardBadgeRespBody
{
	struct GuardBadge
	{
		int64_t badgeId;
		std::string name;
		std::string iconUrl;
		int level;
	};
	std::optional<GuardBadge> badge;
};

using UserGuardBadgeResp = AcliveBackendResp<123, UserGuardBadgeRespBody>;

struct UserGuardBadgeListReq
{
	static constexpr int type = 124;
};

struct UserGuardBadgeListRespBody
{
	struct GuardBadge
	{
		int64_t badgeId;
		std::string name;
		std::string iconUrl;
		int level;
	};
	std::vector<GuardBadge> badges;
};

using UserGuardBadgeListResp = AcliveBackendResp<124, UserGuardBadgeListRespBody>;

struct AnchorGuardRankReq
{
	static constexpr int type = 125;
	int64_t liveId;
};

struct AnchorGuardRankRespBody
{
	struct RankItem
	{
		int64_t uid;
		std::string nickname;
		int level;
	};
	std::vector<RankItem> ranks;
};

using AnchorGuardRankResp = AcliveBackendResp<125, AnchorGuardRankRespBody>;

struct UserWearingGuardBadgeReq
{
	static constexpr int type = 126;
	int64_t uid;
};

struct UserWearingGuardBadgeRespBody
{
	std::optional<int64_t> badgeId;
};

using UserWearingGuardBadgeResp = AcliveBackendResp<126, UserWearingGuardBadgeRespBody>;

struct WearGuardBadgeReq
{
	static constexpr int type = 127;
	int64_t badgeId;
};

struct WearGuardBadgeRespBody
{
};

using WearGuardBadgeResp = AcliveBackendResp<127, EmptyData>;

struct UnwearGuardBadgeReq
{
	static constexpr int type = 128;
};

struct UnwearGuardBadgeRespBody
{
};

using UnwearGuardBadgeResp = AcliveBackendResp<128, EmptyData>;

struct CheckLivePermissionReq
{
	static constexpr int type = 129;
};

struct CheckLivePermissionRespBody
{
	bool hasPermission;
};

using CheckLivePermissionResp = AcliveBackendResp<129, CheckLivePermissionRespBody>;

struct LiveCategoryListReq
{
	static constexpr int type = 130;
};

struct LiveCategoryListRespBody
{
	struct Category
	{
		int64_t categoryId;
		std::string name;
	};
	std::vector<Category> categories;
};

using LiveCategoryListResp = AcliveBackendResp<130, LiveCategoryListRespBody>;

struct StreamSettingsReq
{
	static constexpr int type = 131;
};

struct StreamSettingsRespBody
{
	std::string serverUrl;
	std::string streamKey;
};

using StreamSettingsResp = AcliveBackendResp<131, StreamSettingsRespBody>;

struct LiveStatusReq
{
	static constexpr int type = 132;
	int64_t liveId;
};

struct LiveStatusRespBody
{
	bool isLiving;
	std::string title;
	std::string coverUrl;
};

using LiveStatusResp = AcliveBackendResp<132, LiveStatusRespBody>;

struct TranscodingInfoReq
{
	static constexpr int type = 133;
};

struct TranscodingInfoRespBody
{
	std::optional<std::string> transcodingUrl;
};

using TranscodingInfoResp = AcliveBackendResp<133, TranscodingInfoRespBody>;

struct StartLiveReq
{
	static constexpr int type = 134;
	int64_t categoryId;
	std::string title;
	std::string coverUrl;
};

struct StartLiveRespBody
{
	int64_t liveId;
};

using StartLiveResp = AcliveBackendResp<134, StartLiveRespBody>;

struct StopLiveReq
{
	static constexpr int type = 135;
};

struct StopLiveRespBody
{
};

using StopLiveResp = AcliveBackendResp<135, EmptyData>;

struct UpdateLiveInfoReq
{
	static constexpr int type = 136;
	std::string title;
	std::string coverUrl;
};

struct UpdateLiveInfoRespBody
{
};

using UpdateLiveInfoResp = AcliveBackendResp<136, EmptyData>;

struct QueryAllowClipReq
{
	static constexpr int type = 137;
};

struct QueryAllowClipRespBody
{
	bool allowClip;
};

using QueryAllowClipResp = AcliveBackendResp<137, QueryAllowClipRespBody>;

struct SetAllowClipReq
{
	static constexpr int type = 138;
	bool allowClip;
};

struct SetAllowClipRespBody
{
};

using SetAllowClipResp = AcliveBackendResp<138, EmptyData>;

struct DanmakuData
{
	int64_t liverUID;
	int type;
	std::string jsonData;
};

struct SignalData
{
	int64_t liverUID;
	int type;
	std::string jsonData;
};

template <typename Req>
struct RequestWrapper
{
	int type;
	std::string requestID;
	Req data;
};

template <typename RespData>
struct ResponseWrapper
{
	std::string requestID;
	int result;
	std::optional<std::string> error;
	RespData data;
};

}

#endif
