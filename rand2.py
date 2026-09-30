async def a(_, i, a, f=open('/dev/random','rb')):
    assert _['type'] == 'http'
    await a({
        'type': 'http.response.start',
        'status': 200
    })
    await a({
       'type': 'http.response.body',
       'body': f.read(1024),
    })
