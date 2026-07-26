import asyncio, ssl

async def test():
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    print("Connecting...")
    reader, writer = await asyncio.open_connection("149.154.167.220", 443, ssl=ctx, server_hostname="sprinthost.ru")
    print("Connected!")
    req = b"GET /apiws HTTP/1.1\r\nHost: sprinthost.ru\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n"
    writer.write(req)
    await writer.drain()
    
    print("Waiting for response...")
    data = await reader.read(4096)
    print(f"Response: {data}")

asyncio.run(test())
