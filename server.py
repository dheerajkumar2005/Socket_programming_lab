from socket import INADDR_ANY, socket, AF_INET, SOCK_STREAM

s = socket(AF_INET, SOCK_STREAM)
s.bind(('0.0.0.0', 8080))
s.listen(5)
server_addr = s.getsockname()
print(server_addr)
while True:
    client_socket, client_addr = s.accept()
    print(f"Connection from {client_addr} has been established.")
    data = client_socket.recv(1024).decode('utf-8')
    print(f"Received: {data}")
    client_socket.send(b"Hi from server!")

    client_socket.close()
    break

