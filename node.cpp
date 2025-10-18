// #include <fstream>
// #include <cstring>
// #include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
// #include <netinet/in.h>
// #include <iostream>
// #include <unistd.h>
#include <bits/stdc++.h>
#include <chrono>

using namespace std;

struct Neighbour{
    char alphabet;
    uint16_t udp_port;
    uint32_t ip_addr_be;

    Neighbour(char alphabet, uint16_t port, char* ip_addr = NULL){
        this->alphabet = alphabet;
        if (port > 0) {
            this->udp_port = port;
            inet_pton(AF_INET,ip_addr,&ip_addr_be);
        }
    }

};

struct Message{
    char data[500];
    int len;
};

struct Node {
    const char* ip_addr;
    uint16_t udp_port;
    const char* on_ip_addr;
    uint16_t on_port;
    char NodeAlphabet;

    map<char, map<char, pair<int, shared_ptr<Neighbour>>>> adj;
    map<char,pair<int,char>> routing_table;

    unordered_map<char,shared_ptr<sockaddr_in>> udp_sockaddr_map;
    uint16_t seqno;
    unordered_map<char,uint16_t> last_recv_seqno;
    queue<shared_ptr<Message>> fwd_queue;
    char buffer[1024];
    int tcp_socket;
    int udp_socket;

    int MAX_NODES = 26;

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

        int flags2 = fcntl(udp_socket,F_GETFL,0);
        fcntl(udp_socket,F_SETFL,flags2|O_NONBLOCK);

        sockaddr_in own_ip_addr;
        own_ip_addr.sin_family = AF_INET;
        own_ip_addr.sin_port = htons(udp_port);
        if(inet_pton(AF_INET,ip_addr,&own_ip_addr.sin_addr) <= 0){
            cerr << "invalid on_ip addr\n";
            exit(1);
        }
        if(bind(tcp_socket,(sockaddr*)&own_ip_addr,sizeof(own_ip_addr)) < 0){
            cerr << "error in binding tcp_port\n";
            exit(1);
        }


        if(bind(udp_socket,(sockaddr*)&own_ip_addr,sizeof(own_ip_addr)) < 0){
            cerr << "error in binding udp_port\n";
            exit(1);
        }
        this->seqno = 1;


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
            cout << "sent 6 bytes to ON\n";
        }

    }


    void receive_from_ON() {
        // LINK_STATE messages
        int max_len = 11*MAX_NODES;
        char buffer[max_len];
        bzero(buffer, max_len);
        char* curr = buffer;

        int received_size = 0;
        while (received_size == 0 || received_size % 11 != 0) {
            int len = recv(tcp_socket, curr, max_len, 0);
            if (len < 0) {
                cerr << "Error reading from TCP socket" << endl;
                exit(1);
            }
            received_size += len;
            curr += len;
        }

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
                shared_ptr<Neighbour> vn = make_shared<Neighbour>(alphabet,port,address);
                adj[NodeAlphabet][alphabet] = {cost,vn};
                make_udp_sockaddr_map(vn);
            }
        }

    }

    void make_udp_sockaddr_map(shared_ptr<Neighbour> n){
        shared_ptr<sockaddr_in> tmp = make_shared<sockaddr_in>();
        tmp->sin_family = AF_INET;
        tmp->sin_port = n->udp_port;
        tmp->sin_addr.s_addr = n->ip_addr_be;
        udp_sockaddr_map[n->alphabet] = tmp;
    }

    void broadcast_lsp(){
        // cout << "broadcasting message" << endl;
        int max_size = 5*25+3;
        char buffer[max_size];
        bzero(buffer, max_size);
        memcpy(buffer, &NodeAlphabet,1);
        memcpy(buffer+1, &seqno,2);
        char* curr = buffer + 3;
        int len = 3;
        for(auto [c,p] : adj[NodeAlphabet]){
            // cout << c << endl;
            memcpy(curr,&c,1);
            memcpy(curr+1,&p.first,4);
            curr += 5;
            len+=5;
        }
        for(auto [c,s] : udp_sockaddr_map){
            int bytes = sendto(udp_socket, buffer, len, 0, (sockaddr*)&(*s), sizeof(sockaddr_in));
            if(bytes < 0){
                cerr << "Error in broadcasting udp packets\n";
                exit(1);
            }
        }
        seqno++;
    }


    void recv_lsp(){
        // cout << "receiving message" << endl;
        while(true){
            char buffer[500];
            ssize_t n = recvfrom(udp_socket,buffer,sizeof(buffer),0,NULL,NULL);
            if(n < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    break;
                }
                else{
                    cerr << "error in recvfrom\n";
                    exit(1);
                }
            }
            // cout << n << endl;

            char* curr = buffer;
            char origin_alphabet = *curr;
            curr+=1;
            if(origin_alphabet == NodeAlphabet) {
                continue;
            }

            uint16_t recv_seqno = *((uint16_t*)curr);
            curr+=2;
            if(recv_seqno <= last_recv_seqno[origin_alphabet]) {
                continue;
            }
            else {
                last_recv_seqno[origin_alphabet]++;
            }

            shared_ptr<Message> m = make_shared<Message>();
            memcpy(m->data, &buffer, n);
            // cout << (char)m->data[0] << endl;
            m->len = n;
            fwd_queue.push(m);
            for(int i=0; i<(n-3)/5; i++){
                char a = *curr;
                // cout << origin_alphabet << " " << a << endl;
                curr+=1;
                int cost = *((int*)(curr));
                curr+=4;
                shared_ptr<Neighbour> nei = make_shared<Neighbour>(a, 0);
                adj[origin_alphabet][a] = {cost,nei};
            }
        }
        print_network();
        update_routing_table();
    }

    void forward_lsp(){
        if (fwd_queue.size() > 0) {
            // cout << "Forwarding " << fwd_queue.size() << " messages" << endl;
        }
        while(!fwd_queue.empty()){
            shared_ptr<Message> m = fwd_queue.front();
            // cout << (char)m->data[0] << endl;
            fwd_queue.pop();
            for(auto [c,s] : udp_sockaddr_map){
                int bytes = sendto(udp_socket, m->data, m->len, 0, (sockaddr*)&(*s), sizeof(sockaddr_in));
                // cout << "bytes sent to " << c << " : " << bytes << '\n'; 
                if(bytes < 0){
                    cerr << "Error in forwarding packets\n";
                    exit(1);
                }
            }
        }
    }

    void print_network(){
        cout << "This node: " << NodeAlphabet << '\n';
        for(auto [c1, u]: adj){
            cout << "Node " << c1 << " : ";
            for(auto [c2, p]: adj[c1]){
                cout << c2 << " = " << p.first << " ";
            }
            cout << '\n';
        }
    }

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

        cout << "Updated routing table:" << endl;
        for (auto [c, p] : routing_table) {
            if(c == NodeAlphabet) continue;
            cout << c << " " << p.second << " " << p.first << '\n';    
        }

    }


    /* Reference: https://www.geeksforgeeks.org/computer-networks/tcp-and-udp-server-using-select/ */
    void run() {
        
        int poll_interval = 100; // in milliseconds
        int broadcast_interval = 10000; // in milliseco
        int last_broadcast = 0;

        fd_set rset, wset;
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        int maxfdp1 = max(tcp_socket, udp_socket) + 1;

        while (true) {
            FD_SET(tcp_socket, &rset); 
            FD_SET(udp_socket, &rset); 
            FD_SET(udp_socket, &wset); 
            int nready = select(maxfdp1, &rset, &wset, NULL, NULL); 
            
            // tcp connection
            if (FD_ISSET(tcp_socket, &rset)) {
                receive_from_ON();
            }
            else if (FD_ISSET(udp_socket, &rset)) {
                // read udp packets, update graph, update queue of packets to be forwarded
                recv_lsp();                
            }
            else if (FD_ISSET(udp_socket, &wset)) {
                // send the LSP packets and forwarded packets
                if (last_broadcast > broadcast_interval) {
                    broadcast_lsp();
                    last_broadcast = 0;
                }
                forward_lsp();
            }
            last_broadcast += poll_interval;
            usleep(poll_interval * 1000);

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
    uint16_t on_port = atoi(argv[4]);

    Node* vn = new Node(vn_ip,vn_udp_port,on_ip,on_port);
    vn->connect_to_ON();
    vn->receive_from_ON();
    vn->run();
}