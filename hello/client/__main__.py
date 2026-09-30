from asyncio import *
from uvloop import install
from os import getenv

N = 0
class MyProtocol(Protocol):
    def data_received(*_):
        global N
        N += 1

async def f():
    while True:
        try:
            await get_event_loop().create_connection(MyProtocol, 'localhost', 8080)
        except:
            pass

async def main():
    global N
    for N in range(int(getenv('N',1))):
        Task(f())
    while True:
        await sleep(5)
        print(N, flush=True)
        N = 0

install()
new_event_loop().run_until_complete(main())
