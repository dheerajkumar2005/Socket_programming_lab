#include <fstream>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <errno.h>
#include <bits/stdc++.h>

using namespace std;

struct Neighbour{
    char alphabet;
    uint16_t udp_port;
    uint32_t ip_addr_be;

    Neighbour(char alphabet, uint16_t port, char* ip_addr){
        this->alphabet = alphabet;
        this->udp_port = port;
        inet_pton(AF_INET,ip_addr,&ip_addr_be);
    }
};

struct Node {
    const char* ip_addr;
    uint16_t udp_port;
    const char* on_ip_addr;
    uint16_t on_port;
    char NodeAlphabet;

    unordered_map<char,unordered_map<char,pair<int,Neighbour*>>> adj;
    unordered_map<char,pair<int,char>> routing_table;

    unordered_map<char,sockaddr_in> udp_sockaddr_map;
    uint16_t seqno;
    unordered_map<char,uint16_t> last_recv_seqno;
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
        // int flags1 = fcntl(tcp_socket,F_GETFL,0);
        // fcntl(udp_socket,F_SETFL,flags1|O_NONBLOCK);

        int flags2 = fcntl(udp_socket,F_GETFL,0);
        fcntl(udp_socket,F_SETFL,flags2|O_NONBLOCK);

        sockaddr_in own_ip_addr;
        own_ip_addr.sin_family = AF_INET;
        own_ip_addr.sin_port = htons(udp_port);
        if(inet_pton(AF_INET,ip_addr,&own_ip_addr.sin_addr) <= 0){
            cerr << "invalid on_ip addr\n";
            exit(1);
        }

        if(bind(udp_socket,(sockaddr*)&own_ip_addr,sizeof(own_ip_addr)) < 0){
            cerr << "error in binding udp_port\n";
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
        int res = connect(tcp_socket,(struct sockaddr*)&on_addr,sizeof(on_addr));
        if(res < 0 and errno == EINPROGRESS){
            cerr << "connection to Oracle Node failed\n";
            close(tcp_socket);
            exit(1);
        }
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(tcp_socket,&writefds);

        int ready = select(tcp_socket+1, NULL, &writefds, NULL, NULL);
        if(ready < 0){
            cerr << "Error with select syscall\n";
            exit(1);
        }
        if(FD_ISSET(tcp_socket, &writefds)){
            int err;
            socklen_t len = sizeof(err);
            getsockopt(tcp_socket,SOL_SOCKET,SO_ERROR,&err,&len);
            if(err!=0){
                cerr << "Connection failed: " << strerror(err) << '\n';
                exit(1);
            }
            cout << "Successfully connected to Oracle Node\n";
        }

        char message[6];
        // unique_ptr<char[]> message = make_unique<char[]>(6);
        bzero(message,6);
        uint32_t ip_addr_be;
        if(inet_pton(AF_INET,ip_addr,&ip_addr_be) <= 0){
            cerr << "invalid virtual node ip addr\n";
            exit(1);
        }
        uint16_t udp_port_be = htons(udp_port);
        memcpy(message,&ip_addr_be,4);
        memcpy(message+4,&udp_port_be,2);
        ssize_t send_output = send(tcp_socket,message,6,0);
        if(send_output == -1){
            cerr << "Error in sending the CONNECT message\n";
            exit(1);
        }
        else if(send_output != 6){
            cerr << "Supposed to send 6 bytes but " << send_output << " bytes sent\n";
            exit(1); 
        }
        else{
            cout << "send 6 bytes to ON\n";
        }
    }

    void receive_from_ON() {
        // LINK_STATE messages
        int max_len = 11*26;
        char buffer[max_len];
        bzero(buffer, max_len);
        char* curr = buffer;
        int len = 0;
        while ((len = recv(tcp_socket, curr, max_len, 0)) <= 0) {
            if (len < 0) {
                cerr << "Error reading from TCP socket" << endl;
                exit(1);
            }
        }
        curr += len;

        while ((len = recv(tcp_socket, curr, max_len, 0)) > 0) {
            curr += len;
        }
        if (len < 0) {
            cerr << "Error reading from TCP socket" << endl;
            exit(1);
        }

        int received_size = (curr - buffer);
        cout << "Message size: " << received_size << endl;

        int num_messages = received_size / 11;
        curr = buffer;
        for (int i = 0; i < num_messages; i++) {
            char alphabet = *curr;
            curr += 1;

            char address[20];
            bzero(address, 20);
            inet_ntop(AF_INET, curr, address, 20);
            curr += 4;
            
            uint16_t port = (*((uint16_t*)curr));
            curr += 2;
            
            int cost = ntohl(*((int*)curr));
            curr += 4;

            cout << "Alphabet: " << alphabet << " UDP address: " << address << " UDP Port: " << port << " Edge cost: " << cost << endl;
            
            if (cost == 0) {
                this->NodeAlphabet = alphabet;
            }
            else {
                Neighbour* vn = new Neighbour(alphabet,port,address);
                adj[NodeAlphabet][alphabet] = {cost,vn};
            }
        }
    }

    void make_udp_sockaddr_map(){
        for(int i=0; i<26; i++){
            if(adj[NodeAlphabet-'A'][i].first != -1){
                sockaddr_in tmp;
                tmp.sin_family = AF_INET;
                tmp.sin_port = adj[NodeAlphabet-'A'][i].second->udp_port;
                tmp.sin_addr.s_addr = adj[NodeAlphabet-'A'][i].second->ip_addr_be;
                udp_sockaddr_map[adj[NodeAlphabet-'A'][i].second->alphabet] = tmp;
            }
        }
    }

    void broadcast_lsp(){
        int max_size = 5*25+3;
        char buffer[max_size];
        bzero(buffer,max_size);
        memcpy(buffer,&NodeAlphabet,1);
        memcpy(buffer+1,&seqno,2);
        char* curr = buffer+3;
        for(auto [c,p] : adj[NodeAlphabet]){
            memcpy(curr,&c,1);
            memcpy(curr+1,&p.first,4);
            curr += 4;
        }
        for(auto [c,s] : udp_sockaddr_map){
            sendto(udp_socket,buffer,max_size,0,(sockaddr*)&s,sizeof(s));
        }
        seqno++;
    }
    // void recv_lsp(){

    // }

    void update_routing_table(){
        int n = adj.size();
        char src = NodeAlphabet;
        unordered_map<char,int> dist;
        unordered_map<char,int> parent;
        unordered_map<char,bool> visited;

        for(auto [c,u] : adj){
            dist[c] = INT_MAX;
            parent[c] = -1;
            visited[c] = false;
        }

        priority_queue<pair<int, char>, vector<pair<int, char>>, greater<pair<int, char>>> pq;
        dist[src] = 0;
        pq.push({0,src});

        while(!pq.empty()){
            char u = pq.top().second;
            pq.pop();

            if(visited[u]) continue;
            visited[u] = true;

            for(auto [v, p] : adj[u]){
                int w = p.first;
                if(!visited[v] && dist[u] + w < dist[v]){
                    dist[v] = dist[u] + w;
                    parent[v] = u;
                    pq.push({dist[v],v});
                }
            }
        }

        for(auto [c,u]: adj){
            if(c == NodeAlphabet || dist[c] == INT_MAX) continue;
            char neighbour = c;
            while(parent[neighbour] != -1 && parent[neighbour] != src){
                neighbour = parent[neighbour];
            }
            if(parent[c] != -1){
                routing_table[c] = {dist[c],neighbour};
            }
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
    uint16_t on_port = 5000;

    Node* vn = new Node(vn_ip,vn_udp_port,on_ip,on_port);
    vn->connect_to_ON();
    vn->receive_from_ON();

}