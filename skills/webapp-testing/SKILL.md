---
name: webapp-browser
description: Controls the embedded browser inside Neural Studio. Supports web surfing, inspecting and copying DOM elements, reading console logs, taking screenshots, and debugging UIs — all via [BROWSER: cmd] directives that talk directly to the built-in webview.
intents:
  - WEBAPP_TESTING
  - WEB_SURFING
  - BROWSER_INSPECT
  - CONSOLE_CHECK
---

# Built-in Browser Control

> [!IMPORTANT]
> **You have a real embedded browser inside Neural Studio.** Use `[BROWSER: command]` directives in your response to control it. Do NOT use Playwright or Python for browser tasks — the built-in webview is always available and requires no external setup.

## Browser Commands

Include any of these in your assistant response and they will be executed automatically:

| Directive | What it does |
|---|---|
| `[BROWSER: navigate https://example.com]` | Opens the browser tab and navigates to the URL |
| `[BROWSER: getContent]` | Returns the page title, URL and text content (up to 4000 chars) |
| `[BROWSER: getHTML]` | Returns the raw outer HTML of the page (up to 8000 chars) |
| `[BROWSER: getElement h1]` | Returns `outerHTML` of the first element matching the CSS selector |
| `[BROWSER: consoleLogs]` | Returns all browser console messages captured since the tab opened |
| `[BROWSER: screenshot]` | Takes a screenshot of the current page |

## Example Interactions

**User:** "Open GitHub and show me the h1 heading"
```
I'll open GitHub for you.
[BROWSER: navigate https://github.com]
[BROWSER: getElement h1]
```

**User:** "Check the console for errors on my localhost app"
```
[BROWSER: navigate http://localhost:3000]
[BROWSER: consoleLogs]
```

**User:** "What's on the current page?"
```
[BROWSER: getContent]
```

## Best Practices

1. **Always navigate first** before calling `getContent` / `getElement` / `consoleLogs` on a new URL.
2. **Wait for content**: If you need console logs after page load, navigate first, then request logs in a follow-up.
3. **CSS selectors**: Use standard CSS selectors for `getElement` — e.g. `h1`, `.main-content`, `#nav`, `article p`.
4. **Chain commands**: You can put multiple `[BROWSER: ...]` directives in a single response — they execute in order.
