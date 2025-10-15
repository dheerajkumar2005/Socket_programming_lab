// #include <fstream>
// #include <cstring>
// #include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
// #include <netinet/in.h>
// #include <iostream>
// #include <unistd.h>
#include <bits/stdc++.h>

using namespace std;


struct network_graph{
    vector<int> costs;
};

struct Node {
    const char* ip_addr;
    uint16_t udp_port;
    const char* on_ip_addr;
    uint16_t on_port;
    char NodeAlphabet;

    int tcp_socket;
    int udp_socket;

    Node(const char* ip, uint16_t port, const char* on_ip, uint16_t on_port){
        this->ip_addr = ip;
        this->udp_port = port;
        this->on_ip_addr = on_ip;
        this->on_port = on_port;

        tcp_socket = socket(AF_INET,SOCK_STREAM,0);
        if(tcp_socket < 0){
            cerr << "tcp_socket creation failed\n";
            exit(1);
        }
        udp_socket = socket(AF_INET,SOCK_DGRAM,0);
        if(udp_socket < 0){
            cerr << "udp socket creation failed\n";
            exit(1);
        }
    }

    void connect_to_ON() {
        sockaddr_in on_addr;
        memset(&on_addr,0,sizeof(on_addr));
        on_addr.sin_family = AF_INET;
        on_addr.sin_port = htons(on_port);
        
        if(inet_pton(AF_INET,on_ip_addr,&on_addr.sin_addr) <= 0){
            cerr << "invalid on_ip addr\n";
            exit(1);
        }
        if(connect(tcp_socket,(struct sockaddr*)&on_addr,sizeof(on_addr))){
            cerr << "connection to Oracle Node failed\n";
            exit(1);
        };
        char* message;
        message = new char[6];
        bzero(message,6);
        uint32_t ip_addr_be;
        if(inet_pton(AF_INET,ip_addr,&ip_addr_be) <= 0){
            cerr << "invalid virtual node ip addr\n";
            exit(1);
        }
        memcpy(message,&ip_addr_be,4);
        memcpy(message+4,&port,2);
        if(send(tcp_socket,message,4,0) == -1){
            cerr << "Error in sending the CONNECT message\n";
            exit(1);
        }
    }

};




int main(int argc, char* argv[]){
    if(argc < 5){
        cout << "Incorrect usage";
        exit(1);
    }
    const char* vn_ip = argv[1];
    uint16_t vn_udp_port = stoi(argv[2]);
    const char* on_ip = argv[3];
    uint16_t on_port = 5050;

    Node* vn = new Node(vn_ip,vn_udp_port,on_ip,on_port);


}