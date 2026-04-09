namespace {
using namespace std::chrono;

void saveQrCodeImage(std::string_view base64Data, std::string_view filename) {
	auto imageData = cppcodec::base64_rfc4648::decode(std::string(base64Data));

	std::ofstream file{ std::string{ filename }, std::ios::binary };
	if (!file) {
		std::println("[错误] 无法创建文件: {}", filename);
		return;
	}
    std::ostream_iterator<unsigned char> output_iterator{ file };

	std::ranges::copy(imageData, output_iterator);
	file.close();

	std::println("[二维码] 已保存到: {} ({} 字节)", filename, imageData.size());
}

std::string generateRequestID() {
	static int counter = 0;
	return "req_" + std::to_string(++counter);
}

std::string formatExpireTime(int64_t expireTimeMs) {
	sys_time<milliseconds> expireTime{ milliseconds{ expireTimeMs } };
	zoned_time localTime{ current_zone(), expireTime };
	return std::format("{:%Y-%m-%d %H:%M:%S}", localTime);
}
}

int main() {
	std::println("=== acfunlive-backend 登录测试 ===");
	std::println("");

	std::string wsUrl = "ws://127.0.0.1:15368/";
	std::println("[信息] 连接地址: {}", wsUrl);
	std::println("[信息] 提示: 请确保 acfunlive-backend 已启动");
	std::println("");

	Ws ws{
        wsUrl,
        "type",
        3s,
        []() -> nlohmann::json {
            return {
                {"type", 1}
            };
        }
    };

	bool loginSuccess = false;
	bool qrExpired = false;

	ws.on("7", [&](const WsData& data) {
		auto json = nlohmann::json::parse(data.payload);

		if (json.contains("result") && json["result"] == 1 && json.contains("data")) {
			auto respData = json["data"];
			if (respData.contains("imageData") && respData.contains("expireTime")) {
				std::string imageData = respData["imageData"];
				int64_t expireTime = respData["expireTime"];

				std::println("");
				std::println("========================================");
				std::println("[登录] 二维码已生成!");
				std::println("[登录] 过期时间: {}", formatExpireTime(expireTime));
				std::println("========================================");
				std::println("");

				saveQrCodeImage(imageData, "qrcode.png");
			}
		} else if (json.contains("error")) {
			std::println("[错误] 获取二维码失败: {}", json["error"].get<std::string>());
		}
	});

	ws.on("8", [&](const WsData&) {
		std::println("");
		std::println("========================================");
		std::println("[登录] 用户已扫描二维码!");
		std::println("[登录] 请在手机上确认登录...");
		std::println("========================================");
		std::println("");
	});

	ws.on("9", [&](const WsData&) {
		std::println("");
		std::println("========================================");
		std::println("[登录] 二维码已过期或用户取消登录");
		std::println("[登录] 请重新请求二维码");
		std::println("========================================");
		std::println("");
		qrExpired = true;
	});

	ws.on("10", [&](const WsData& data) {
		auto json = nlohmann::json::parse(data.payload);

		if (json.contains("result") && json["result"] == 1 && json.contains("data")) {
			auto tokenInfo = json["data"]["tokenInfo"];

			std::println("");
			std::println("========================================");
			std::println("[登录] 登录成功!");
			std::println("========================================");
			std::println("[用户ID] {}", tokenInfo["userID"].get<int64_t>());
			std::println("[安全密钥] {}", tokenInfo["securityKey"].get<std::string>());
			std::println("[服务令牌] {}", tokenInfo["serviceToken"].get<std::string>());
			std::println("[设备ID] {}", tokenInfo["deviceID"].get<std::string>());
			std::println("========================================");
			std::println("");

			loginSuccess = true;
		}
	});

	try {
		std::println("[连接] 正在连接...");
		ws.connect();
		std::println("[连接] 已连接");
	} catch (const std::exception& e) {
		std::println("[错误] 连接失败: {}", e.what());
		return 1;
	}

	nlohmann::json request = { {"type", 7}, {"requestID", generateRequestID()} };
	std::println("[请求] 发送登录二维码请求: {}", request.dump());
	ws.send(request);

	std::println("");
	std::println("[等待] 等待用户扫描二维码...");
	std::println("[提示] 二维码图片已保存为 qrcode.png");
	std::println("[提示] 请用 AcFun App 扫描二维码登录");
	std::println("");

	constexpr auto maxWaitTime = 180s;
	constexpr auto pollInterval = 100ms;
	auto startTime = steady_clock::now();
	auto lastDotTime = startTime;

	while (!loginSuccess && !qrExpired) {
		ws.execCb();
		std::this_thread::sleep_for(pollInterval);

		auto now = steady_clock::now();
		if (now - startTime >= maxWaitTime) {
			break;
		}

		if (now - lastDotTime >= 1s) {
			std::print(".");
			lastDotTime = now;
		}
	}
	std::println("");

	if (loginSuccess) {
		std::println("");
		std::println("=== 测试完成: 登录成功 ===");
	} else if (qrExpired) {
		std::println("");
		std::println("=== 测试完成: 二维码已过期 ===");
	} else {
		std::println("");
		std::println("=== 测试超时 ===");
	}

	ws.disconnect();
	std::println("[连接] 已断开");

	return loginSuccess ? 0 : 1;
}
