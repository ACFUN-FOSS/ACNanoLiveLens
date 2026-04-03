// 高级 JavaScript 示例：弹幕管理系统

// 弹幕管理器类
class DanmakuManager {
    constructor() {
        this.danmakuQueue = [];
        this.isProcessing = false;
        this.processInterval = 100; // 处理间隔（毫秒）
    }

    // 添加弹幕到队列
    addDanmaku(sender, content, timestamp = Date.now()) {
        this.danmakuQueue.push({
            sender: sender,
            content: content,
            timestamp: timestamp
        });
    }

    // 批量添加弹幕
    addBatch(danmakuList) {
        danmakuList.forEach(d => {
            this.addDanmaku(d.sender, d.content, d.timestamp);
        });
    }

    // 处理队列中的弹幕
    processQueue() {
        if (this.isProcessing || this.danmakuQueue.length === 0) {
            return;
        }

        this.isProcessing = true;
        
        const processNext = () => {
            if (this.danmakuQueue.length > 0) {
                const danmaku = this.danmakuQueue.shift();
                danmaku.add(danmaku.sender, danmaku.content, danmaku.timestamp);
                setTimeout(processNext, this.processInterval);
            } else {
                this.isProcessing = false;
            }
        };

        processNext();
    }

    // 清空队列
    clearQueue() {
        this.danmakuQueue = [];
    }

    // 获取队列长度
    getQueueLength() {
        return this.danmakuQueue.length;
    }
}

// 创建弹幕管理器实例
const manager = new DanmakuManager();

// 示例 1：模拟直播弹幕流
function simulateLiveStream(duration = 10000) {
    const messages = [
        "666666", "主播好厉害！", "学到了", "感谢分享", "太强了",
        "支持一下", "关注了", "点赞", "收藏了", "转发",
        "这个功能不错", "期待更多内容", "加油", "继续努力", "很棒"
    ];
    
    const users = [
        "粉丝A", "粉丝B", "粉丝C", "粉丝D", "粉丝E",
        "路人甲", "路人乙", "路人丙", "路人丁", "路人戊"
    ];
    
    const startTime = Date.now();
    let count = 0;
    
    const interval = setInterval(() => {
        if (Date.now() - startTime >= duration) {
            clearInterval(interval);
            log("直播弹幕模拟结束，共发送 " + count + " 条弹幕");
            return;
        }
        
        const randomUser = users[Math.floor(Math.random() * users.length)];
        const randomMessage = messages[Math.floor(Math.random() * messages.length)];
        
        manager.addDanmaku(randomUser, randomMessage);
        manager.processQueue();
        count++;
    }, 200);
}

// 示例 2：从配置文件加载弹幕
function loadDanmakuFromConfig(configJson) {
    try {
        const config = JSON.parse(configJson);
        if (config.danmakuList && Array.isArray(config.danmakuList)) {
            manager.addBatch(config.danmakuList);
            manager.processQueue();
            log("从配置加载了 " + config.danmakuList.length + " 条弹幕");
        }
    } catch (e) {
        log("配置解析错误: " + e.message);
    }
}

// 示例 3：弹幕过滤器
class DanmakuFilter {
    constructor() {
        this.bannedWords = ["广告", "垃圾", "spam"];
        this.maxContentLength = 100;
    }

    filter(danmaku) {
        // 检查违禁词
        for (const word of this.bannedWords) {
            if (danmaku.content.includes(word)) {
                return false;
            }
        }
        
        // 检查长度
        if (danmaku.content.length > this.maxContentLength) {
            return false;
        }
        
        return true;
    }

    addBannedWord(word) {
        this.bannedWords.push(word);
    }
}

// 创建过滤器实例
const filter = new DanmakuFilter();

// 示例 4：带过滤的弹幕添加
function addFilteredDanmaku(sender, content, timestamp = Date.now()) {
    const danmaku = { sender: sender, content: content, timestamp: timestamp };
    
    if (filter.filter(danmaku)) {
        manager.addDanmaku(sender, content, timestamp);
        manager.processQueue();
        return true;
    } else {
        log("弹幕被过滤: " + content);
        return false;
    }
}

// 示例 5：弹幕统计
class DanmakuStats {
    constructor() {
        this.totalCount = 0;
        this.userCounts = {};
        this.filteredCount = 0;
    }

    record(danmaku, wasFiltered = false) {
        if (wasFiltered) {
            this.filteredCount++;
        } else {
            this.totalCount++;
            this.userCounts[danmaku.sender] = (this.userCounts[danmaku.sender] || 0) + 1;
        }
    }

    getStats() {
        return {
            total: this.totalCount,
            filtered: this.filteredCount,
            uniqueUsers: Object.keys(this.userCounts).length,
            topUsers: this.getTopUsers(5)
        };
    }

    getTopUsers(n) {
        return Object.entries(this.userCounts)
            .sort((a, b) => b[1] - a[1])
            .slice(0, n)
            .map(([user, count]) => ({ user, count }));
    }

    printStats() {
        const stats = this.getStats();
        log("=== 弹幕统计 ===");
        log("总数: " + stats.total);
        log("过滤: " + stats.filtered);
        log("独立用户: " + stats.uniqueUsers);
        log("活跃用户 TOP " + stats.topUsers.length + ":");
        stats.topUsers.forEach((u, i) => {
            log("  " + (i + 1) + ". " + u.user + ": " + u.count + " 条");
        });
    }
}

// 创建统计实例
const stats = new DanmakuStats();

// 示例 6：运行演示
function runDemo() {
    log("=== 开始弹幕系统演示 ===");
    
    // 1. 添加一些测试弹幕
    log("1. 添加测试弹幕...");
    for (let i = 0; i < 10; i++) {
        addFilteredDanmaku("测试用户" + i, "测试弹幕 #" + i);
        stats.record({ sender: "测试用户" + i, content: "测试弹幕 #" + i });
    }
    
    // 2. 添加一些会被过滤的弹幕
    log("2. 添加过滤测试弹幕...");
    addFilteredDanmaku("广告用户", "这是一条广告");
    stats.record({ sender: "广告用户", content: "这是一条广告" }, true);
    
    // 3. 模拟直播
    log("3. 模拟直播弹幕流...");
    simulateLiveStream(5000);
    
    // 4. 显示统计
    setTimeout(() => {
        stats.printStats();
        log("=== 演示结束 ===");
    }, 6000);
}

// 运行演示
runDemo();

// 导出函数供外部调用
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        DanmakuManager,
        DanmakuFilter,
        DanmakuStats,
        manager,
        filter,
        stats,
        simulateLiveStream,
        addFilteredDanmaku,
        runDemo
    };
}
