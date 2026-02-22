import socket
import time
import json

facts = [
    "Machine learning involves training models on data.",
    "Data compression reduces the number of bits needed to represent data.",
    "Huffman coding is a lossless data compression algorithm.",
    "Lempel-Ziv is a dictionary-based compression method.",
    "Neural networks are inspired by the biological brain.",
    "Deep learning uses neural networks with many layers.",
    "Lossless compression allows exact original data reconstruction.",
    "Lossy compression loses some information for smaller sizes.",
    "BWT enables reversible permutation of characters.",
    "Move-to-front transform is often used with BWT.",
    "Run-length encoding compresses sequences of identical data.",
    "Entropy measures the unpredictability of information.",
    "Shannon limit is the maximum data compression possible losslessly.",
    "CMIX combines hundreds of context models.",
    "Context mixing averages probabilities from different models.",
    "An LSTM can remember long-term dependencies.",
    "Transformers rely entirely on self-attention mechanisms.",
    "A bit is the basic unit of information in computing.",
    "A byte consists of 8 bits.",
    "ASCII uses 7 bits to encode standard text characters.",
    "UTF-8 is a variable-width character encoding for Unicode.",
    "An algorithm is a finite sequence of rigorous instructions.",
    "Big O notation describes time or space complexity.",
    "QuickSort has an average time complexity of O(n log n)",
    "MergeSort is a stable sorting algorithm.",
    "Dynamic programming solves complex problems by breaking them down.",
    "Recursion is when a function calls itself.",
    "A hash table maps keys to values for efficient lookup.",
    "Binary search trees keep their keys in sorted order.",
    "Graph theory studies pairwise relationships between objects.",
    "Dijkstra's algorithm finds the shortest path in a graph.",
    "Turing machines were invented by Alan Turing in 1936.",
    "A compiler translates source code into machine code.",
    "An interpreter executes instructions directly.",
    "Operating systems manage computer hardware and software resources.",
    "A kernel is the core component of an operating system.",
    "Virtual memory allows OS to use disk space as RAM.",
    "Multithreading allows multiple threads to exist within one process.",
    "Deadlock happens when processes wait on each other indefinitely.",
    "Mutexes prevent race conditions in concurrent programming.",
    "TCP provides reliable, ordered transmission of packets.",
    "UDP is connectionless and does not guarantee delivery.",
    "IP addresses identify devices on a network.",
    "DNS translates domain names to IP addresses.",
    "HTTP is the foundation of data communication for the Web.",
    "HTTPS uses TLS encryption to secure communication.",
    "REST uses standard HTTP methods like GET and POST.",
    "GraphQL allows clients to request exactly what they need.",
    "Relational databases use SQL for querying data.",
    "NoSQL databases handle unstructured or semi-structured data.",
    "A transaction must be Atomic, Consistent, Isolated, and Durable (ACID)",
    "Indexing speeds up data retrieval in databases.",
    "Git is a widely used distributed version control system.",
    "Linux is an open-source operating system kernel.",
    "Containers package applications with their dependencies.",
    "Docker simplifies application containerization.",
    "Kubernetes orchestrates container deployments.",
    "CI/CD automates software integration and deployment.",
    "Python is an interpreted, high-level programming language.",
    "C++ supports both procedural and object-oriented programming.",
    "JavaScript runs primarily in web browsers.",
    "TypeScript adds static typing to JavaScript.",
    "Rust emphasizes memory safety without garbage collection.",
    "Go was designed by Google for scalable backend systems.",
    "Java compiles to platform-independent bytecode.",
    "Public-key cryptography uses a pair of asymmetric keys.",
    "RSA is a widely used public-key cryptosystem.",
    "SHA-256 generates a 256-bit hash value.",
    "Symmetric encryption uses the same key for encryption and decryption.",
    "Blockchain is a decentralized and distributed digital ledger.",
    "Cloud computing provides on-demand computing resources.",
    "AWS, Azure, and Google Cloud are major cloud providers.",
    "Serverless computing dynamically manages machine resources.",
    "An API allows different software programs to communicate.",
    "Webhooks push real-time data to other applications.",
    "JSON is a lightweight data-interchange format.",
    "XML utilizes tags to structure data.",
    "Object-oriented programming bundles data and behavior.",
    "Inheritance allows a class to derive properties of another.",
    "Polymorphism allows treating different objects via the same interface.",
    "Encapsulation hides internal details of an object.",
    "Functional programming emphasizes pure functions and immutability.",
    "State machines model systems through a set of states and transitions.",
    "Agile methodology promotes iterative software development.",
    "Scrum relies on short development cycles called sprints.",
    "Test-driven development requires writing tests before code.",
    "Unit testing validates individual components of software.",
    "Integration testing ensures multiple components work together.",
    "Refactoring improves code structure without changing behavior.",
    "Technical debt occurs when prioritizing speed over code quality.",
    "Microservices architecture structures apps as loosely coupled services.",
    "Monolithic architecture bundles all components into one executable.",
    "A load balancer distributes network traffic across servers.",
    "Caching stores frequently accessed data for fast retrieval.",
    "Redis is a fast, in-memory key-value data store.",
    "Distributed systems coordinate actions via messaging.",
    "CAP theorem states you can only have two of Consistency, Availability, and Partition tolerance.",
    "A firewall monitors and controls network traffic.",
    "Penetration testing identifies vulnerabilities by attacking systems.",
    "Zero-day vulnerabilities are unknown to software vendors."
]

print("Connecting to Neural AI Chat Port for 100-fact training...")
s = socket.socket()
try:
    s.connect(('127.0.0.1', 9000))
    print(s.recv(2048).decode())

    for idx, fact in enumerate(facts):
        msg = f"learn {fact}"
        s.send((msg + '\n').encode())
        # Wait a very small amount of time to ensure it is processed
        time.sleep(0.05)
        # We don't need to print every response to avoid flooding the terminal
        response = s.recv(2048).decode()
        if idx % 10 == 0:
            print(f"Sent {idx + 1}/100 facts...")
            
    s.send(b"exit\n")
    time.sleep(3)
    print("Advanced 100-fact training session completed.")
    
except Exception as e:
    print(f"Connection failed: {e}")
