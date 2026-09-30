def byte(n, c={}):
    t = c.get(n)
    if not t:
        t = c[n] = bytes(n)
    return t

async def a(_, i, a):
    assert _['type'] == 'http'
    await a({
        'type': 'http.response.start',
        'status': 200
    })
    _,i = map(int, _['path'][1:].split('/'))
    i = byte(i)
    for _ in range(_-1, -1, -1):
        await a({
            'type': 'http.response.body',
            'body': i,
            'more_body': bool(_)
        })
