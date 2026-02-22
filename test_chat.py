import socket
import time

print("Connecting to Neural AI Chat Port for advanced training...")
s = socket.socket()
try:
    s.connect(('127.0.0.1', 9000))
    # Read welcome message
    print(s.recv(2048).decode())

    def interact(msg):
        print(f"User: {msg}")
        s.send((msg + '\n').encode())
        time.sleep(0.5)
        response = s.recv(2048).decode()
        print(f"AI: {response}")

    # Start training session
    interact("learn The CMIX neural compression algorithm uses a massive ensemble of context models combined by an LSTM network.")
    interact("learn BWT stands for Burrows-Wheeler Transform, which rearranges data into runs of similar characters to improve compressibility.")
    interact("learn PPM or Prediction by Partial Matching is an adaptive statistical data compression technique based on context modeling.")
    interact("learn My local server exposes a neural backend capable of analyzing file entropy and recommending the best compression parameters.")
    interact("learn The chat port operates on TCP 9000 and is directly integrated with the C++ Smart Brain engine.")
    interact("learn The Smart Brain uses a TF-IDF vectorizer and cosine similarity to retrieve knowledge items from compressed vaults.")
    interact("learn Context mixing dynamically assigns weights to multiple independent compression models, favoring the ones with the highest accuracy history.")
    
    interact("exit")
    
    time.sleep(3)
    print("Advanced training session completed.")
    
except Exception as e:
    print(f"Connection failed: {e}")
