import asyncio
import websockets
import os

async def test():
    async with websockets.connect("wss://pclead.co.uk/apiws") as ws:
        payload = os.urandom(64)
        await ws.send(payload)
        resp = await asyncio.wait_for(ws.recv(), timeout=5)
        print("Received", len(resp), "bytes")

asyncio.run(test())
