from playwright.sync_api import sync_playwright

def find_elements(url):
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.goto(url)
        
        print(f"--- Elements on {url} ---")
        
        # Find all buttons
        buttons = page.query_selector_all("button")
        print(f"Found {len(buttons)} buttons:")
        for b in buttons:
            print(f"  - {b.inner_text()} (id: {b.get_attribute('id')})")
            
        # Find all inputs
        inputs = page.query_selector_all("input")
        print(f"Found {len(inputs)} inputs:")
        for i in inputs:
            print(f"  - {i.get_attribute('placeholder')} (id: {i.get_attribute('id')})")
            
        browser.close()

if __name__ == "__main__":
    import sys
    url = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:3000"
    find_elements(url)
