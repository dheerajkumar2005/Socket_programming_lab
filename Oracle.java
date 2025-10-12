// Use a select call, Have a program loop
// based on output of select, perform action
// this is highest performing approach, compared to multi-process/thread

import java.net.Socket;
import java.net.ServerSocket;
import java.util.List;
import java.util.ArrayList;

class GraphNode {
    final List<int[]> adj_nodes;

    public GraphNode() {
        this.adj_nodes = new ArrayList<>();
    }

    public void addEdge(int v, int cost) {
        adj_nodes.add(new int[]{v, cost});
        return;
    }
} 

class Graph {

    final List<GraphNode> adjList; 

    public Graph() {
        this.adjList = new ArrayList<>();
    }

    public void addEdge(int v1, int v2, int cost) {

        if (cost < 0) {
            return;
        }

        while (this.adjList.size() < Math.max(v1, v2) + 1) {
            this.adjList.add(new GraphNode());
        }
        this.adjList.get(v1).addEdge(v2, cost);
        this.adjList.get(v2).addEdge(v1, cost);
    }

    public int numNodes() {
        return this.adjList.size();
    }

}


class Connection {
    char nodeAlphabet;
    int port;
    java.net.InetAddress ipAddress;
    java.io.BufferedReader in;
    java.io.PrintWriter out;

    public Connection(char nodeAlphabet, 
                    int port, 
                    java.net.InetAddress ipAddress, 
                    java.io.BufferedReader in, 
                    java.io.PrintWriter out) {
            
        this.nodeAlphabet = nodeAlphabet;
        this.port = port;
        this.ipAddress = ipAddress;
        this.in = in;
        this.out = out;
    }

}


public class Oracle {

    String config_file;
    int port = 5000;
    Graph topology;
    ServerSocket tcpSocket;
    List<Connection> virtualNodes;
    char nextAlphabet = 'A';

    public Oracle(String config_file) {
        this.config_file = config_file;

        this.topology = this.parseConfigFile(this.config_file);

        // open TCP socket
        try {
            this.tcpSocket = new ServerSocket(this.port);
            System.out.println("Server listening on port " + this.port);
            try {
                java.net.InetAddress bound = this.tcpSocket.getInetAddress();
                String ip = (bound == null || bound.isAnyLocalAddress())
                        ? java.net.InetAddress.getLocalHost().getHostAddress()
                        : bound.getHostAddress();
                System.out.println("Server bound to IP: " + ip + " Port: " + this.tcpSocket.getLocalPort());
            } catch (java.net.UnknownHostException e) {
                System.out.println("Server bound to Port: " + this.tcpSocket.getLocalPort() + " (local host unknown)");
            }
        } catch (java.io.IOException e) {
            System.err.println("I/O error: " + e.getMessage());
        }

        this.virtualNodes = new ArrayList<>();
        // listen for clients
        for (int i = 0; i < this.topology.numNodes(); i++) {
            try (Socket client = this.tcpSocket.accept();
                    java.io.BufferedReader in = new java.io.BufferedReader(new java.io.InputStreamReader(client.getInputStream()));
                    java.io.PrintWriter out = new java.io.PrintWriter(client.getOutputStream(), true)) {
                System.out.println("Client connected: " + client.getRemoteSocketAddress());
                int clientPort = client.getPort();
                java.net.InetAddress clientIPAddress = client.getInetAddress();
                String received = in.readLine();
                System.out.println("Received: " + received);
                try { Thread.sleep(3000); } catch (InterruptedException ignored) {}
                out.println("Message received");
                System.out.println("Response sent, closing connection");

                this.virtualNodes.add(new Connection(this.nextAlphabet, clientPort, clientIPAddress, in, out));
                this.nextAlphabet++;
            } catch (java.io.IOException e) {
                System.err.println("I/O error: " + e.getMessage());
            }
        }
    }

    public void run() {
        java.nio.file.Path path = java.nio.file.Paths.get(this.config_file);
        long lastModified = 0L;

        try {
        if (java.nio.file.Files.exists(path)) {
            lastModified = java.nio.file.Files.getLastModifiedTime(path).toMillis();
        }
        } catch (java.io.IOException e) {
        System.err.println("Could not read config timestamp: " + e.getMessage());
        }

        while (true) {
        try {
            if (java.nio.file.Files.exists(path)) {
            long lm = java.nio.file.Files.getLastModifiedTime(path).toMillis();
            if (lm != lastModified) {
                lastModified = lm;
                System.out.println("Config file changed, reloading...");
                updateGraph();
            }
            } else {
            // config file missing; optionally handle this case
            }
            Thread.sleep(1000); // poll interval
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            break;
        } catch (java.io.IOException e) {
            System.err.println("Error checking config file: " + e.getMessage());
        }
        }
    }

    private void updateGraph() {
        this.topology = parseConfigFile(config_file);
        this.sendMessages();
    }

    private Graph parseConfigFile(String config_file) {
        java.nio.file.Path path = java.nio.file.Paths.get(config_file);

        Graph graph = new Graph();

        try (java.io.BufferedReader br = java.nio.file.Files.newBufferedReader(path, java.nio.charset.StandardCharsets.UTF_8)) {
            String line;

            int v1 = 0;

            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                System.out.println(line);
                String[] values = line.split(" ");

                // Building graph
                int v2 = v1+1;
                for (int j = 0; j < values.length; j++) {
                    if (values[j] == "") {
                        continue;
                    }
                    int cost = Integer.parseInt(values[j]);
                    graph.addEdge(v1, v2, cost);
                    v2++;
                }
                v1++;

            }
        } catch (java.io.IOException e) {
            System.err.println("Error reading config file: " + e.getMessage());
        }
        return graph;
    }


    private void sendMessages() {

    }


    public static void main(String argv[]) {
        System.out.println("hi");

        if (argv.length < 1) {
            System.out.println("Usage: java Oracle <path to config file>");
        }
        String config_file = argv[0];

        Oracle oracle = new Oracle(config_file);
        oracle.run();
    }



}

