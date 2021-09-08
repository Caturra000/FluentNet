import subprocess
import time

size = "16384" # per message
timeout = "5" # s
local = "127.0.0.1"
port = "2533"

for thread in [2, 4, 8]:
    for session in [10, 100, 1000]:
        print("thread:", thread, "session:", session)
        time.sleep(1)
        # server_cmd = ["./server", str(thread)]
        server_cmd = ["./server_multiplex", str(thread)]
        server_handle = subprocess.Popen(server_cmd)
        # client_cmd = ["./client", local, str(thread), str(session), size, timeout]
        client_cmd = ["./asio_client", local, port, str(thread), size, str(session), timeout]
        client_handle = subprocess.Popen(client_cmd)
        client_handle.wait()
        server_handle.kill()