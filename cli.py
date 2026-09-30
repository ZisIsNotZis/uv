from asyncio import *
from uvloop import install
from sys import argv
from time import time

N = 0
class MyProtocol(Protocol):
    def connection_made(_, t):
        t.write(b'GET ' + argv[3].encode() + b' HTTP/1.0\n\n')
    def data_received(_, a):
        global N
        N += len(a)
    def connection_lost(_, e):
        if e:
            print(e)
        Task(get_event_loop().create_connection(MyProtocol, *argv[1:3]))

async def main():
    global N
    for _ in range(int(argv[4])):
        await get_event_loop().create_connection(MyProtocol, *argv[1:3])
    while True:
        t = time()
        await sleep(3)
        print((N >> 20) / (time() - t))
        N = 0

install()
new_event_loop().run_until_complete(main())
