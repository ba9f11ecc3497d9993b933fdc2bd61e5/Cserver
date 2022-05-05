#include <stdlib.h>
#include <stdio.h> 
#include <string.h>         //required by memset and by custom function to check errors
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>     //required to convert hostbyte order to network byte order (hton) and also to declare struct sockaddr_in;
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>         //needed for read
#include <poll.h>           //needed to poll connections
#include <errno.h>

int client_address;
char buffer[4096];
struct pollfd poll_file_descriptors[20];
int poll_struct_number = 0;
int poll_struct_file_descriptors_total = 20;
int poll_timeout = 2;
int TRUE = 1; 
int pooler_num = 0;
struct sockaddr_in peer;




// function to create a socket as INET4 FAMILY; the socket is a file descriptor
int SocketCreate(void) {
   int tcp_socket;
   tcp_socket = socket(AF_INET , SOCK_STREAM , 0);
   return tcp_socket;
}

// functions to avoid checking the return code of socket actions in main code
void CheckReturnCodeCreateSocket(int return_code) {
    if (return_code == -1) {
        printf("FAIL : return code is %d for Create Socket\n", return_code);
        exit(1);
    }
    else {
        printf("SUCCESS : return code is %d for Create Socket\n", return_code);
    }
}

void CheckSetSocketReuse(int return_code) {
    if (return_code < 0) {
        printf("FAIL : return code is %d for applying option SO_REUSEADDR to socket\n", return_code);
        exit(1);
    }
    else {
        printf("SUCCESS : return code is %d for applying option SO_REUSEADDR to socket\n", return_code);
    }    
}

void CheckSetSocketPortReuse(int return_code) {
    if (return_code < 0) {
        printf("FAIL : return code is %d for applying option SO_REUSEPORT to socket\n", return_code);
        exit(1);
    }
    else {
        printf("SUCCESS : return code is %d for applying option SO_REUSEPORT to socket\n", return_code);
    }    
}

void CheckReturnCodeBindSocket(int return_code) {
    if (return_code > 0) {
        printf("FAIL : return code is %d for Bind Socket\n", return_code);
        exit(1);
    }
    else {
        printf("SUCCESS : return code is %d for Bind Socket\n", return_code);
    }    
}

void CheckReturnCodeListenSocket(int return_code) {
    if (return_code == -1) {
        printf("FAIL : return code is %d for Listen Socket\n", return_code);
        exit(1);
    }
    if (return_code == 0) {
        printf("SUCCESS : return code is %d for Listen Socket\n", return_code);
    }    
}

void CheckReturnCodeAcceptSocket(int return_code) {
    if (return_code < 0) {
        printf("FAIL : return code is %d for Accept Socket\n", return_code);
        exit(1);
    }
    else {
        printf("SUCCESS : return code is %d for Accept Socket\n", return_code);
    } 
}

void CheckReturnCodePoller(int return_code, int pooler_num) {
    if (return_code < 0) {
        printf("FAIL : return code is %d for poll call\n", return_code);
        exit(1);
    }
    if (return_code > 0) {
        printf("SUCCESS : return code for poll call is %d (number of POOLIN events found), checking pooler #%d\n", return_code, pooler_num);
    } 
}    

void CheckReturnCodeReadSocket(int return_code) {
    if (return_code < 0) {
        printf("FAIL : return code is %d for Read Socket\n", return_code);
        exit(1);
    }
    else {
        printf("SUCCESS : return code is %d for Read Socket\n", return_code);
    } 
}



int main() {
    printf("===== Server is starting =====\n");
    int tcp_socket;
    tcp_socket = SocketCreate();
    CheckReturnCodeCreateSocket(tcp_socket);
    
    //SOL_SOCKET means apply option at the socket level, SO_REUSEADDR Specifies that the rules used in validating addresses supplied to bind() should allow reuse of local addresses,
    int set_socket_reuse = setsockopt(tcp_socket, SOL_SOCKET,  SO_REUSEADDR, &TRUE, sizeof(TRUE));
    CheckSetSocketReuse(set_socket_reuse);
    int set_socket_port_reuse = setsockopt(tcp_socket, SOL_SOCKET,  SO_REUSEPORT, &TRUE, sizeof(TRUE));
    CheckSetSocketPortReuse(set_socket_port_reuse);
    
    // socket parameters, here we use an existing struct (sockaddr_in) from netinet/in.h
    // inet_pton is needed to put the address in network byte order
    struct sockaddr_in socket_params;                                                                             
    socket_params.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &socket_params.sin_addr); 
    socket_params.sin_port = htons(8080);
    
    // here we bind the socket (sockaddr_in* can be cast to type sockaddr*)
    int bind_socket = bind(tcp_socket, (struct sockaddr *) &socket_params, sizeof(socket_params));
    CheckReturnCodeBindSocket(bind_socket);
    
    
    // start listening on the socket (5 is the backlog of how many connection can queue up before you accept() them  
    // client_address_length is needed because the accept() needs the length of client address to pass 2 pointers to accept()
    int listen_socket = listen(tcp_socket,5);                                                                     
    CheckReturnCodeListenSocket(listen_socket);                                                                                                                                                  
    socklen_t client_address_length = sizeof(client_address);
    
    while(1) {
        // loop in all the pollfds structs and set POLLIN or POLLPRI as an event of interest (poll will try to match revents in the same struct against these events)
        for (poll_struct_number = 0; poll_struct_number < poll_struct_file_descriptors_total; poll_struct_number++) {
         
            // fill the pollfd structs using tcp_socket as our fd
            poll_file_descriptors[poll_struct_number].fd = tcp_socket;
            poll_file_descriptors[poll_struct_number].events = POLLIN;
            
            // poll all fd's
            int poller = poll(poll_file_descriptors, 1, poll_timeout);
            CheckReturnCodePoller(poller, poll_struct_number);
            if (poll_file_descriptors[poll_struct_number].revents == POLLIN) {
                // launch accept() if we got a POOLIN event
                // passing both pointers to struct of type sockaddr (this is the actual connection)
                int connected_socket = accept(tcp_socket, (struct sockaddr *)&client_address, &client_address_length);                
                CheckReturnCodeAcceptSocket(connected_socket);
                // get client ip and port
                socklen_t peer_len = sizeof(peer);
                getpeername(connected_socket, (struct sockaddr *)&peer, &peer_len);
                printf("client address is %s\n", inet_ntoa(peer.sin_addr));
                printf("Peer's port is: %d\n", (int) ntohs(peer.sin_port));
                int socket_reader = read(connected_socket, buffer, 4096);
                CheckReturnCodeReadSocket(socket_reader);
                printf("client is sending :\n================\n%s\n================\n", buffer);
                bzero(buffer,4096);
                char *reply =
                "HTTP/1.1 200 OK\n"
                "Server: CServer/0.1\n"
                "Content-Type: text/html; charset=utf-8\n"
                "\n"
                "<html><body><p>hello world</p></body></html>";
                printf("reply is : \n================\n%s\n================\n", reply);
                send(connected_socket, reply, strlen(reply), 0);
                close(connected_socket);
            }            
        }
    }
}
    

