# 聊天室（Chat System）

基于 Linux epoll 主从 Reactor 模型的多线程 TCP 多人聊天室，C/S 架构。

- 网络层：epoll + 主从 Reactor，支持高并发连接
- 存储：MySQL（用户/好友/群组/消息/文件），Redis（在线状态）
- 功能：注册登录（密码/邮箱验证码）、好友（添加/删除/屏蔽）、私聊、群组（创建/解散/申请审批/踢人/设管理员）、文件传输、离线消息与离线文件补发、心跳保活、验证码找回密码
- 双端客户端：C++ 命令行客户端 + WebSocket 网页版（网页端目前尚未完成，暂不可用）

## 目录结构

```
├── CMakeLists.txt        # 构建脚本
├── config.example.ini    # 配置文件模板（复制为 config.ini 使用）
├── schema.sql            # 数据库建表脚本
├── server/               # 服务端
├── client/               # 客户端
├── common/               # 公共代码（crypto/email/config）
├── database/             # MySQL/Redis 连接池
├── include/              # 头文件
└── web/                  # 前端页面
```

## 依赖

- 编译工具：`g++`、`cmake`（>= 3.15）
- 数据库：MySQL、Redis
- 库：`pkg-config`、`libmysqlclient-dev`、`libhiredis-dev`、`libcurl4-openssl-dev`、`libssl-dev`
- 日志库：`spdlog`（可选，若未安装会用 `third_party/spdlog`）

Ubuntu/Debian 安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libmysqlclient-dev libhiredis-dev \
                    libcurl4-openssl-dev libssl-dev redis-server mysql-server
```

## 配置

1. 复制配置模板：

```bash
cp config.example.ini config.ini
```

2. 编辑 `config.ini`，填入你自己的 MySQL 账号密码和邮箱 SMTP 授权码。

> `config.ini` 已被 `.gitignore` 忽略，**不会被上传**，密码也不会出现在代码里。

## 数据库初始化

1. 启动 MySQL 和 Redis：

```bash
sudo systemctl start mysql redis-server
```

2. 建库建表：

```bash
mysql -u root -p < schema.sql
```

3. 建一个数据库账号（可选，与 `config.ini` 保持一致）：

```sql
CREATE USER 'chat'@'localhost' IDENTIFIED BY '你的密码';
GRANT ALL PRIVILEGES ON chat_system.* TO 'chat'@'localhost';
FLUSH PRIVILEGES;
```

## 目录准备

文件传输需要 `uploads/` 目录（已加入 `.gitignore`），首次运行前创建：

```bash
mkdir -p uploads
```

## 编译

```bash
cmake -B build
cmake --build build -j
```

## 运行

### 服务端（必须从 `build/` 目录启动）

网页页面和配置文件都使用相对路径（`../web/index.html`、`../config.ini`），因此**必须 `cd build` 后启动**才能正确加载网页版：

```bash
cd build
./chat_server 8888                 # 端口 + 可选绑定地址 + 可选配置路径
./chat_server 8888 0.0.0.0 ../config.ini   # 显式指定绑定地址和配置路径
```

> 默认监听 `0.0.0.0`（所有网卡，局域网内设备均可访问）；若只想本机访问可绑定 `127.0.0.1`。

### C++ 客户端

```bash
./build/chat_client 127.0.0.1 8888   # 端口需与服务端一致
```

### 网页版客户端（局域网访问）

> **网页端目前尚未完成，暂不可用**，以下为预留的访问方式（待完成后按此操作）。

无需安装任何东西，同一网络内的设备用浏览器直接访问服务端 IP 即可：

```
http://服务器IP:8888
```

步骤：

1. 查看本机局域网 IP：

```bash
hostname -I
# 例：10.30.0.182（wlp4s0 无线网卡）；忽略 127.0.0.1 回环和 docker0 网卡
```

2. 放行防火墙端口：

```bash
sudo ufw allow 8888
```

3. 验证服务端确实监听在所有网卡：

```bash
ss -tlnp | grep 8888    # 应显示 0.0.0.0:8888
```

4. 同一局域网内的设备（手机连同一 WiFi 等）浏览器打开 `http://10.30.0.182:8888` 即可。

> **常见问题**
> - 打不开：防火墙未放行 / 手机与电脑不在同一网络 / 路由器开启了 AP 或客户端隔离
> - 页面空白：多半没有从 `build/` 目录启动（网页路径 `../web/index.html` 找不到）
> - 能开页面但登录连不上：服务端可能绑定了 `127.0.0.1`，改用 `./chat_server 8888 0.0.0.0`

## 自定义协议（简）

固定 20 字节消息头 `MsgHeader` + 变长包体（均小端序）。

**消息头 `MsgHeader`（20 字节）：**

- `length`（4 字节）：整包总长度 = 消息头 + 包体，用于粘包拆包
- `version`（2 字节）：协议版本号，当前为 1
- `type`（2 字节）：消息类型，决定包体格式（如 100=登录请求、300=私聊、700=心跳等）
- `sequence`（4 字节）：请求序号，用于请求/响应配对
- `timestamp`（8 字节）：消息时间戳

**包体 `Body`（变长）：** 按 `type` 决定格式；字符串统一用 4 字节长度前缀 + 原始字节编码。
