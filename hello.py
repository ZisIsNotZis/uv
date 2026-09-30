async def a(_, i, a):
    assert _['type'] == 'http'
    await a({
        'type': 'http.response.start',
        'status': 200
    })
    await a({
        'type': 'http.response.body',
        'body': b'hello'
    })
