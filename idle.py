from asyncio import Future

N = 0
async def a(_, i, a, f=Future()):
    global N
    assert _['type'] == 'http'
    N += 1
    print(N, end='\r')
    try:
        await f
    except:
        N -= 1
        print(N, end='\r')
