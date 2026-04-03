// 测试 JavaScript 绑定

log("=== JavaScript 绑定测试 ===");

// 测试 1: 基本弹幕添加
log("测试 1: 添加基本弹幕");
danmaku.add("测试用户1", "这是一条测试弹幕");
danmaku.add("测试用户2", "这是另一条测试弹幕");

// 测试 2: 带时间戳的弹幕
log("测试 2: 添加带时间戳的弹幕");
danmaku.add("测试用户3", "带时间戳的弹幕", Date.now());

// 测试 3: 随机弹幕
log("测试 3: 添加随机弹幕");
danmaku.addRandom(3);

// 测试 4: JSON 弹幕
log("测试 4: 从 JSON 添加弹幕");
danmaku.addFromJson('{"sender": "JSON用户", "content": "来自 JSON 的弹幕", "timestamp": 1234567890000}');

// 测试 5: JSON 数组弹幕
log("测试 5: 从 JSON 数组添加弹幕");
danmaku.addFromJson('[
  {"sender": "数组用户1", "content": "第一条", "timestamp": 1234567890000},
  {"sender": "数组用户2", "content": "第二条", "timestamp": 1234567891000}
]');

log("=== 测试完成 ===");
log("JavaScript 绑定工作正常！");
