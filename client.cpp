#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main(int argc, char* argv[]){
    if (argc < 3){
        cout << "Error: need to provide 2 command line args <ip_addr_of_server> <server_port>\n";
        exit(1);
    }
    
    int server_addr;
    server_addr = stoi(argv[1]);

    int server_port;
    server_port = stoi(argv[2]);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(server_port);
    serverAddress.sin_addr.s_addr = server_addr;

    int clientSocket = socket(AF_INET,SOCK_STREAM,0);
    
    connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

    const char* message = "Hello ...";
    send(clientSocket,message,strlen(message),0);

    char buffer[512];
    bzero((void*)buffer,512);

    recv(clientSocket,buffer,512,0);
    cout << "Received data from server: " << buffer << '\n';
    close(clientSocket);
}

