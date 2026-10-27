# Contributing to uv

`uv` is an archived playground: small, self-contained experiments that put the same trivial HTTP workload on
different event loops (Python asyncio/uvloop/uvicorn, C on liburing) and print a rate. See
[README.md](README.md) for what each piece does and how to run it.

## Scope

Welcome: another loop stack behind the same two workloads (a canned `hello` reply, a stream of zeros), a
bug fix in an existing experiment, or a recorded set of results.

Out of scope: frameworks, packaging, a plugin layer, or anything that makes an experiment need more than one
file. This is a scratch harness, not a service.

## Ground rules

- **One file per experiment.** A new runtime means one new file that starts, serves and prints — not a shared
  abstraction.
- **Keep the handler dumb.** If a change makes the per-request work noticeably heavier than `write(fd, "hello", 5)`,
  the comparison stops meaning anything.
- **No deleted experiments.** Marking something superseded in `README.md` is fine; deleting the file is not.
- **Checks are manual.** There is no test suite or linter. Before opening a PR, run the server and the matching
  generator and confirm the printed rate is finite and non-zero.

## Running the checks

```bash
pip install 'uvicorn[standard]' aiofiles
uvicorn hello:a --host 0.0.0.0 --port 8080      # in one terminal
python get 127.0.0.1 8080 hello 100             # in another
```

```bash
cd uring && make                                # compiles srv and cli (needs liburing)
./srv & ./cli 127.0.0.1
```

The working tree should stay clean: build products (`uring/srv`, `uring/cli`) are ignored.

## Pull requests

- Small and focused; one experiment or one fix per PR.
- Say what you ran, on which kernel and CPU, and paste the printed rate if the change is meant to move it.
- New results belong next to the flags that produced them (`-Wall -Ofast -s -march=native`, ring size, `BUF`).
  A rate without that context is not comparable and will be asked about.
- No dependency additions to the C half beyond `liburing`; on the Python side, `uvicorn`, `uvloop` and
  `aiofiles` are the whole set.

## Issues

Say what you ran, what you expected, what you saw, and the kernel version for anything in `uring/`. Feature
requests for abstractions will be closed with a pointer to the scope section — that is not a judgement on the
idea, only on what this archive is for.
