import asyncio
import json
import websockets
from playwright.async_api import async_playwright
import os

PORT = 8080
HOST = "localhost"

class BrowserBridge:
    def __init__(self):
        self.playwright = None
        self.browser = None
        self.context = None
        self.page = None

    async def start_browser(self):
        if not self.playwright:
            self.playwright = await async_playwright().start()
            self.browser = await self.playwright.chromium.launch(headless=False)
            self.context = await self.browser.new_context()
            self.page = await self.context.new_page()
            
            # Setup console logging relay
            self.page.on("console", lambda msg: self.broadcast(f"[Console] {msg.text}"))
            self.page.on("requestfailed", lambda req: self.broadcast(f"[Network Error] {req.url}: {req.failure}"))

    async def broadcast(self, message):
        if hasattr(self, 'current_ws') and self.current_ws:
            try:
                await self.current_ws.send(message)
            except:
                pass

    async def handle_command(self, websocket, data):
        cmd_id = data.get("id")
        command = data.get("command")
        params = data.get("params", {})

        print(f"Received command: {command} with params {params}")

        if command == "browserAgent.connect":
            await websocket.send(f"Bridge active. Connected to {HOST}:{PORT}")
            return

        await self.start_browser()

        if command == "browserAgent.navigate":
            url = params.get("url")
            await websocket.send(f"Navigating to {url}...")
            await self.page.goto(url)
            await websocket.send(f"Navigation to {url} complete.")
            
            # Optional: Automatic screenshot on navigate
            screenshot_path = "latest_view.png"
            await self.page.screenshot(path=screenshot_path)
            await websocket.send(f"Screenshot saved to {os.path.abspath(screenshot_path)}")

        elif command == "browserAgent.screenshot":
             path = "manual_screenshot.png"
             await self.page.screenshot(path=path)
             await websocket.send(f"Manual screenshot saved to {os.path.abspath(path)}")

        elif command == "browserAgent.click":
            selector = params.get("selector")
            await websocket.send(f"Clicking {selector}...")
            await self.page.click(selector)
            await websocket.send(f"Clicked {selector}.")

    async def server_handler(self, websocket):
        self.current_ws = websocket
        async for message in websocket:
            try:
                data = json.loads(message)
                if data.get("type") == "command":
                    await self.handle_command(websocket, data)
            except Exception as e:
                await websocket.send(f"Error: {str(e)}")

async def main():
    bridge = BrowserBridge()
    print(f"Starting Browser Bridge WS Server on ws://{HOST}:{PORT}")
    async with websockets.serve(bridge.server_handler, HOST, PORT):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
