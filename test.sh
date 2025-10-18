config_test="./config_test.txt"
node_program="./node.cpp"
oracle_program="./Oracle.java"
oracle_ip="192.168.1.133"
oracle_port=5000

# Initial config
cat config2.txt > $config_test

num_nodes=0
read -p "Enter number of nodes: " num_nodes
echo "Number of nodes: $num_nodes" 
curr_nodes=0

# Oracle: 
javac $oracle_program
java Oracle $config_test > oracle_out.txt &

# Nodes
g++ $node_program -o node_test
rm -v node_*_out.txt

# Initially 2 nodes
while [[ curr_nodes -lt num_nodes ]]
do  
    curr_nodes=$(($curr_nodes + 1))
    echo $((5000 + $curr_nodes))
    sleep 1
    ./node_test $oracle_ip $(($oracle_port + $curr_nodes)) $oracle_ip $oracle_port > node_${curr_nodes}_out.txt &
done    

while true
do
    # Update config file

    echo "Update $config file in an editor"
    num_nodes=0
    read -p "Enter total number of nodes after update: " num_nodes
    echo "Number of nodes: $num_nodes" 

    while [[ curr_nodes -lt num_nodes ]]
    do  
        curr_nodes=$(($curr_nodes + 1))
        echo $((5000 + $curr_nodes))
        sleep 1
        ./node_test $oracle_ip $(($oracle_port + $curr_nodes)) $oracle_ip $oracle_port > node_${curr_nodes}_out.txt &
    done  

done
