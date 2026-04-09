int main() {
	std::println("=== Ws 组件测试 ===");

	httplib::Server svr;
	svr.WebSocket("/echo", [](const httplib::Request&, httplib::ws::WebSocket& ws) {
		std::string msg;
		while (true) {
			auto result = ws.read(msg);
			if (result == httplib::ws::ReadResult::Text) {
				std::println("[服务器] 收到: {}", msg);
				ws.send(msg);
			} else {
				break;
			}
		}
	});

	std::jthread serverThread([&svr]() {
		std::println("[服务器] 启动在端口 18080...");
		svr.listen("127.0.0.1", 18080);
	});

	svr.wait_until_ready();
	std::println("[服务器] 已就绪");

	// === 测试 1: on/once 回调 ===
	std::println("\n--- 测试 1: on/once 回调 ---");

	Ws ws{ "ws://127.0.0.1:18080/echo" };

	int onCount = 0;
	int onceCount = 0;

	ws.on("test", [&onCount](const WsData& data) {
		++onCount;
		std::println("[客户端] on 触发 #{}: {}", onCount, data.payload);
	});

	ws.once("test", [&onceCount](const WsData& data) {
		++onceCount;
		std::println("[客户端] once 触发 #{}: {}", onceCount, data.payload);
	});

	ws.connect();
	std::println("[客户端] 已连接");

	for (int i = 1; i <= 3; ++i) {
		ws.send({ {"event", "test"}, {"message", "hello"} });
		std::println("[客户端] 已发送消息 #{}", i);

		for (int j = 0; j < 20; ++j) {
			ws.execCb();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	std::println("[客户端] 等待处理剩余消息...");
	for (int i = 0; i < 20; ++i) {
		ws.execCb();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	std::println("=== 测试 1 结果 ===");
	std::println("on 触发次数: {} (期望: 3)", onCount);
	std::println("once 触发次数: {} (期望: 1)", onceCount);

	bool test1Pass = (onCount == 3 && onceCount == 1);
	std::println("测试 1: {}", test1Pass ? "通过" : "失败");

	// === 测试 2: 移动构造 ===
	std::println("\n--- 测试 2: 移动构造 ---");

	int moveCtorCount = 0;
	ws.on("move_test", [&moveCtorCount](const WsData& data) {
		++moveCtorCount;
		std::println("[客户端] 收到: {}", data.payload);
	});

	std::println("[移动构造] 移动前发送消息...");
	ws.send({ {"event", "move_test"}, {"message", "before_move"} });

	for (int i = 0; i < 10; ++i) {
		ws.execCb();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	std::println("[移动构造] 执行移动构造...");
	Ws wsMoved{ std::move(ws) };
	std::println("[移动构造] 移动完成");

	std::println("[移动构造] 移动后发送消息...");
	wsMoved.send({ {"event", "move_test"}, {"message", "after_move"} });

	for (int i = 0; i < 10; ++i) {
		wsMoved.execCb();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	std::println("=== 测试 2 结果 ===");
	std::println("移动构造后收到消息数: {} (期望: 2)", moveCtorCount);

	bool test2Pass = (moveCtorCount == 2);
	std::println("测试 2: {}", test2Pass ? "通过" : "失败");

	// === 测试 3: 移动赋值 ===
	std::println("\n--- 测试 3: 移动赋值 ---");

	int moveAssignCount = 0;
	wsMoved.on("assign_test", [&moveAssignCount](const WsData& data) {
		++moveAssignCount;
		std::println("[客户端] 收到: {}", data.payload);
	});

	std::println("[移动赋值] 赋值前发送消息...");
	wsMoved.send({ {"event", "assign_test"}, {"message", "before_assign"} });

	for (int i = 0; i < 10; ++i) {
		wsMoved.execCb();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	std::println("[移动赋值] 执行移动赋值...");
	Ws wsAssigned{ "ws://127.0.0.1:18080/echo" };
	wsAssigned = std::move(wsMoved);
	std::println("[移动赋值] 赋值完成");

	std::println("[移动赋值] 赋值后发送消息...");
	wsAssigned.send({ {"event", "assign_test"}, {"message", "after_assign"} });

	for (int i = 0; i < 10; ++i) {
		wsAssigned.execCb();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	std::println("=== 测试 3 结果 ===");
	std::println("移动赋值后收到消息数: {} (期望: 2)", moveAssignCount);

	bool test3Pass = (moveAssignCount == 2);
	std::println("测试 3: {}", test3Pass ? "通过" : "失败");

	// === 清理 ===
	wsAssigned.disconnect();
	std::println("\n[客户端] 已断开");

	svr.stop();

	// === 总结 ===
	std::println("\n=== 测试总结 ===");
	std::println("测试 1 (on/once): {}", test1Pass ? "通过" : "失败");
	std::println("测试 2 (移动构造): {}", test2Pass ? "通过" : "失败");
	std::println("测试 3 (移动赋值): {}", test3Pass ? "通过" : "失败");

	bool allPass = test1Pass && test2Pass && test3Pass;
	std::println("\n总体结果: {}", allPass ? "全部通过!" : "存在失败!");
	return allPass ? 0 : 1;
}
