#ifndef NANOLIVELENS_CORE_ACLIVE_BACKEND_MSG_HXX
#define NANOLIVELENS_CORE_ACLIVE_BACKEND_MSG_HXX

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <rfl.hpp>

struct EmptyData
{
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct LiveActivityRequestData {
	std::uint64_t liverUID;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct LiveActivityRequestWire {
	int type;
	std::string requestID;
	LiveActivityRequestData data;
};

struct LiveStatusReqWire
{
	int type = 903;
	std::string requestID;
};

struct LiveActivityUserMedal {
	std::uint64_t uperID;
	std::uint64_t userID;
	std::string clubName;
	int level;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct LiveActivityUserInfo {
	std::uint64_t userID;
	std::string nickname;
	std::string avatar;
	LiveActivityUserMedal medal;
	int managerType;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct LiveActivityDanmuInfo {
	std::chrono::system_clock::time_point sendTime;
	LiveActivityUserInfo userInfo;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct DanmakuActivityData {
	LiveActivityDanmuInfo danmuInfo;
	std::string content;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct LikeActivityData {
	std::chrono::system_clock::time_point sendTime;
	LiveActivityUserInfo userInfo;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct GiftDetail {
	std::uint64_t giftID;
	std::string giftName;
	std::string arLiveName;
	int payWalletType;
	std::int64_t price;
	std::string webpPic;
	std::string pngPic;
	std::string smallPngPic;
	std::vector<std::int64_t> allowBatchSendSizeList;
	bool canCombo;
	bool canDraw;
	std::int64_t magicFaceID;
	std::int64_t vupArID;
	std::string description;
	std::int64_t redpackPrice;
	std::string cornerMarkerText;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct GiftActivityData {
	LiveActivityDanmuInfo danmuInfo;
	GiftDetail giftDetail;
	std::int64_t count;
	std::int64_t combo;
	std::int64_t value;
	std::string comboID;
	std::int64_t slotDisplayDuration;
	std::int64_t expireDuration;
	rfl::ExtraFields<rfl::Generic> extraFields;
};


// “Wire” type: align with aclive-backend's JSON request / response structure, for serializing / deserializing.
template <int TYPE, typename T>
struct LiveActivityWire {
	std::uint64_t liverUID;
	int type = TYPE;
	T data;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

using DanmakuActivityWire = LiveActivityWire<1000, DanmakuActivityData>;
using LikeActivityWire = LiveActivityWire<1001, LikeActivityData>;
using GiftActivityWire = LiveActivityWire<1005, GiftActivityData>;

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

	static constexpr int type = TYPE;
};

using StartLiveActivityResp = AcliveBackendResp<100, LiveActivityRequestData>;
using StopLiveActivityResp = AcliveBackendResp<101, LiveActivityRequestData>;

template <int TYPE, typename T>
struct LiveActivityEvent
{
	std::uint64_t liverUID;
	T data;

	static constexpr int type = TYPE;
};

using DanmakuActivity = LiveActivityEvent<1000, DanmakuActivityData>;
using LikeActivity = LiveActivityEvent<1001, LikeActivityData>;
using GiftActivity = LiveActivityEvent<1005, GiftActivityData>;
using LiveActivity = std::variant<DanmakuActivity, LikeActivity, GiftActivity>;

struct QrCodeLoginRespBody
{
	std::string imageData;
	std::chrono::system_clock::time_point expireTime;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

using QrCodeLoginResp = AcliveBackendResp<7, QrCodeLoginRespBody>;
using QrCodeScannedResp = AcliveBackendResp<8, EmptyData>;
using QrCodeLoginTerminatedResp = AcliveBackendResp<9, EmptyData>;

struct LoginTokenInfo
{
	std::uint64_t userID;
	std::string securityKey;
	std::string serviceToken;
	std::string deviceID;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

struct QrCodeLoginSuccessRespBody
{
	LoginTokenInfo tokenInfo;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

using QrCodeLoginSuccessResp = AcliveBackendResp<10, QrCodeLoginSuccessRespBody>;

struct LiveStatusRespBody
{
	std::optional<std::string> liveID;
	std::optional<std::string> streamName;
	std::optional<std::string> title;
	std::optional<std::chrono::system_clock::time_point> liveStartTime;
	std::optional<bool> portrait;
	std::optional<bool> panoramic;
	std::optional<std::string> liveCover;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

using LiveStatusResp = AcliveBackendResp<903, LiveStatusRespBody>;

struct LiveActivityEndedWire
{
	std::uint64_t liverUID;
	int type = 2000;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

// “Wire” type: align with aclive-backend's JSON request / response structure, for serializing / deserializing.
struct HeartbeatReqWire
{
	int type = 1;
};

struct QrCodeLoginReqWire
{
	int type = QrCodeLoginResp::type;
	std::string requestID;
};

template <typename T>
struct AcliveBackendRespWire
{
	int type;
	std::string requestID;
	int result;
	std::optional<std::string> error;
	std::optional<T> data;
	rfl::ExtraFields<rfl::Generic> extraFields;
};

using QrCodeLoginRespWire = AcliveBackendRespWire<QrCodeLoginRespBody>;
using QrCodeScannedRespWire = AcliveBackendRespWire<EmptyData>;
using QrCodeLoginTerminatedRespWire = AcliveBackendRespWire<EmptyData>;
using QrCodeLoginSuccessRespWire = AcliveBackendRespWire<QrCodeLoginSuccessRespBody>;
using StartLiveActivityRespWire = AcliveBackendRespWire<LiveActivityRequestData>;
using StopLiveActivityRespWire = AcliveBackendRespWire<LiveActivityRequestData>;
using LiveStatusRespWire = AcliveBackendRespWire<LiveStatusRespBody>;

using AnyResp = std::variant<
	StartLiveActivityResp,
	StopLiveActivityResp,
	QrCodeLoginResp,
	QrCodeScannedResp,
	QrCodeLoginTerminatedResp,
	QrCodeLoginSuccessResp,
	LiveStatusResp
>;

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

inline AcliveBackendRespMeta getMetaFromAnyResp(const AnyResp &anyResp) {
	return std::visit([](auto &&resp) { return resp.meta; }, anyResp);
}

inline int getTypeOfAnyResp(const AnyResp &anyResp) {
	return std::visit([](auto &&resp) {
		return std::remove_cvref_t<decltype(resp)>::type;
	}, anyResp);
}

#endif
