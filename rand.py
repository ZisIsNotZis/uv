from aiofiles import open
F = None
async def a(_, i, a):
    global F
    if not F:
        F = await open('/dev/random', 'rb')
    assert _['type'] == 'http'
    await a({
        'type': 'http.response.start',
        'status': 200
    })
    _,i = _['path'][1:].partition('/')[::2]
    _ = int(_ or 32)
    i = int(i or 1048576)
    while _ > i:
        await a({
            'type': 'http.response.body',
            'body': await F.read(i),
            'more_body': True
        })
        _ -= i
    await a({
        'type': 'http.response.body',
        'body': await F.read(_)
    })
