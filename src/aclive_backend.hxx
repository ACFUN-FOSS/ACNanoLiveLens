#ifndef NANOLIVELENS_ACLIVE_BACKEND_HXX
#define NANOLIVELENS_ACLIVE_BACKEND_HXX

namespace ACLive {

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
	AcliveBackendRespMeta meta;
	T data;

	static int type;
};

template <int TYPE, typename T>
int AcliveBackendResp<TYPE, T>::type = TYPE;

struct HeartbeatReq
{
};

struct LoginReq
{
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
	std::string clientId;
};

struct SetClientIdRespBody
{
};

using SetClientIdResp = AcliveBackendResp<4, EmptyData>;

struct SetTokenReq
{
	std::string token;
};

struct SetTokenRespBody
{
};

using SetTokenResp = AcliveBackendResp<5, EmptyData>;

struct QrCodeLoginReq
{
};

struct QrCodeLoginRespBody
{
	std::string imageData;
	std::chrono::steady_clock::time_point expireTime;
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
	int64_t liveId;
};

struct GetDanmakuRespBody
{
};

using GetDanmakuResp = AcliveBackendResp<100, EmptyData>;

struct StopGetDanmakuReq
{
	int64_t liveId;
};

struct StopGetDanmakuRespBody
{
};

using StopGetDanmakuResp = AcliveBackendResp<101, EmptyData>;

struct LiveRoomAudienceListReq
{
	int64_t liveId;
};

struct LiveRoomAudienceListRespBody
{
	std::vector<int64_t> uids;
};

using LiveRoomAudienceListResp = AcliveBackendResp<102, LiveRoomAudienceListRespBody>;

struct GiftContributionRankReq
{
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
	int64_t liveId;
};

struct LiveReplayRespBody
{
	std::string replayUrl;
};

using LiveReplayResp = AcliveBackendResp<106, LiveReplayRespBody>;

struct AllGiftListReq
{
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
};

struct AccountWalletRespBody
{
	int64_t balance;
	std::string currency;
};

using AccountWalletResp = AcliveBackendResp<108, AccountWalletRespBody>;

struct UserLiveInfoReq
{
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
	std::string imageData;
};

struct UploadImageRespBody
{
	std::string imageUrl;
};

using UploadImageResp = AcliveBackendResp<111, UploadImageRespBody>;

struct LiveStatisticsReq
{
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
};

struct LiveScheduleListRespBody
{
	struct ScheduleInfo
	{
		int64_t scheduleId;
		std::string title;
		std::chrono::system_clock::time_point startTime;
	};
	std::vector<ScheduleInfo> schedules;
};

using LiveScheduleListResp = AcliveBackendResp<113, LiveScheduleListRespBody>;

struct LiveRoomGiftListReq
{
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
	int64_t liveId;
};

struct LiveClipInfoRespBody
{
	std::vector<std::string> clipUrls;
};

using LiveClipInfoResp = AcliveBackendResp<116, LiveClipInfoRespBody>;

struct ModeratorListReq
{
};

struct ModeratorListRespBody
{
	std::vector<int64_t> uids;
};

using ModeratorListResp = AcliveBackendResp<117, ModeratorListRespBody>;

struct AddModeratorReq
{
	int64_t uid;
};

struct AddModeratorRespBody
{
};

using AddModeratorResp = AcliveBackendResp<118, EmptyData>;

struct RemoveModeratorReq
{
	int64_t uid;
};

struct RemoveModeratorRespBody
{
};

using RemoveModeratorResp = AcliveBackendResp<119, EmptyData>;

struct AnchorKickRecordReq
{
	int64_t liveId;
};

struct AnchorKickRecordRespBody
{
	struct KickRecord
	{
		int64_t uid;
		std::string nickname;
		std::string reason;
		std::chrono::system_clock::time_point time;
	};
	std::vector<KickRecord> records;
};

using AnchorKickRecordResp = AcliveBackendResp<120, AnchorKickRecordRespBody>;

struct ModeratorKickReq
{
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
	int64_t uid;
};

struct UserWearingGuardBadgeRespBody
{
	std::optional<int64_t> badgeId;
};

using UserWearingGuardBadgeResp = AcliveBackendResp<126, UserWearingGuardBadgeRespBody>;

struct WearGuardBadgeReq
{
	int64_t badgeId;
};

struct WearGuardBadgeRespBody
{
};

using WearGuardBadgeResp = AcliveBackendResp<127, EmptyData>;

struct UnwearGuardBadgeReq
{
};

struct UnwearGuardBadgeRespBody
{
};

using UnwearGuardBadgeResp = AcliveBackendResp<128, EmptyData>;

struct CheckLivePermissionReq
{
};

struct CheckLivePermissionRespBody
{
	bool hasPermission;
};

using CheckLivePermissionResp = AcliveBackendResp<129, CheckLivePermissionRespBody>;

struct LiveCategoryListReq
{
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
};

struct StreamSettingsRespBody
{
	std::string serverUrl;
	std::string streamKey;
};

using StreamSettingsResp = AcliveBackendResp<131, StreamSettingsRespBody>;

struct LiveStatusReq
{
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
};

struct TranscodingInfoRespBody
{
	std::optional<std::string> transcodingUrl;
};

using TranscodingInfoResp = AcliveBackendResp<133, TranscodingInfoRespBody>;

struct StartLiveReq
{
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
};

struct StopLiveRespBody
{
};

using StopLiveResp = AcliveBackendResp<135, EmptyData>;

struct UpdateLiveInfoReq
{
	std::string title;
	std::string coverUrl;
};

struct UpdateLiveInfoRespBody
{
};

using UpdateLiveInfoResp = AcliveBackendResp<136, EmptyData>;

struct QueryAllowClipReq
{
};

struct QueryAllowClipRespBody
{
	bool allowClip;
};

using QueryAllowClipResp = AcliveBackendResp<137, QueryAllowClipRespBody>;

struct SetAllowClipReq
{
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

} 

#endif
