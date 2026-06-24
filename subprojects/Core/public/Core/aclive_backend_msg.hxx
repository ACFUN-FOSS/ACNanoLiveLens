#ifndef NANOLIVELENS_CORE_ACLIVE_BACKEND_MSG_HXX
#define NANOLIVELENS_CORE_ACLIVE_BACKEND_MSG_HXX

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <rfl.hpp>

struct EmptyData
{
	rfl::ExtraFields<rfl::Generic> extraFields;
};

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

using AnyResp = std::variant<
	QrCodeLoginResp,
	QrCodeScannedResp,
	QrCodeLoginTerminatedResp,
	QrCodeLoginSuccessResp
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
