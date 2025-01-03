#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT "8000"      // The port the server will listen on
#define BACKLOG 10       // Maximum number of queued connections

void *handle_client(void *arg);

int main() {
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes = 1;
    int rv;

    // Zero out the hints structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;       // Use either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;   // TCP stream socket
    hints.ai_flags = AI_PASSIVE;       // Use my IP address automatically

    // Get server address information
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            continue;  // If socket creation fails, try the next address
        }

        // Set socket options to allow port reuse
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(sockfd);
            return 1;
        }

        // Bind the socket to the specified port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);  // Close the socket if bind fails
            continue;       // Try the next address
        }

        break; // Successfully bound to an address
    }

    freeaddrinfo(servinfo);  // Free the address info structure

    // If p is NULL, then we failed to bind to any address
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 1;
    }

    // Listen for incoming connections
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("server: waiting for connections...\n");

    // Main accept loop to handle incoming connections
    while (1) {
        sin_size = sizeof(their_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        // Create a new thread for each client connection
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)(intptr_t)new_fd) != 0) {
            perror("pthread_create");
            close(new_fd);
            continue;
        }

        pthread_detach(client_thread); // Automatically cleans up thread
    }

    close(sockfd);
    return 0;
}

// Function to handle each client's request
void *handle_client(void *arg) {
    int new_fd = (int)(intptr_t)arg;
    char buf[1024];
    int bytes_received;

    // Receive the client's request
    bytes_received = recv(new_fd, buf, sizeof(buf) - 1, 0);
    if (bytes_received == -1) {
        perror("recv");
        close(new_fd);
        pthread_exit(NULL);
    }
    buf[bytes_received] = '\0'; // Null-terminate the received data
    printf("Received data: %s\n", buf); // Debug output to verify request content

    // Check if it's a GET request
    if (strncmp(buf, "GET ", 4) != 0) {
        const char *bad_request = "HTTP/1.0 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        printf("Non-GET request received, sending 400 Bad Request.\n"); // Debug output
        send(new_fd, bad_request, strlen(bad_request), 0);
        close(new_fd);
        pthread_exit(NULL);
    }

    // Debug output for response being sent
    printf("Sending 200 OK response to client.\n");
    
    // Send a basic 200 OK response to confirm server is working
    const char *ok_response = "HTTP/1.0 200 OK\r\nContent-Length: 12\r\n\r\nHello World!";
    if (send(new_fd, ok_response, strlen(ok_response), 0) == -1) {
        perror("send");
    }

    // Close the connection and end the thread
    close(new_fd);
    pthread_exit(NULL);
}
