**English** | [简体中文](README.zh-CN.md)

<p align="center"><img src="assets/logo.svg" alt="uv" width="200"></p>

<h1 align="center">uv</h1>

<p align="center">Nine event loops, one dumb HTTP handler: Python asyncio / uvloop / uvicorn against C on raw liburing.</p>

**This playground races the same trivial HTTP workload — reply `hello`, or stream zero bytes — across every
event-loop stack its author could wire up in a single file, because a benchmark that changes both the loop and
the handler measures neither: keep the handler dumb and the gap in throughput is the loop.**

> **Status: archived.** Migrated out of a private collection on 2026-09-30 and kept for reference. Not
> maintained, no CI, no test suite. The commands below are read off the sources and are expected to work on a
> normal Linux box; the numbers are yours to reproduce.

## Quick start

Serve the smallest app and measure it — about 20 seconds, no build step:

```bash
python -m venv .venv && . .venv/bin/activate
pip install 'uvicorn[standard]' aiofiles

uvicorn hello:a --host 0.0.0.0 --port 8080
```

In a second terminal, open 100 keep-alive-free connections and print MiB/s every three seconds:

```bash
python get 127.0.0.1 8080 hello 100
```

Swap the app for the zero streamer — `/{chunks}/{bytes}` per chunk — by restarting uvicorn with `zero:a` and
asking for 64 MiB:

```bash
python get 127.0.0.1 8080 64/1048576 100
```

The C half, on Linux with `liburing` installed (`liburing-dev` on Debian/Ubuntu; `io_uring` needs a recent
enough kernel):

```bash
cd uring
gcc srv.c -Wall -Ofast -s -march=native -o srv
gcc cli.c -Wall -Ofast -s -march=native -l:liburing.a -ocli
./srv &                       # accept loop on :8000, prints accepts/s
./cli 127.0.0.1               # io_uring connect storm, prints connections/s
```

## What it can do now

Python ASGI 3 apps — one file each, single-letter entry point `a`, run with `uvicorn <module>:a`:

| app | request path | response |
| --- | --- | --- |
| `hello.py` | any | `hello`, 5 bytes, one body chunk |
| `zero.py` | `/{chunks}/{bytes}` | `chunks × bytes` zero-filled bytes, streamed |
| `rand.py` | `/{total_bytes}/{chunk_bytes}` | that many bytes from `/dev/random`, async reads, default 32 bytes in one chunk |
| `rand2.py` | any | 1 KiB read of `/dev/random` per request, blocking the loop |
| `range.py` | any | one line per second for ten seconds, streamed |
| `idle.py` | any | never responds; holds the connection open |

uvloop load generators, both printing MiB/s every three seconds:

- `get` — `<host> <port> <path> <connections>`, repeatable as further groups of four, reconnects on close.
- `cli.py` — the same probe for a single target, `python cli.py <host> <port> <path> <connections>`.

`hello/` — the same "hello" as a bare asyncio server: a `uvloop`-driven `create_server` that writes `hello`,
closes and counts connections every five seconds, plus a client that reconnects as fast as the loop allows
(`N` workers). `hello/docker-compose.yml` runs one server and 20 client replicas on one bridge network.

`uring/` — the C side, one idea per file:

- `srv.c` — plain `accept()` loop on `:8000`, counts accepts; measures the accept path with no response at all.
- `cli.c` — 512 `io_uring_prep_connect` requests in flight, refilled as they complete.
- `io_uring_server.c` — accept + send of a canned `HTTP/1.0 200\n\nhello`, `PORT` and `RING` env knobs, closes after send.
- `io_uring_zero_server.c` — accept, then one send of `BUF` zeros (default 512 KiB) per connection.
- `io_uring_zero_client.c` — `host port count` triples, receives into `BUF`, prints MiB/s.
- `io_uring_server.bak.c` — an earlier variant that tags completions with a typed struct inside `user_data`; kept for reference.

Container images: the root `dockerfile` is `python` + `uvicorn` + the sources, so
`docker build -t uv . && docker run --rm -p 8080:8080 uv uvicorn hello:a --host 0.0.0.0 --port 8080` serves any
app above; `uring/dockerfile` is `ubuntu` with the two prebuilt C binaries copied in.

## Architecture

The repository is two independent halves that only ever meet at a TCP socket, and they share exactly two
workloads: a five-byte canned response, and a stream of zeros. Nothing is packaged, imported across directories
or configurable beyond a handful of environment variables — every file is a whole experiment you can read in
one sitting, which is the point: a number you cannot explain down to the syscall is not a result.

```
.
├── hello.py  zero.py  rand.py  rand2.py  range.py  idle.py   ASGI apps, one workload each
├── get  cli.py                     uvloop load generators (MiB/s)
├── dockerfile                      python + uvicorn image for the apps above
├── hello/                          raw asyncio/uvloop server + client fan-out, docker-compose fleet
│   ├── server/__main__.py          create_server, writes "hello", closes
│   ├── client/__main__.py          reconnect loop, N workers
│   └── docker-compose.yml          1 server, 20 client replicas, network "idle"
├── uring/                          C + liburing
│   ├── srv.c  cli.c                accept-loop server, connect-storm client
│   ├── io_uring_server.c           accept + send "hello"
│   ├── io_uring_zero_server.c      accept + send BUF zeros
│   ├── io_uring_zero_client.c      connect storm + recv, MiB/s
│   ├── makefile
│   └── dockerfile                  ubuntu + the two binaries
├── assets/logo.svg
├── docs/project-status.md          provenance of the archive
└── LICENSE
```

Data flow for every experiment: a generator (Python `get`/`cli.py` or C `cli`) holds many connections open and
counts the bytes or connections that come back; the server (uvicorn, bare asyncio, or io_uring) does the
smallest possible thing per connection. The printed rate is the only output — no files, no plots, no history.

## How to develop

There is no test suite, no linter configuration and no package metadata; the project is a scratch harness, and
its check is watching the printed rate stay finite and non-zero while a server runs.

- Serve an app: `uvicorn <module>:a --host 0.0.0.0 --port 8080`.
- Build the C pieces: `cd uring && make` (compiles `srv` and `cli` with the flags above, then `docker build -tio .`).
- Reproduce a comparison: run one server, run one generator against it, note MiB/s or conn/s.

## How to contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) — in short: one file per experiment, keep the handler dumb, and if you
add a result, say which kernel, which flags and which hardware produced it.

## Roadmap

**Implemented.** Six ASGI workloads; two uvloop load generators; a bare-asyncio "hello" server with a 20-replica
compose fleet; five C programs covering accept-only, io_uring connect storms, io_uring hello and io_uring zero
streaming in both directions; container images for both halves.

**Planned.** Nothing — the archive is closed. If it were revived, the first thing worth having is a results file
that records rate, kernel version, `-march`/`-Ofast` flags and CPU for each pairing, because io_uring behaviour
follows the kernel and a rate without that context cannot be compared to anything later.

Beyond that, the shape of the idea: any number of loop stacks can be dropped in behind the same two workloads,
so "which runtime carries how much of this machine" stays a question you can answer by adding one file.

## License

GNU Affero General Public License v3.0 only (AGPL-3.0-only), © ZisIsNotZis. See [LICENSE](LICENSE).
