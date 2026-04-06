# Ws - WebSocket 封装组件

## 概述

Ws 是一个基于 httplib 的 WebSocket 客户端封装组件，旨在提供线程安全的 WebSocket 通信能力，使调用方无需关心线程同步问题，可以像 JavaScript 一样编写 WebSocket 逻辑。

## 设计目标

1. **线程安全**：所有回调在主线程执行，调用方无需处理同步问题
2. **事件驱动**：支持 `on`（持久回调）和 `once`（一次性回调）两种事件订阅模式
3. **移动语义**：支持移动构造和移动赋值，移动后连接保持
4. **RAII**：析构时自动断开连接

## 架构

```
┌─────────────────────────────────────────────────────────┐
│                        Ws 类                            │
│  ┌─────────────────────────────────────────────────┐   │
│  │                   State                          │   │
│  │  ┌─────────────┐  ┌──────────────────────────┐  │   │
│  │  │ WebSocket   │  │     消息队列              │  │   │
│  │  │ Client      │  │  (deque + mutex)         │  │   │
│  │  └─────────────┘  └──────────────────────────┘  │   │
│  │  ┌─────────────────────────────────────────────┐│   │
│  │  │              回调注册表                      ││   │
│  │  │  eventCallbacks (持久) + onceCallbacks      ││   │
│  │  └─────────────────────────────────────────────┘│   │
│  │  ┌─────────────────────────────────────────────┐│   │
│  │  │           接收线程 (jthread)                 ││   │
│  │  └─────────────────────────────────────────────┘│   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## 核心组件

### State 结构体

State 是 Ws 的内部状态，包含所有需要在移动时保持的数据：

```cpp
struct Ws::State {
    std::string url;                                    // WebSocket URL
    std::string eventFieldName;                         // 事件字段名
    std::unique_ptr<httplib::ws::WebSocketClient> ws;   // httplib WebSocket 客户端
    
    std::unordered_map<std::string, Callback> eventCallbacks;  // 持久回调
    std::unordered_map<std::string, Callback> onceCallbacks;   // 一次性回调
    
    std::deque<WsData> messageQueue;      // 消息队列
    std::mutex messageQueueMutex;         // 队列互斥锁
    
    std::jthread receiveThread;           // 接收线程
    std::atomic<bool> isConnected;       // 连接状态
};
```

### 消息数据结构

```cpp
struct WsData {
    std::string event;    // 事件名（从 JSON 的指定字段提取）
    std::string payload;  // 原始消息内容
};
```

## 工作流程

### 连接流程

```
connect()
    │
    ├─→ 创建 WebSocketClient
    ├─→ 建立 WebSocket 连接
    └─→ 启动接收线程
            │
            └─→ receiveLoop()
                    │
                    ├─→ ws->read() 阻塞等待消息
                    ├─→ 解析 JSON 提取 event
                    └─→ enqueueMessage() 放入队列
```

### 回调执行流程

```
主线程调用 execCb()
    │
    ├─→ 加锁交换出消息队列
    ├─→ 遍历消息
    │       │
    │       ├─→ 查找 onceCallbacks，执行并删除
    │       └─→ 查找 eventCallbacks，执行
    └─→ 返回
```

## 线程模型

```
┌──────────────┐     ┌──────────────────┐
│   主线程      │     │    接收线程       │
│              │     │                  │
│  execCb()    │←────│  receiveLoop()   │
│  执行回调     │     │  接收消息         │
│              │     │  入队消息         │
│  send()      │────→│                  │
│  发送消息     │     │                  │
└──────────────┘     └──────────────────┘
        │                    │
        └────────────────────┘
              消息队列 (线程安全)
```

**关键点：**
- 接收线程只负责接收消息并放入队列，不执行回调
- 主线程通过 `execCb()` 从队列取出消息并执行回调
- 这确保了回调在主线程执行，避免线程安全问题

## 移动语义实现

### 问题

接收线程的 lambda 捕获了 `this` 指针，移动后 `this` 指针改变，导致悬垂指针。

### 解决方案

让接收线程捕获 `State*` 而不是 `this`：

```cpp
m_state->receiveThread = std::jthread([state = m_state.get()](std::stop_token) {
    state->receiveLoop();  // State 的成员函数
});
```

`receiveLoop` 和 `enqueueMessage` 作为 `State` 的成员函数，直接操作 `State` 的数据成员。

### 移动操作

```cpp
Ws::Ws(Ws&& other) noexcept
    : m_state{ std::move(other.m_state) } {}

Ws& Ws::operator=(Ws&& other) noexcept {
    if (this != &other) {
        disconnect();
        m_state = std::move(other.m_state);
    }
    return *this;
}
```

移动后：
- 新对象获得完整的 `State`（包括连接和接收线程）
- 旧对象的 `m_state` 变为空
- 连接保持，接收线程继续运行

## 使用示例

### 基本用法（默认 event 字段）

```cpp
#include <Core/ws.hxx>

int main() {
    // 默认使用 "event" 作为事件字段名
    Ws ws{ "ws://example.com/socket" };
    
    // 注册持久回调
    ws.on("message", [](const WsData& data) {
        std::println("收到消息: {}", data.payload);
    });
    
    // 注册一次性回调
    ws.once("connected", [](const WsData& data) {
        std::println("已连接");
    });
    
    // 连接
    ws.connect();
    
    // 发送消息（JSON 格式，包含 "event" 字段）
    ws.send(R"({"event": "chat", "text": "hello"})");
    
    // 主循环中执行回调
    while (running) {
        ws.execCb();  // 在主线程执行所有待处理的回调
        // ... 其他逻辑
    }
    
    // 析构时自动断开
    return 0;
}
```

### 自定义事件字段名

```cpp
// 服务器使用 "msgType" 作为事件字段名
Ws ws{ "ws://example.com/socket", "msgType" };

ws.on("chat", [](const WsData& data) {
    std::println("收到聊天: {}", data.payload);
});

ws.connect();
// 消息格式: {"msgType": "chat", "text": "hello"}
ws.send(R"({"msgType": "chat", "text": "hello"})");
```

```cpp
// 服务器使用 "eventID" 作为事件字段名
Ws ws{ "ws://example.com/socket", "eventID" };

ws.on("notification", [](const WsData& data) {
    std::println("通知: {}", data.payload);
});

ws.connect();
// 消息格式: {"eventID": "notification", "content": "..."}
```

## 消息格式

消息必须是 JSON 格式，包含构造时指定的事件字段（默认为 `event`）：

```json
{
    "event": "事件名",
    "其他字段": "值"
}
```

`WsData.event` 从 JSON 的指定字段提取，`WsData.payload` 保存原始消息。

## 线程安全保证

| 操作 | 线程安全 |
|------|---------|
| `connect()` | 是（单线程调用） |
| `disconnect()` | 是（单线程调用） |
| `send()` | 是（内部检查连接状态） |
| `execCb()` | 是（单线程调用） |
| `on()` / `once()` | 是（单线程调用） |
| 接收线程入队 | 是（mutex 保护） |

## 注意事项

1. **execCb() 必须在主线程调用**：这是设计核心，确保回调线程安全
2. **移动后不要使用旧对象**：移动后旧对象的 `m_state` 为空，使用属于 use-after-free
3. **消息格式要求**：必须是 JSON，且包含构造时指定的事件字段
4. **析构自动断开**：无需手动调用 `disconnect()`
