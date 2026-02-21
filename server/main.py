import os
import subprocess
from fastapi import FastAPI, BackgroundTasks
from fastapi.responses import StreamingResponse
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
from pydantic import BaseModel

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class CompressRequest(BaseModel):
    file_path: str
    algorithm: str = "--cmix"

@app.get("/api/status")
def status():
    return {"status": "ok", "message": "Neural Studio Python API running."}

@app.post("/api/compress")
def compress_file(payload: CompressRequest):
    if not os.path.exists(payload.file_path):
        return {"error": f"File not found: {payload.file_path}"}
        
    target_myzip = payload.file_path + ".myzip"
    # Server is in c:\Jay\_Plugin\compress\server\
    # Executable is in c:\Jay\_Plugin\compress\bin\myzip.exe
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    exe = os.path.join(base_dir, "bin", "myzip.exe")
    
    if not os.path.exists(exe):
        return {"error": f"Executable not found: {exe}"}
        
    try:
        # We run the process and wait for the result
        result = subprocess.run([exe, "compress", payload.file_path, target_myzip, payload.algorithm], capture_output=True, text=True)
        return {
            "status": "success", 
            "output": target_myzip, 
            "message": f"Compression finished for {payload.file_path} into {target_myzip}.",
            "stdout": result.stdout,
            "stderr": result.stderr
        }
    except Exception as e:
        return {"error": str(e)}

class DecompressRequest(BaseModel):
    archive_path: str
    output_path: str

@app.post("/api/decompress")
def decompress_file(payload: DecompressRequest):
    if not os.path.exists(payload.archive_path):
        return {"error": f"Archive not found: {payload.archive_path}"}
        
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    exe = os.path.join(base_dir, "bin", "myzip.exe")
    
    if not os.path.exists(exe):
        return {"error": f"Executable not found: {exe}"}
        
    try:
        result = subprocess.run([exe, "decompress", payload.archive_path, payload.output_path], capture_output=True, text=True)
        return {
            "status": "success", 
            "output": payload.output_path, 
            "message": f"Decompression finished for {payload.archive_path}.",
            "stdout": result.stdout,
            "stderr": result.stderr
        }
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/compress_stream")
def compress_stream(payload: CompressRequest):
    if not os.path.exists(payload.file_path):
        return StreamingResponse(iter([b"Error: File not found"]), media_type="text/plain")
        
    target_myzip = payload.file_path + ".myzip"
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    exe = os.path.join(base_dir, "bin", "myzip.exe")

    def generator():
        process = subprocess.Popen([exe, "compress", payload.file_path, target_myzip, payload.algorithm], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        buf = b""
        while True:
            char = process.stdout.read(1)
            if not char:
                if buf: yield buf
                break
            buf += char
            if char in (b'\r', b'\n') or len(buf) >= 128:
                yield buf
                buf = b""
        process.wait()

    return StreamingResponse(generator(), media_type="text/event-stream", headers={"X-Content-Type-Options": "nosniff", "Cache-Control": "no-cache"})

@app.post("/api/decompress_stream")
def decompress_stream(payload: DecompressRequest):
    if not os.path.exists(payload.archive_path):
        return StreamingResponse(iter([b"Error: Archive not found"]), media_type="text/plain")
        
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    exe = os.path.join(base_dir, "bin", "myzip.exe")

    def generator():
        process = subprocess.Popen([exe, "decompress", payload.archive_path, payload.output_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        buf = b""
        while True:
            char = process.stdout.read(1)
            if not char:
                if buf: yield buf
                break
            buf += char
            if char in (b'\r', b'\n') or len(buf) >= 128:
                yield buf
                buf = b""
        process.wait()

    return StreamingResponse(generator(), media_type="text/event-stream", headers={"X-Content-Type-Options": "nosniff", "Cache-Control": "no-cache"})

class CommandRequest(BaseModel):
    command: str

@app.post("/api/cmd")
def run_command(payload: CommandRequest):
    try:
        # Warning: Direct command execution endpoint. 
        # Only use in local dev environment.
        result = subprocess.run(payload.command, shell=True, capture_output=True, text=True)
        return {
            "status": "success",
            "stdout": result.stdout,
            "stderr": result.stderr,
            "code": result.returncode
        }
    except Exception as e:
        return {"error": str(e)}

if __name__ == "__main__":
    uvicorn.run("main:app", host="127.0.0.1", port=8001, reload=True)
