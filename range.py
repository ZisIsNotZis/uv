from asyncio import sleep
async def a(_, i, a):
    assert _['type'] == 'http'
    await a({
        'type': 'http.response.start',
        'status': 200
    })
    for i in range(10):
        await sleep(1)
        await a({
            'type': 'http.response.body',
            'body': f'{i}\n'.encode(),
            'more_body': i < 9
        })
