// Use a select call, Have a program loop
// based on output of select, perform action
// this is highest performing approach, compared to multi-process/thread

import java.net.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

class GraphNode {
    final List<int[]> adjNodes;

    public GraphNode() {
        this.adjNodes = new ArrayList<>();
    }

    public void addEdge(int v, int cost) {
        adjNodes.add(new int[]{v, cost});
    }
} 

class Graph {

    final List<GraphNode> adjList; 

    public Graph() {
        this.adjList = new ArrayList<>();
    }

    public void addEdge(int v1, int v2, int cost) {

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
    int nodeIndex;
    byte nodeAlphabet;
    byte[] port;
    InetAddress ipAddress;

    byte[] udpPort;
    InetAddress udpAddress;

    public Connection(
                        int nodeIndex,
                        byte nodeAlphabet, 
                        byte[] port, 
                        InetAddress ipAddress, 
                        byte[] connect
                    ) {
            
        this.nodeIndex = nodeIndex;
        this.nodeAlphabet = nodeAlphabet;

        this.port = java.util.Arrays.copyOf(port, 2);
        this.ipAddress = ipAddress;

        // parsing CONNECT message
        // first 4 bytes - udp ip address, next 2 bytes - udpPort  
        byte[] udpAddress = java.util.Arrays.copyOfRange(connect, 0, 4);
        this.udpPort = java.util.Arrays.copyOfRange(connect, 4, 6);
        try {
            this.udpAddress = InetAddress.getByAddress(udpAddress);
        } catch (UnknownHostException e) {
            System.out.println("Error in converting " + udpAddress + " to inetAddress");
        }    

        int portInt = ByteBuffer.wrap(this.port)
            .order(ByteOrder.BIG_ENDIAN)
            .getShort() & 0xFFFF;
        System.out.println("Node " + this.nodeAlphabet + " TCP details: " + this.ipAddress + ":" + portInt);

        int udpPortInt = ByteBuffer.wrap(this.udpPort)
                .order(ByteOrder.BIG_ENDIAN)
                .getShort() & 0xFFFF;

        System.out.println("Node " + this.nodeAlphabet + " UDP details: " + this.udpAddress + ":" + udpPortInt);
    }
}


public class Oracle {

    String config_file;
    Graph topology;

    int port = 5000;
    ServerSocket tcpSocket;

    List<Connection> virtualNodes;
    static List<Socket> vnSockets = new ArrayList<>();

    byte nextAlphabet = 'A';

    public Oracle(String config_file) {
        System.out.println(this.nextAlphabet);
        this.config_file = config_file;

        this.topology = this.parseConfigFile(this.config_file);

        // open TCP socket
        try {
            this.tcpSocket = new ServerSocket(this.port);
            try {
                InetAddress bound = this.tcpSocket.getInetAddress();
                String ip = (bound == null || bound.isAnyLocalAddress())
                        ? InetAddress.getLocalHost().getHostAddress()
                        : bound.getHostAddress();
                System.out.println("Oracle bound to IP: " + ip);
                System.out.println("Oracle bound to Port: " + this.tcpSocket.getLocalPort());
            } catch (UnknownHostException e) {
                System.out.println("Oracle bound to Port: " + this.tcpSocket.getLocalPort() + " (local host unknown)");
            }
        } catch (java.io.IOException e) {
            System.err.println("I/O error: " + e.getMessage());
        }

        this.connectToVNs();

        for (int i = 0; i < vnSockets.size(); i++) {
            System.out.println(vnSockets.get(i).getInetAddress());
        }

        this.sendMessages();

    }

    void connectToVNs() {
        // listen for VNs
        this.virtualNodes = new ArrayList<>();

        for (int i = 0; i < this.topology.numNodes(); i++) {
            try {
                Socket vn = this.tcpSocket.accept();
            
                // VN tcp connection details - ip and port
                vnSockets.add(vn);
                int vnPort = vn.getPort();
                InetAddress vnIPAddress = vn.getInetAddress();
                
                byte[] vnPortBytes = ByteBuffer.allocate(2)   // 2 bytes for a 16-bit value
                    .order(ByteOrder.BIG_ENDIAN)    // network byte order
                    .putShort((short) vnPort)       // cast to short
                    .array();

                // read CONNECT message
                byte[] received = new byte[6];
                int total = 0;
                java.io.InputStream is = vn.getInputStream();
                while (total < received.length) {
                    int n = is.read(received, total, received.length - total);
                    if (n < 0) {
                        break;
                    }
                    total += n;
                }

                // Create connection record
                this.virtualNodes.add(new Connection(
                                                        i,
                                                        this.nextAlphabet, 
                                                        vnPortBytes, 
                                                        vnIPAddress, 
                                                        received
                                                    ));
                this.nextAlphabet++;

                System.out.println(received + " " + received.length);
                // vn.getOutputStream().write(received);
            
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
                for (String value : values) {
                    if (value == "") {
                        continue;
                    }
                    int cost = Integer.parseInt(value);
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
        for (int i = 0; i < this.topology.numNodes(); i++) {
            GraphNode n = this.topology.adjList.get(i);
            Connection conn = this.virtualNodes.get(i);
            Socket vn = vnSockets.get(i);

            int msgLength = 11 * (1 + n.adjNodes.size());
            byte[] msg = new byte[msgLength];

            int offset = 0;
            msg[offset] = conn.nodeAlphabet;
            offset += 1;
            // getAddress() gives network byte order inherently
            System.arraycopy(conn.udpAddress.getAddress(), 0, msg, offset, 4);
            offset += 4;
            System.arraycopy(conn.udpPort, 0, msg, offset, 2);
            offset += 2;
            int cost = 0;
            byte[] costBytes = ByteBuffer.allocate(4)
                    .order(ByteOrder.BIG_ENDIAN)  // network byte order
                    .putInt(cost)
                    .array();
            System.arraycopy(costBytes, 0, msg, offset, 4);
            offset += 4;

            for (int j = 0; j < n.adjNodes.size(); j++) {
                conn = this.virtualNodes.get(n.adjNodes.get(j)[0]);
                cost = n.adjNodes.get(j)[1];
                msg[offset] = conn.nodeAlphabet;
                offset += 1;
                // getAddress() gives network byte order inherently
                System.arraycopy(conn.udpAddress.getAddress(), 0, msg, offset, 4);
                offset += 4;
                System.arraycopy(conn.udpPort, 0, msg, offset, 2);
                offset += 2;
                costBytes = ByteBuffer.allocate(4)
                        .order(ByteOrder.BIG_ENDIAN)  // network byte order
                        .putInt(cost)
                        .array();
                System.arraycopy(costBytes, 0, msg, offset, 4);
                offset += 4;
            }

            System.out.println(msg);
            System.out.println(vn.getInetAddress());
            try {
                vn.getOutputStream().write(msg);
            } catch (java.io.IOException e) {
              System.err.println("Error sending LINK-STATE messages: " + e.getMessage());
            }
        }
    }


    public static void main(String argv[]) {
        if (argv.length < 1) {
            System.out.println("Usage: java Oracle <path to config file>");
        }
        String config_file = argv[0];

        Oracle oracle = new Oracle(config_file);
        // oracle.run();
    }



}

