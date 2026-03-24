from playwright.sync_api import sync_playwright
import os

def test_static_html(file_path):
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        
        # Open local file
        abs_path = os.path.abspath(file_path)
        page.goto(f"file://{abs_path}")
        
        print(f"--- Snapshot of {file_path} ---")
        print(f"Title: {page.title()}")
        print(f"Current URL: {page.url}")
        
        # Take a screenshot
        screenshot_path = "static_page.png"
        page.screenshot(path=screenshot_path)
        print(f"Screenshot saved to {os.path.abspath(screenshot_path)}")
        
        browser.close()

if __name__ == "__main__":
    import sys
    file_path = sys.argv[1] if len(sys.argv) > 1 else "index.html"
    test_static_html(file_path)
