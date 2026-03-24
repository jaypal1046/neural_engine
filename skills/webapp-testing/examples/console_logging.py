from playwright.sync_api import sync_playwright

def capture_logs(url):
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Listen for console events
        page.on("console", lambda msg: print(f"[Browser Console] {msg.type}: {msg.text}"))
        page.on("pageerror", lambda err: print(f"[Browser Error] {err.message}"))
        
        print(f"--- Navigating to {url} ---")
        page.goto(url)
        
        # Wait a bit for potential async logs
        page.wait_for_timeout(2000)
        
        browser.close()

if __name__ == "__main__":
    import sys
    url = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:3000"
    capture_logs(url)
