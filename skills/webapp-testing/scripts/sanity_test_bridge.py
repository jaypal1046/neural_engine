import asyncio
import websockets
import json

async def test_bridge():
    uri = "ws://localhost:8080"
    print(f"Connecting to {uri}...")
    async with websockets.connect(uri) as websocket:
        # 1. Connect
        print("Sending connect command...")
        await websocket.send(json.dumps({
            "id": "test_1",
            "type": "command",
            "command": "browserAgent.connect",
            "params": {}
        }))
        response = await websocket.recv()
        print(f"Bridge Response: {response}")

        # 2. Navigate
        print("Sending navigate command to google.com...")
        await websocket.send(json.dumps({
            "id": "test_2",
            "type": "command",
            "command": "browserAgent.navigate",
            "params": {"url": "https://www.google.com"}
        }))
        
        # Listen for multiple progress messages
        for _ in range(3):
            response = await websocket.recv()
            print(f"Bridge Status: {response}")

if __name__ == "__main__":
    try:
        asyncio.run(test_bridge())
    except Exception as e:
        print(f"Test Failed: {e}")
