from asyncio import *
from uvloop import install

N = 0
class MyProtocol(Protocol):
    def connection_made(_, t):
        global N
        N += 1
        t.write(b'hello')
        t.close()

async def main():
    global N
    await get_event_loop().create_server(MyProtocol, '0.0.0.0', 8080, reuse_port=True)
    while True:
        await sleep(5)
        print(N, flush=True)
        N = 0

install()
new_event_loop().run_until_complete(main())
