#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

// Don't include these in another file.
#include "thread.h"
#include "thread-sync.h"

#define BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 1024
#define DEFAULT_PORT 8080

// Function declarations
void handle_request(int client_socket);
void send_200_response(int client_socket, const char *response_body);
void send_404_response(int client_socket);
void send_500_response(int client_socket);
void log_request(const char *method, const char *path, int status_code);

// multi-thread
#define Q_SIZE 8
#define WORKER_COUNT 4
#define CAN_PRODUCE (!QUEUE_FULL && GLOBAL_CLIENT_SOCKET_UPDATED)
#define CAN_CONSUME (!QUEUE_EMPTY)
#define QUEUE_EMPTY (tail == -1)
#define QUEUE_FULL (!QUEUE_EMPTY && (tail + 1) % Q_SIZE == head)
int GLOBAL_CLIENT_SOCKET_UPDATED = 0;
int head = 0, tail = -1;
int global_client_socket = -1;
mutex_t lk = MUTEX_INIT();
cond_t cv = COND_INIT();
typedef struct {
    int client_socket;
} Task;
Task TaskQueue[Q_SIZE];
void T_PRODUCER() {
    while(1) {
        mutex_lock(&lk);
        while(!CAN_PRODUCE){
            cond_wait(&cv, &lk);
        }
        tail = (tail + 1) % Q_SIZE;
        TaskQueue[tail].client_socket = global_client_socket;
        cond_broadcast(&cv);
        mutex_unlock(&lk);
        GLOBAL_CLIENT_SOCKET_UPDATED = 0;
    }
}
void T_CONSUMER() {
    while(1) {
        mutex_lock(&lk);
        while(!CAN_CONSUME){
            cond_wait(&cv, &lk);
        }
        int client_socket = TaskQueue[head].client_socket;
        if(head == tail) {
            tail = -1;
            head = 0;
        } else {
            head = (head + 1) % Q_SIZE;
        }
        cond_broadcast(&cv);
        mutex_unlock(&lk);

        char buffer[BUFFER_SIZE];
        int bytes_received;
        // Read request
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Failed to receive data or connection closed by client.\n");
            return;
        }
        buffer[bytes_received] = '\0';
        // Parse HTTP request line: "METHOD /path HTTP/version"
        char method[16] = {0};
        char path[MAX_PATH_LENGTH] = {0};
        char version[16] = {0};
        
        // Extract the first line of the request
        char *first_line = strtok(buffer, "\r\n");
        if (first_line) {
            sscanf(first_line, "%15s %1023s %15s", method, path, version);
            
            // Separate path from query string for file access
            char file_path[MAX_PATH_LENGTH] = {0};
            char *query_start = strchr(path, '?');
            if (query_start) {
                // Copy only the path part (before '?')
                int path_len = query_start - path;
                strncpy(file_path, path, path_len);
                file_path[path_len] = '\0';
            } else {
                // No query string, use the full path
                strcpy(file_path, path);
            }
            
            // Check if file exists
            char full_path[MAX_PATH_LENGTH + 10]; // Extra space for "./" prefix
            snprintf(full_path, sizeof(full_path), ".%s", file_path); // Prepend current directory
            
            if (access(full_path, F_OK) == 0) {
                // Check if it's executable
                if (access(full_path, X_OK) == 0) {
                    // Create pipe to capture child process output
                    int pipefd[2];
                    if (pipe(pipefd) == -1) {
                        perror("pipe failed");
                        send_500_response(client_socket);
                        log_request(method, path, 500);
                    } else {
                        // Execute the file using fork() and execl()
                        pid_t pid = fork();
                        if (pid == 0) {
                            // Set essential CGI environment variables
                            setenv("REQUEST_METHOD", method, 1);
                            
                            // Parse query string from path (everything after '?')
                            if (query_start) {
                                setenv("QUERY_STRING", query_start + 1, 1);
                            } else {
                                setenv("QUERY_STRING", "", 1);
                            }
                            
                            // Redirect stdout to pipe
                            close(pipefd[0]);  // Close read end
                            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe write end
                            dup2(pipefd[1], STDERR_FILENO);  // Redirect stderr to pipe write end
                            close(pipefd[1]);  // Close original pipe write end
                            
                            execl(full_path, full_path, NULL);
                            // If execl returns, there was an error
                            perror("execl failed");
                            exit(EXIT_FAILURE);
                        } else if (pid > 0) {
                            // Parent process - read child output and wait for completion
                            close(pipefd[1]);  // Close write end
                            
                            // Use dynamic buffer allocation for larger outputs
                            char *output_buffer = malloc(BUFFER_SIZE);
                            if (!output_buffer) {
                                perror("malloc failed");
                                close(pipefd[0]);
                                send_500_response(client_socket);
                                log_request(method, path, 500);
                                waitpid(pid, NULL, 0);  // Clean up child
                                return;
                            }
                            
                            ssize_t bytes_read;
                            int total_bytes = 0;
                            int buffer_size = BUFFER_SIZE;
                            
                            // Read all output from child process with dynamic buffer expansion
                            while ((bytes_read = read(pipefd[0], output_buffer + total_bytes, 
                                                    buffer_size - total_bytes - 1)) > 0) {
                                total_bytes += bytes_read;
                                
                                // If buffer is getting full, expand it
                                if (total_bytes >= buffer_size - 1024) {
                                    buffer_size *= 2;
                                    char *new_buffer = realloc(output_buffer, buffer_size);
                                    if (!new_buffer) {
                                        break;
                                    }
                                    output_buffer = new_buffer;
                                }
                            }
                            output_buffer[total_bytes] = '\0';
                            close(pipefd[0]);  // Close read end
                            
                            // Wait for child to complete
                            int status;
                            waitpid(pid, &status, 0);
                            
                            if (WIFEXITED(status)) {
                                send_200_response(client_socket, output_buffer);
                                log_request(method, path, 200);
                            } else {
                                send_500_response(client_socket);
                                log_request(method, path, 500);
                            }
                            
                            free(output_buffer);  // Clean up allocated memory
                        } else {
                            // Fork failed
                            perror("fork failed");
                            close(pipefd[0]);
                            close(pipefd[1]);
                            send_500_response(client_socket);
                            log_request(method, path, 500);
                        }
                    }
                } else if (access(full_path, R_OK) == 0) {
                    send_404_response(client_socket); // Treat as not found for non-executable files
                    log_request(method, path, 404);
                } else {
                    send_404_response(client_socket);
                    log_request(method, path, 404);
                }
            } else {
                // File does not exist
                send_404_response(client_socket);
                log_request(method, path, 404);
            }
        }

        // Close the client socket after processing
        close(client_socket);
    }
}

int main(int argc, char *argv[]) {
    spawn(T_PRODUCER);
    for(int i = 0; i < WORKER_COUNT; i++) {
        spawn(T_CONSUMER);
    }

    // Socket variables
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Get port from command line or use default
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    // Set up signal handler for SIGPIPE to prevent crashes
    // when client disconnects
    signal(SIGPIPE, SIG_IGN);

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address
    // (prevents "Address already in use" errors)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any interface
    server_addr.sin_port = htons(port);       // Convert port to network byte order

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections with system-defined maximum backlog
    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    // Main server loop - accept and process connections indefinitely
    while (1) {
        // Accept new client connection
        if ((client_socket = accept(server_socket,
                                    (struct sockaddr *)&client_addr,
                                    &client_len)) < 0) {
            perror("Accept failed");
            continue;  // Continue listening for other connections
        }

        // Set timeouts to prevent hanging on slow or dead connections
        struct timeval timeout;
        timeout.tv_sec = 30;  // 30 seconds timeout
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO,
                   (const char*)&timeout, sizeof(timeout));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO,
                   (const char*)&timeout, sizeof(timeout));

        // Process the client request
        handle_request(client_socket);
    }

    // Clean up (note: this code is never reached in this example)
    close(server_socket);
    join();
    return 0;
}

void handle_request(int client_socket) {
    mutex_lock(&lk);
    global_client_socket = client_socket;
    GLOBAL_CLIENT_SOCKET_UPDATED = 1;
    cond_broadcast(&cv);
    mutex_unlock(&lk);
}

void log_request(const char *method, const char *path, int status_code) {
    time_t now;
    struct tm *tm_info;
    char timestamp[26];

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // In real systems, we write to a log file,
    // like /var/log/nginx/access.log
    printf("[%s] [%s] [%s] [%d]\n", timestamp, method, path, status_code);
    fflush(stdout);
}

void send_200_response(int client_socket, const char *response_body) {
    int body_length = strlen(response_body);
    
    char content_length_header[64];
    sprintf(content_length_header, "Content-Length: %d\r\n", body_length);
    
    send(client_socket, "HTTP/1.1 200 OK\r\n", 17, 0);
    send(client_socket, "Content-Type: text/html; charset=utf-8\r\n", 40, 0);
    send(client_socket, content_length_header, strlen(content_length_header), 0);
    send(client_socket, "Connection: close\r\n", 19, 0);
    send(client_socket, "\r\n", 2, 0);
    send(client_socket, response_body, body_length, 0);
}

void send_404_response(int client_socket) {
    const char *response = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    send(client_socket, response, strlen(response), 0);
}

void send_500_response(int client_socket) {
    const char *response = 
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    send(client_socket, response, strlen(response), 0);
}
