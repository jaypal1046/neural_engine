@echo off
echo ========================================
echo Testing Code Generation Routing
echo ========================================
echo.

echo [Test 1] Debug endpoint - Check keyword detection
echo ------------------------------------------------
curl -X POST http://127.0.0.1:8001/api/debug/code-detection ^
  -H "Content-Type: application/json" ^
  -d "{\"message\": \"Write a fibonacci function\", \"history\": [], \"web_search\": false}"
echo.
echo.

echo [Test 2] Actual chat endpoint - Code request
echo ------------------------------------------------
curl -X POST http://127.0.0.1:8001/api/chat ^
  -H "Content-Type: application/json" ^
  -d "{\"message\": \"Write a fibonacci function\", \"history\": [], \"web_search\": false}"
echo.
echo.

echo [Test 3] Actual chat endpoint - Non-code request
echo ------------------------------------------------
curl -X POST http://127.0.0.1:8001/api/chat ^
  -H "Content-Type: application/json" ^
  -d "{\"message\": \"What is compression?\", \"history\": [], \"web_search\": false}"
echo.
echo.

echo ========================================
echo Tests Complete
echo ========================================
pause
