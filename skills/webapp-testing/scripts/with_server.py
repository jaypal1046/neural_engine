import subprocess
import time
import socket
import sys
import argparse
import os

def is_port_open(port, host='localhost'):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex((host, port)) == 0

def run_with_server(server_cmd, test_cmd, port, timeout=30):
    print(f"🚀 Starting server: {server_cmd}")
    server_process = subprocess.Popen(server_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    print(f"⌛ Waiting for port {port} to open...")
    start_time = time.time()
    while time.time() - start_time < timeout:
        if is_port_open(port):
            print(f"✅ Port {port} is open!")
            break
        time.sleep(1)
    else:
        print(f"❌ Timeout waiting for port {port} to open.")
        server_process.terminate()
        return 1

    print(f"🧪 Running test command: {test_cmd}")
    test_process = subprocess.run(test_cmd, shell=True)

    print("🛑 Shutting down server...")
    server_process.terminate()
    return test_process.returncode

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run a test command while a server is running.")
    parser.add_argument("--server", required=True, help="Command to start the server")
    parser.add_argument("--test", required=True, help="Command to run the test")
    parser.add_argument("--port", required=True, type=int, help="Port the server listens on")
    parser.add_argument("--timeout", type=int, default=30, help="Timeout in seconds to wait for the port")

    args = parser.parse_args()
    sys.exit(run_with_server(args.server, args.test, args.port, args.timeout))
