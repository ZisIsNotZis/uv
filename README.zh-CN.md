[English](README.md) | **简体中文**

<p align="center"><img src="assets/logo.svg" alt="uv" width="200"></p>

<h1 align="center">uv</h1>

<p align="center">九种事件循环，同一个傻瓜 HTTP 处理器：Python 的 asyncio / uvloop / uvicorn 对阵裸 liburing 写的 C。</p>

**这个实验场把同一个最简单的 HTTP 负载——回一句 `hello`，或者灌一堆零字节——搬到它能凑出来的每一种事件循环
上跑，因为同时换了循环又换了处理器，测出来的东西既不是循环也不是处理器：把处理器按住不动，吞吐量之间的差距就
只剩循环本身。**

> **状态：已归档。** 2026-09-30 从一份私有收藏中迁出，此后仅作参考保留。不再维护，没有 CI，也没有测试。
> 下面的命令都是照着源码写出来的，在普通 Linux 机器上应当可用；具体数字请自行复现。

## 快速开始

跑起最小的那个应用并测它——大约 20 秒，不用编译：

```bash
python -m venv .venv && . .venv/bin/activate
pip install 'uvicorn[standard]' aiofiles

uvicorn hello:a --host 0.0.0.0 --port 8080
```

在另一个终端里开 100 条短连接，每三秒打印一次 MiB/s：

```bash
python get 127.0.0.1 8080 hello 100
```

换成零字节流——路径是 `/{块数}/{每块字节数}`——把 uvicorn 换成 `zero:a` 重启，然后要 64 MiB：

```bash
python get 127.0.0.1 8080 64/1048576 100
```

C 那一半，需要装好 `liburing` 的 Linux（Debian/Ubuntu 上是 `liburing-dev`，且内核要足够新才支持 `io_uring`）：

```bash
cd uring
gcc srv.c -Wall -Ofast -s -march=native -o srv
gcc cli.c -Wall -Ofast -s -march=native -l:liburing.a -ocli
./srv &                       # 监听 :8000 的 accept 循环，打印每秒接受连接数
./cli 127.0.0.1               # io_uring 连接风暴，打印每秒连接数
```

## 目前能做什么

Python ASGI 3 应用——一个文件一个负载，入口点是单字母的 `a`，用 `uvicorn <模块>:a` 启动：

| 应用 | 请求路径 | 响应 |
| --- | --- | --- |
| `hello.py` | 任意 | `hello`，5 字节，一个 body 块 |
| `zero.py` | `/{块数}/{字节数}` | `块数 × 字节数` 个零字节，分块流式发送 |
| `rand.py` | `/{总字节数}/{每块字节数}` | 该总数的 `/dev/random` 字节，异步读取，默认 32 字节一块 |
| `rand2.py` | 任意 | 每个请求阻塞式读 1 KiB `/dev/random`，卡住事件循环 |
| `range.py` | 任意 | 每秒一行，共十行，流式发送 |
| `idle.py` | 任意 | 永不响应，只把连接晾在那里 |

两个 uvloop 压测端，都是每三秒打印一次 MiB/s：

- `get` —— `<host> <port> <path> <connections>`，后面可以按四元组继续接更多目标，连接断开后自动重连。
- `cli.py` —— 同样的探针，只针对单个目标：`python cli.py <host> <port> <path> <connections>`。

`hello/` —— 同一句 "hello"，但只用裸 asyncio：`uvloop` 驱动的 `create_server` 写下 `hello` 就关闭，每五秒
统计一次连接数；客户端不限速地重连（`N` 个并发）。`hello/docker-compose.yml` 在一个桥接网络里跑 1 个服务端
和 20 个客户端副本。

`uring/` —— C 那一半，一个文件一个想法：

- `srv.c` —— 朴素的 `accept()` 循环，监听 `:8000`，只数接受次数；完全不发响应，单测 accept 路径。
- `cli.c` —— 保持 512 个 `io_uring_prep_connect` 在飞，完成一个补一个。
- `io_uring_server.c` —— accept 之后 send 一段写死的 `HTTP/1.0 200\n\nhello`，`PORT` 和 `RING` 两个环境变量可调，发完关闭。
- `io_uring_zero_server.c` —— accept 之后一次 send `BUF` 个零字节（默认 512 KiB）。
- `io_uring_zero_client.c` —— 参数是 `host port count` 的三元组，收进 `BUF`，打印 MiB/s。
- `io_uring_server.bak.c` —— 更早的版本，用 `user_data` 里放带类型的结构体来标记完成事件；留作参考。

容器镜像：根目录的 `dockerfile` 是 `python` + `uvicorn` + 全部源码，所以
`docker build -t uv . && docker run --rm -p 8080:8080 uv uvicorn hello:a --host 0.0.0.0 --port 8080` 就能提供
上面任意一个应用；`uring/dockerfile` 是 `ubuntu` 加上编译好的两个 C 程序。

## 架构

这个仓库是两半彼此独立的东西，它们只在 TCP socket 上相遇，共享的负载恰好只有两个：一句五字节的固定回答，和
一串零。这里没有打包，没有跨目录 import，除了几个环境变量之外没有任何配置——每个文件都是一个能一口气读完
的完整实验，而这正是重点：解释不到 syscall 层的数字不算结果。

```
.
├── hello.py  zero.py  rand.py  rand2.py  range.py  idle.py   各一个负载的 ASGI 应用
├── get  cli.py                     uvloop 压测端（MiB/s）
├── dockerfile                      python + uvicorn 镜像
├── hello/                          裸 asyncio/uvloop 服务端 + 客户端压测，compose 集群
│   ├── server/__main__.py          create_server，写 "hello" 后关闭
│   ├── client/__main__.py          重连循环，N 个并发
│   └── docker-compose.yml          1 个服务端，20 个客户端副本，网络 "idle"
├── uring/                          C + liburing
│   ├── srv.c  cli.c                accept 循环服务端、连接风暴客户端
│   ├── io_uring_server.c           accept + send "hello"
│   ├── io_uring_zero_server.c      accept + send BUF 个零字节
│   ├── io_uring_zero_client.c      连接风暴 + recv，MiB/s
│   ├── makefile
│   └── dockerfile                  ubuntu + 两个编译产物
├── assets/logo.svg
├── docs/project-status.md          归档来源说明
└── LICENSE
```

每个实验的数据流都一样：压测端（Python 的 `get`/`cli.py` 或 C 的 `cli`）拉着大量连接，数回来的字节数或连接数；
服务端（uvicorn、裸 asyncio 或 io_uring）对每条连接做尽可能少的事。唯一的输出就是打印出来的速率——没有文件，
没有图表，没有历史记录。

## 如何开发

没有测试套件，没有 lint 配置，也没有包元数据；这就是个草稿级的实验台，它的检查方式就是在服务端跑着的时候，
看打印出来的速率既不是零也不是无穷。

- 启动应用：`uvicorn <模块>:a --host 0.0.0.0 --port 8080`。
- 编译 C 部分：`cd uring && make`（按上面的参数编译出 `srv` 和 `cli`，然后执行 `docker build -tio .`）。
- 复现对比：起一个服务端，起一个压测端对着它跑，记下 MiB/s 或连接数/秒。

## 如何贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)——一句话：一个实验一个文件，处理器保持傻瓜，如果你要贴结果，请写清是哪
个内核、哪组编译参数、什么硬件跑出来的。

## 路线图

**已实现。** 六个 ASGI 负载；两个 uvloop 压测端；一个裸 asyncio 的 "hello" 服务端外加 20 副本的 compose 集群；
五个 C 程序，覆盖只收不答、io_uring 连接风暴、io_uring 发 hello、以及 io_uring 双向零字节流；两半各有一个
容器镜像。

**计划中。** 没有——这个归档已经封存。如果哪天重启它，第一件值得做的事是一份结果文件，把每种组合的速率、内核
版本、`-march`/`-Ofast` 参数和 CPU 都记下来，因为 io_uring 的行为跟着内核走，少了这些背景的速率跟后来的任何
东西都没法比。

再往远了说，这个想法的形状是：任意多种循环栈都可以塞到同样两个负载背后，于是"某个运行时能吃下这台机器的多少"
始终是一个加一个文件就能回答的问题。

## 许可

GNU Affero 通用公共许可证第 3 版（AGPL-3.0-only），© ZisIsNotZis。见 [LICENSE](LICENSE)。
