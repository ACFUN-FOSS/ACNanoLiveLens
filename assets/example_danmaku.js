// JavaScript 示例：如何使用弹幕绑定

// 示例 1：添加单个弹幕
danmaku.add("User1", "Hello from JavaScript!");

// 示例 2：添加带时间戳的弹幕
danmaku.add("User2", "This is a danmaku with timestamp", Date.now());

// 示例 3：添加多个弹幕
danmaku.add("User3", "Third danmaku");
danmaku.add("User4", "Fourth danmaku");
danmaku.add("User5", "Fifth danmaku");

// 示例 4：添加随机弹幕
danmaku.addRandom(5);

// 示例 5：从 JSON 添加弹幕
danmaku.addFromJson('[
  {"sender": "JSON_User1", "content": "First JSON danmaku", "timestamp": 1234567890000},
  {"sender": "JSON_User2", "content": "Second JSON danmaku", "timestamp": 1234567891000},
  {"sender": "JSON_User3", "content": "Third JSON danmaku", "timestamp": 1234567892000}
]');

// 示例 6：添加单个 JSON 弹幕
danmaku.addFromJson('{"sender": "Single_JSON_User", "content": "Single JSON danmaku", "timestamp": 1234567893000}');

// 示例 7：使用 log 函数
log("JavaScript bindings are working!");
log("Danmaku system is ready to use.");

// 示例 8：循环添加弹幕
for (let i = 0; i < 10; i++) {
    danmaku.add("Loop_User_" + i, "This is danmaku #" + i);
}

// 示例 9：清空弹幕
// danmaku.clear();

// 示例 10：模拟实时弹幕流
function simulateLiveStream() {
    const messages = [
        "Great stream!",
        "Hello everyone!",
        "Nice content!",
        "Keep it up!",
        "Amazing!",
        "LOL",
        "Thanks for sharing!",
        "Awesome!",
        "Cool!",
        "Nice!"
    ];
    
    const users = [
        "Viewer1", "Viewer2", "Viewer3", "Viewer4", "Viewer5",
        "Viewer6", "Viewer7", "Viewer8", "Viewer9", "Viewer10"
    ];
    
    for (let i = 0; i < 20; i++) {
        const randomUser = users[Math.floor(Math.random() * users.length)];
        const randomMessage = messages[Math.floor(Math.random() * messages.length)];
        danmaku.add(randomUser, randomMessage);
    }
}

// 运行模拟
simulateLiveStream();

log("Example script completed!");
