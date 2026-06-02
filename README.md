# MiniIPC

轻量级进程间通信（IPC）框架，基于 TCP 的发布-订阅（Pub/Sub）模式，附带 Web 调试面板。

## 技术栈

| 层 | 语言/构建 | 关键依赖 |
|---|-----------|----------|
| Core | C++17, CMake + Ninja | epoll, POSIX sockets, yaml-cpp |
| Bridge | C++17, Boost.Beast | nlohmann/json |
| Dashboard | Vue 3, TypeScript, Vite 6 | — |

## 架构

```
Dashboard (Vue 3) ←──WebSocket──→ Bridge (C++) ←──Core API──→ Discovery Daemon (C++)
                                      │                              │
                              Core libmini_ipc_core.so ←─────────────┘
                                      │
                              ┌───────┴───────┐
                          Publisher      Subscriber
                         (TCP server)   (TCP client)
                              │               │
                              └─── P2P TCP ───┘
```

## 快速开始

### 构建

```bash
cmake -B build -G Ninja && cmake --build build
```

### 运行示例

```bash
# 终端 1: 启动发现服务
./build/core/examples/discovery_daemon

# 终端 2: 启动订阅者
./build/core/examples/listener

# 终端 3: 启动发布者
./build/core/examples/talker
```

### 启动 Dashboard

```bash
cd dashboard && npm install && npm run dev
```

访问 `http://127.0.0.1:5173`，连接到 `ws://127.0.0.1:9000`（需先运行 Bridge）。

## 核心设计

### Node（事件循环核心）

单一 epoll 实例管理三种 fd：

```
epoll_fd
├── Publisher server_fd   — accept subscriber 连接
├── Subscriber client_fd  — 接收消息，帧解析，触发回调
└── Pending sc_fd         — 等待 daemon 通知 publisher 上线
```

### Discovery Daemon（发现与注册中心）

```
TopicEntry { port, port_available, publisher_fd, waiting_fds }
```

- Publisher 通过长连接注册，daemon 监控其存活
- Subscriber 查询时返回端口（publisher 在线）或进入等待（publisher 离线）
- Publisher 上线时通知所有等待中的 subscriber

### 通信协议

| 通道 | 协议 | 帧格式 |
|------|------|--------|
| Pub/Sub 数据 | TCP 直连 | 4 字节大端长度 + payload |
| Daemon 注册/发现 | TCP | 文本命令：PUB / SUB / OK / WAITING / PORT |

### Subscriber 状态机

```
P2P Connected ──EOF──→ Pending ──PORT──→ P2P Connected
     │                    │                    │
     └──connect失败──→   Pending ←──connect失败──┘
```

- **P2P Connected**: 直连 publisher，通过 epoll 接收帧消息
- **Pending**: 连在 daemon 上等待 PORT 通知

## 已实现功能

| 功能 | 说明 |
|------|------|
| Pub/Sub 基础通信 | Publisher 创建 TCP server，Subscriber 直连 |
| Discovery 注册中心 | 支持 PUB/SUB/WAITING/PORT，publisher 存活监控 |
| 帧协议 | writev 写入，缓冲区拼帧读取，解决 TCP 粘包拆包 |
| SIGPIPE 处理 | Publisher 安全处理 subscriber 掉线 |
| 断连重订阅 | Subscriber 检测 EOF 后自动查询 daemon 重连 |
| connect 失败 fallback | connect 失败时回退到 pending 等待模式 |
| WebSocket Bridge | publish 通道可用（subscribe 待实现） |
| Dashboard | 连接 / 订阅 / 发布 / 消息列表 / 日志面板 |
| CI | GitHub Actions 构建 + 冒烟测试 |

## 已知问题

| 优先级 | 位置 | 问题 |
|--------|------|------|
| 中 | daemon | `read_bytes <= 0` 对 publisher_fd 未清理 epoll/close |
| 中 | daemon | 旧 publisher fd 被覆盖时未清理，epoll 残留 |
| 中 | daemon | nested loop 在 read_bytes <= 0 处理中效率低 |
| 低 | node.cpp | `read_bytes == -1` 未清理 map |
| 低 | bridge | `handle_subscribe` 未创建实际 Subscriber |
| 低 | daemon | 文本协议无帧定界 |

## 文件结构

```
mini-ipc/
├── CMakeLists.txt
├── README.md
├── .github/workflows/ci.yml
├── core/
│   ├── CMakeLists.txt
│   ├── include/mini_ipc/
│   │   ├── node.hpp
│   │   ├── publisher.hpp
│   │   ├── subscriber.hpp
│   │   └── param_manager.hpp
│   ├── src/
│   │   ├── node.cpp
│   │   └── param_manager.cpp
│   ├── config/comm.yaml
│   └── examples/
│       ├── discovery_daemon.cpp
│       ├── talker.cpp
│       └── listener.cpp
├── bridge/
│   ├── CMakeLists.txt
│   ├── include/mini_ipc/
│   │   ├── bridge_dispatcher.hpp
│   │   ├── websocket_server.hpp
│   │   └── websocket_session.hpp
│   └── src/
│       ├── main.cpp
│       ├── bridge_dispatcher.cpp
│       ├── websocket_server.cpp
│       └── websocket_session.cpp
└── dashboard/
    ├── package.json
    ├── vite.config.ts
    ├── src/
    │   ├── App.vue
    │   ├── main.ts
    │   ├── styles.css
    │   └── api/wsClient.ts
    └── index.html
```
