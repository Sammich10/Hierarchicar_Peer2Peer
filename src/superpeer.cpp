#include "dependencies.h"
#include "assist/message_utils.hpp"
#include "assist/message_structs.hpp"
#include "assist/queues.hpp"

#define PORT 8059
#define BUFSIZE 1024
#define SERVER_BACKLOG 100
#define CONFIG_FILE_PATH "./test/superpeerconfigtest.cfg"
#define THREAD_POOL_SIZE 15
#define MAX_CONNECTIONS 25

std::string super_peer_id;
std::string ip = "127.0.0.1";
int port = PORT;
int accepted_connections = 0;

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;
std::thread thread_pool[THREAD_POOL_SIZE];

std::vector<peer_info> peers;//vector to store registered peers
std::vector<super_peer_info> super_peers;//vector to store registered super peers
std::map<std::string, std::string> message_id_map;

std::mutex mtx;
std::condition_variable cv;

SocketQueue socket_queue;
QueryMessageQueue qm_queue;
QueryHitMessageQueue qhm_queue;


/*** Helper functions for super peer ***/

bool loadConfig(const char filepath[]){//function to load configuration parameters from config file
	char real_path[PATH_MAX+1];
	realpath(filepath,real_path);
	std::ifstream in(real_path);
	if(!in.is_open()){
		std::cout << "config file could not be opened\n";
		return false;
	}
	std::string param;
	std::string value;

	while(!in.eof()){
		in >> param;
		in >> value;

        if(param == "IP"){
            ip = value;
        }
        else if(param == "PEER_ID"){
            super_peer_id = value;
        }

		else if(param == "PORT"){
			port = stoi(value);
		}
        else if(param == "NEIGHBOR"){
            super_peer_info s;
            strcpy(s.ip,value.substr(0,value.find(":")).c_str());
            s.port = stoi(value.substr(value.find(":")+1,value.length()));
            super_peers.push_back(s);
        }
		
	}
	in.close();

	return true;
}

void check(int n, const char *msg){
    std::string err = super_peer_id + ":" + msg;
	if(n<0){
		perror(err.c_str());
	}
}
/*****************************************************************************************************************/

/*** Socket functions for super peer ***/

int sconnect(char IP[], int port){//connect to super-peer or another peer node and return the socket
    int sock = 0, client_fd;
    struct sockaddr_in server_addr;

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("error creating socket\n");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, IP, &server_addr.sin_addr) <=0){
        printf("\nInvalid address or address not supported\n");
        return -1;
    }

    if((client_fd = connect(sock, (SA*)&server_addr, sizeof(server_addr))) < 0){
        //printf("Connection to server on port %d failed!\n",port);
        return -1;
    }

    //printf("Connection to server on port %d successful\n",port);
    return sock;
}

/*** Core functions for super peer ***/

int update_all_peer_files(int client_socket, struct sockaddr_in client_addr){//function to update all peer files
	MessageHeader h;
	peer_info p;
	
	int peer_port;
	int num_files_int;

	recv(client_socket, &h, sizeof(h), 0); //receive message header
	recv(client_socket, &peer_port, h.length, 0); //receive peer port
	recv(client_socket, &num_files_int, h.length, 0); //receive number of files

	strcpy(p.ip,inet_ntoa(client_addr.sin_addr));
	p.port = peer_port;

	if(peers.size() < 1){
		printf("Peer %s:%d not registered\n",p.ip,p.port);
	}
	else{
		for(int i=0;(long unsigned int)i < peers.size();i++){
			if(strcmp(peers[i].ip,p.ip) == 0 && peers[i].port == p.port){
				peers[i].files.clear();
				for(int j=0;j<num_files_int;j++){
					file_info f;
					char file_name_buffer[BUFSIZE] = {0};
					recv(client_socket, &h, sizeof(h), 0); //receive message header
					recv(client_socket, &file_name_buffer, h.length, 0); //receive message body
					recv(client_socket, &f.file_size, sizeof(f.file_size), 0); //receive message body
					std::string file_name = file_name_buffer;
					f.file_name = file_name;
					peers[i].files.push_back(f);
				}
				printf("Peer %s:%d updated\n",p.ip,p.port);
                close(client_socket);
				return 0;
			}
		}
    close(client_socket);
	printf("Peer %s:%d not registered\n",p.ip,p.port);
	}
	return 0;
}

int update_peer_files(int client_socket, struct sockaddr_in client_addr){//function to update peer files

	MessageHeader h;
	peer_info p;
	
	int peer_port;
	int operation;

	recv(client_socket, &h, sizeof(h), 0); //receive message header
	recv(client_socket, &peer_port, h.length, 0); //receive peer port
	recv(client_socket, &operation, h.length, 0); //receive operation

	strcpy(p.ip,inet_ntoa(client_addr.sin_addr));
	p.port = peer_port;

	if(peers.size() < 1){
		printf("Peer %s:%d not registered\n",p.ip,p.port);
		close(client_socket);
		return 0;
	}

	for(int i=0;(long unsigned int)i < peers.size();i++){
		if(strcmp(peers[i].ip,p.ip) == 0 && peers[i].port == p.port){
			if(operation == 0){//delete a file
				char file_name_buffer[BUFSIZE] = {0};
				recv(client_socket, &h, sizeof(h), 0); //receive message header
				recv(client_socket, &file_name_buffer, h.length, 0); //receive message body
				std::string file_name = file_name_buffer;
				for(int j=0;(long unsigned int)j < peers[i].files.size();j++){
					if(peers[i].files[j].file_name == file_name){
						peers[i].files.erase(peers[i].files.begin()+j);
						printf("Peer %s:%d updated\nFile %s deleted",p.ip,p.port,file_name.c_str());
						close(client_socket);
						return 0;
					}
				}
			}
			else if(operation == 1){//add a file
				file_info f;
				char file_name_buffer[BUFSIZE] = {0};
				recv(client_socket, &h, sizeof(h), 0); //receive message header
				recv(client_socket, &file_name_buffer, h.length, 0); //receive message body
				recv(client_socket, &f.file_size, sizeof(f.file_size), 0); //receive message body
				std::string file_name = file_name_buffer;
				f.file_name = file_name;
				for(int j = 0; (long unsigned int)j<peers[i].files.size();j++){
					if(peers[i].files[j].file_name == file_name){
						printf("Peer %s:%d already hosting file %s",p.ip,p.port,file_name.c_str());
						close(client_socket);
						return 0;
					}
				}
				peers[i].files.push_back(f);
				printf("Peer %s:%d updated\nFile %s added",p.ip,p.port,file_name.c_str());
				close(client_socket);
				return 0;
				}
		}
	}
	 
	return 0;
}

int register_peer(int client_socket){//function to register peer

	char buffer[BUFSIZE] = {0};
    peer_info info;
    if((recv(client_socket, buffer, BUFSIZE, 0))<0){
        printf("Error receiving peer info for registration");
        return 0;
    }
    deserialize_peer_info(buffer, info);

    peers.push_back(info);

    printf("%s %s:%d registered\n",info.peer_id.c_str(),info.ip,info.port);
    /*
    printf("Files hosted:");
    for (int i = 0; (long unsigned int)i < info.files.size(); i++)
    {
        printf(" %s, size: %d", info.files[i].file_name.c_str(),info.files[i].file_size);
    }
    */
    close(client_socket);

    return 0;
}

int unregister_peer(int client_socket){//function to unregister peer
	char buffer[BUFSIZE] = {0};
    peer_info info;
    if((recv(client_socket, buffer, BUFSIZE, 0))<0){//receive peer info
        printf("Error receiving peer info for unregistration");
        return -1;
    }
    deserialize_peer_info(buffer, info);
    /*
    //print the files the peer is hosting (for testing purposes)
    printf("Files hosted:");
    for (int i = 0; (long unsigned int)i < info.files.size(); i++)
    {
        printf(" %s, size: %d", info.files[i].file_name.c_str(),info.files[i].file_size);
    }
    */
    close(client_socket);
    //check if peer is registered and unregister it
	for(int i=0;(long unsigned int)i<peers.size();i++){
		if(strcmp(peers[i].ip,info.ip)==0 && peers[i].port==info.port){
            mtx.lock();
			peers.erase(peers.begin()+i);
			printf("%s at %s:%d unregistered successfully\n",info.peer_id.c_str(),info.ip,info.port);
            mtx.unlock();
			close(client_socket);
			return 0;
		}
	}
    //if peer is not registered
    printf("%s %s:%d not found\n",info.peer_id.c_str(),info.ip,info.port);
    return -1;
}

//query all registered weak peers for a file, return a vector of peer_info structs containing the ip and port of the weak peers that have the file
std::vector<peer_info> queryWeakPeers(std::string file_name){
	std::vector<peer_info> result;
    mtx.lock();
	for(int i=0;(long unsigned int)i<peers.size();i++){
		for(int j=0;(long unsigned int)j<peers[i].files.size();j++){
			if(peers[i].files[j].file_name == file_name){
				result.push_back(peers[i]);
			}
		}
	}
    mtx.unlock();
	return result;
}


//send a QueryHitMessage to the specified IP and port
void sendQueryHitMessage(struct QueryHitMessage qhm, std::string ip, int port){
    //if this QueryMessage is not in our map, it has already been seen, ignore it
    if(message_id_map.find(qhm.message_id) == message_id_map.end()){
        printf("QueryHitMessage already seen\n");
        return;
    }

    //connect to the peer given in the QueryMessage struct qm
    int sock;
    if((sock = sconnect((char*)ip.c_str(), port))<0){
       printf("failed to create socket for QueryHitMessage\n"); 
    }
    //send the message header and content "QueryHit"
    MessageHeader h;
    char msg[] = "QueryHit";
    h.length = sizeof(msg);
    std::vector<uint8_t> serialized_data = serializeQueryHitMessage(qhm);
    send(sock, &h, sizeof(h), 0);
    send(sock, msg, sizeof(msg), 0);
    char rb[10] = {0};
    if((recv(sock,rb,10,0) < 0)){
        printf("Error receiving OK QueryHitMessage response\n");
        close(sock);
        return;
    }
    //serialize the QueryHitMessage struct and send it
    send(sock, serialized_data.data(), serialized_data.size(), 0);
    //close the socket
    close(sock);
    
    //remove the message id from the map
    message_id_map.erase(qhm.message_id);
    return;
}



//send a QueryMessage to all neighboring super peers
void queryMessageSuperPeers(struct QueryMessage qm){
    qm.origin_ip_address = ip;
    qm.origin_port = port;
    qm.origin_peer_id = super_peer_id;
    qm.ttl = qm.ttl - 1;
    for(int i = 0; (long unsigned int)i < super_peers.size(); i++){
        int sock = sconnect(super_peers[i].ip, super_peers[i].port);
        if(sock < 0){
            //printf("Error connecting to super peer %s:%d\n", super_peers[i].ip, super_peers[i].port);            
            continue;
        }else{
            MessageHeader h;
            char msg[] = "msgquery";
            //char response[1];
            h.length = sizeof(msg);
            if((send(sock, &h, sizeof(h), 0)) < 0){
                printf("Error sending query message header\n");
                close(sock);
                continue;
            }
            if((send(sock, msg, sizeof(msg), 0)) < 0){
                printf("Error sending query message command\n");
                close(sock);
                continue;
            }

            char rb[10] = {0};
            recv(sock, rb, 10, 0);

            if(strcmp(rb, "OK") != 0){
                printf("Error receiving OK to send query message\n");
                close(sock);
                continue;
            }

            std::vector<uint8_t> serialized_data = serializeQueryMessage(qm);
            if((send(sock, serialized_data.data(), serialized_data.size(), 0)) < 0){
                printf("Error sending query message content\n");
                close(sock);
                continue;
            }
            close(sock);
        }
    }
}

//handle a QueryMessage from a super peer
void handleQueryMessage(int client_socket){
    send(client_socket, "OK", 2, 0);
    QueryMessage qm;
    std::vector<uint8_t> serialized_data;   
    char buffer[BUFSIZE];
    int total_size = 0;
    int bytes_read;
    while((bytes_read = recv(client_socket, buffer, BUFSIZE, 0)) > 0){
        serialized_data.insert(serialized_data.end(), buffer, buffer + bytes_read);
        total_size += bytes_read;
        if(bytes_read < 0){
            printf("Error receiving query message content\n");
            close(client_socket);
            return;
        }
    }

    close(client_socket);

    if(total_size == 0){
        printf("Error receiving query message content\n");
        return;
    }

    std::vector<uint8_t> serialized_vector(serialized_data.begin(),serialized_data.end());
    try{
        qm = deserializeQueryMessage(serialized_vector);
    }catch(const std::exception& e){
        printf("Error deserializing query message content\n");
        return;
    }

    printf("Received Query Message for file: %s\n", qm.file_name.c_str());
    /*
    //print out the contents of the QueryMessage (for debugging)
    for(const auto& [key, value] : message_id_map){
        printf("Message ID: %s Upstream Peer: %s\n", key.c_str(), value.c_str());
    }
    */
    if(qm.ttl == 0 && message_id_map.find(qm.message_id) != message_id_map.end()){
        //if we have already seen this message and its ttl is 0, we need to get delete it
        printf("Message ID: %s TTL is 0, deleting it\n", qm.message_id.c_str());
        message_id_map.erase(qm.message_id);
        return;
    }
    //if we have already seen this message, we need to ignore it so we don't have duplicate QueryMessages in our map
    if(message_id_map.find(qm.message_id) != message_id_map.end()){
        printf("Already seen this query message, dropping it\n");
        return;
    }
    //if we have not seen this QueryMessage, we need to first check if we have the file in a connected weak peer
    std::vector<peer_info> result = queryWeakPeers(qm.file_name);
    //if we have the file, we need to send a QueryHitMessage to the peer that sent the QueryMessage
    message_id_map.insert(std::pair<std::string, std::string>(qm.message_id, qm.origin_peer_id));
    if(result.size() > 0){
        int random_peer = rand() % result.size();
        peer_info selectedPeer = result[random_peer];
        QueryHitMessage qhm;
        qhm.message_id = qm.message_id;
        qhm.file_name = qm.file_name;
        qhm.peer_id = selectedPeer.peer_id;
        qhm.ip_address = selectedPeer.ip;
        qhm.port = selectedPeer.port;        
        printf("File in QueryMessage found in connected weak peer. Sending QueryHitMessage to %s:%d\n", qm.origin_ip_address.c_str(), qm.origin_port);
        sendQueryHitMessage(qhm, qm.origin_ip_address, qm.origin_port);
        queryMessageSuperPeers(qm);
    }else{
        //if we don't have the file, we need to forward the QueryMessage to all super peers
        //we need to decrement the ttl by 1 and add the message id to the map        
        printf("File not located in any connected weak peers, forwarding QueryMessage to super peers\n");
        queryMessageSuperPeers(qm);
    }


    return;
}

//if we are a super peer, we need to handle the query hit message from another super peer
//we need to forward the queryHitMessage to the super peer or peer that sent the query message to us originally
//by using the map that we created
void handleQueryHitMessage(int client_socket){
    std::string fwd_peer_id;
    std::vector<uint8_t> serialized_data;
    QueryHitMessage qhm;
    char buffer[BUFSIZE];
    int bytes_read;
    send(client_socket, "OK", 2, 0);
    while((bytes_read = recv(client_socket, buffer, BUFSIZE, 0)) > 0){
        serialized_data.insert(serialized_data.end(), buffer, buffer + bytes_read);
    }
    close(client_socket);

    std::vector<uint8_t> serialized_vector(serialized_data.begin(),serialized_data.end());

    try{
        qhm = deserializeQueryHitMessage(serialized_vector);
    }catch(const std::exception& e){
        printf("Error deserializing query hit message content\n");
    return;
    }

    
    printf("Received QueryHit for file: %s at: %s:%d\n",qhm.file_name.c_str(), qhm.ip_address.c_str(), qhm.port);
    //check if we have seen this message id before, if so we can use the map to forward the query hit message to the correct super peer or weak peer
    if(message_id_map.find(qhm.message_id) == message_id_map.end()){
        printf("Cannot find QueryHit message id: %s in map\n", qhm.message_id.c_str());
    }
    //if we have seen this message id before, we need to forward the query hit message to the correct super peer or weak peer
    else{
        fwd_peer_id = message_id_map[qhm.message_id];
        //if the peer id is not a super peer, we need to forward the query hit message to the weak peer
        
        for(int i = 0; (long unsigned int)i < super_peers.size(); i++){
            if(super_peers[i].super_peer_id == fwd_peer_id){
                sendQueryHitMessage(qhm, super_peers[i].ip, super_peers[i].port);
                printf("Forwarding QueryHit to super peer: %s", fwd_peer_id.c_str());
                //after we forward the query hit message, we need to delete the message id from the map
                message_id_map.erase(qhm.message_id);
                return;
            }
        }
        for(int i = 0; (long unsigned int)i < peers.size(); i++){
            if(peers[i].peer_id == fwd_peer_id){
                printf("Forwarding QueryHit to peer: %s\n", fwd_peer_id.c_str());
                sendQueryHitMessage(qhm, peers[i].ip, peers[i].port);
                message_id_map.erase(qhm.message_id);
                return;
            }
        }
    }
    printf("QueryHit message id: %s not found in map\n", qhm.message_id.c_str());
}

void handle_connection(int client_socket){//function to handle connections
	char buffer[BUFSIZE] = {0};
	MessageHeader h;
    //receive message header
	recv(client_socket, &h, sizeof(h), 0);
    //receive message body
	recv(client_socket, buffer, BUFSIZE, 0);

	if(strncmp(buffer,"register",8)==0){//register peer
        send(client_socket, "OK", 2, 0);
		//printf("Registering peer\n");
		register_peer(client_socket);
	}
	else if(strncmp(buffer,"unregister",10)==0){//unregister peer
        send(client_socket,"OK",2,0);
		//printf("Unregistering peer\n");
		unregister_peer(client_socket);
	}
	else if(strncmp(buffer,"list",4)==0){//list peers
		printf("Listing peers and their files\n");
		//TODO
	}
    else if(strncmp(buffer,"msgquery",5)==0){//query from super peer"{
        //printf("Querying super-peers for file\n");
        handleQueryMessage(client_socket);
    }
    else if(strncmp(buffer,"QueryHit",8)==0){//query hit from super peer"{
        printf("Received query hit from super-peer\n");
        handleQueryHitMessage(client_socket);
    }
	else if(strncmp(buffer,"update",6)==0){
		printf("Updating peer files\n");
		//update_all_peer_files(client_socket,client_addr);
	}
	else if(strncmp(buffer,"single_update",13)==0){
		//update_peer_files(client_socket,client_addr);
	}
	else{//invalid command
        send(client_socket, "Invalid command", 15, 0);
		printf("Invalid command: %s\n",buffer);
	}

    accepted_connections--;

	fflush(stdout);
	try {
        close(client_socket);
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << '\n';
    }
	//printf("connection closed\n");
}

void thread_pool_function(){
    while(true){
        int *client_socket;
        std::unique_lock<std::mutex> lock(mtx);
        if((client_socket = socket_queue.dequeue()) == NULL){
            cv.wait(lock);
        }
        lock.unlock();
        if(client_socket != NULL){
            handle_connection(*client_socket);
        }
    }
    return;
}

void handle_sigpipe(int sig) {
    void *array[10];
    size_t size;
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    // print out all the frames to stderr
    fprintf(stderr, "%s Error: signal %d: %s\n",super_peer_id.c_str(),sig, strsignal(sig));
    backtrace_symbols_fd(array, size, STDOUT_FILENO);
    //exit(1);
}

int main(int argc, char *argv[]){//main function
	if(argc < 2){
		printf("Usage: %s <config file path>\n",argv[0]);
		loadConfig(CONFIG_FILE_PATH);
	}else{
		loadConfig(argv[1]);
	}

    signal(SIGPIPE, handle_sigpipe);

	int opt = 1;

	int server_socket, client_socket, addr_size;
	sockaddr_in server_addr, client_addr;

	check((server_socket = socket(AF_INET, SOCK_STREAM, 0)),"socket creation failed");//create socket

	if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){//set socket options
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	server_addr.sin_family = AF_INET;//configure server
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	check(bind(server_socket,(sockaddr*)&server_addr,sizeof(server_addr)),"bind failed");//bind socket
	check(listen(server_socket, SERVER_BACKLOG),"listen failed");//listen for connections

	printf("Waiting for connections on port %d...\n",port);

    for(int i = 0; i < THREAD_POOL_SIZE; i++){
        thread_pool[i] = std::thread(&thread_pool_function);
    }

	while(true){//accept connections
		addr_size = sizeof(sockaddr_in);
		check((client_socket = accept(server_socket, (sockaddr*)&client_addr,(socklen_t*)&addr_size)),"failed to accept client connection");//accept connection
        accepted_connections++;

        if(accepted_connections > 25){
            printf("TOO MANY CONNECTIONS\n");
        }

        int *client = (int*)malloc(sizeof(int));
        *client = client_socket;

        std::unique_lock<std::mutex> lock(mtx);
        socket_queue.enqueue(client);
        lock.unlock();
        cv.notify_one();
    }
	return 0;
}

