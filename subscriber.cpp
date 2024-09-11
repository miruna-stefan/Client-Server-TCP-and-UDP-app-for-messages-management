// STEFAN MIRUNA ANDREEA 324CA
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.hpp"
#include "helpers.hpp"

using namespace std;

#define MSG_MAXSIZE 1700
#define MAX_CONNECTIONS 32

/* function that receives a command of format "subscribe <topic>" or
"unsubscribe <topic>" and returns only the topic or NULL if the
command is not valid */
char *get_topic(char *buffer)
{
    char *copy_buffer = (char *)malloc(strlen(buffer) * sizeof(char));
    strcpy(copy_buffer, buffer);

    // get the first word from the command
    char *command = strtok(copy_buffer, " ");
    /* the client only accepts "exit", "subscribe" and "unsubscribe" commands.
    This cannot be an "exit" command because we have already checked that, so
    if it is not "subscribe" or "unsubscribe", the command is invalid */
    if (strcmp(command, "subscribe") != 0 && strcmp(command, "unsubscribe") != 0)
        return NULL;

    char *topic = strtok(NULL, " ");
    return topic;
}

void initialize_poll_fd(int index, struct pollfd *poll_fds, int fd, int &num_sockets)
{
    poll_fds[index].fd = fd;
    poll_fds[index].events = POLLIN;
    num_sockets++;
}

void initialize_poll_fds(struct pollfd *poll_fds, int fd, int &num_sockets)
{
    initialize_poll_fd(0, poll_fds, fd, num_sockets);
    initialize_poll_fd(1, poll_fds, STDIN_FILENO, num_sockets);
}

bool is_command_valid(char *buf, char *topic)
{
    // check if the command is complete, meaning that the topic is not omitted
    if (topic == NULL) {
        fprintf(stderr, "Invalid command.\n");
        return false;
    }

    // if the command is different from subscribe or unsubscribe, it is invalid
    if (strncmp(buf, "subscribe", strlen("subscribe")) != 0
        && strncmp(buf, "unsubscribe", strlen("unsubscribe")) != 0) {
        fprintf(stderr, "Invalid command.\n");
        return false;
    }

    return true;
}

void handle_subscription_commands(char *buf, int socketfd)
{
    char *topic = get_topic(buf);
    if (!is_command_valid(buf, topic))
        return;

    send_all(socketfd, buf, MSG_MAXSIZE);

    if (strncmp(buf, "subscribe", strlen("subscribe")) == 0) {
        printf("Subscribed to topic %s", topic);
        return;
    }

    if (strncmp(buf, "unsubscribe", strlen("unsubscribe")) == 0)
        printf("Unsubscribed from topic %s", topic);
}

void run_client(int socketfd)
{
    char buf[MSG_MAXSIZE];
    memset(buf, 0, MSG_MAXSIZE);

    // messages can either be received on socketfd or from stdin
    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_sockets = 0;

    initialize_poll_fds(poll_fds, socketfd, num_sockets);

    while(1) {
        int rc = poll(poll_fds, num_sockets, -1);
        DIE(rc < 0, "poll");

        // check if we have received input from the socket
        if (poll_fds[0].revents & POLLIN) {
            char *message= (char *)calloc(MSG_MAXSIZE, sizeof(char));
            DIE(message == NULL, "calloc failed\n");

            int rc = recv_all(poll_fds[0].fd, message, MSG_MAXSIZE);
            DIE(rc < 0, "recv failed\n");

            if (rc == 0) {
                return;
            }

            printf("%s", message);
            continue;
        }

        // check if we have received input from stdin
        if (poll_fds[1].revents & POLLIN) {
            fgets(buf, sizeof(buf), stdin);

            // identify command type

            // check if the command is exit
            if (strncmp(buf, "exit", strlen("exit")) == 0) {
                shutdown(socketfd, SHUT_RDWR);
                return;
            }
    
            // if we have reached this point, the command is either subscribe or unsubscribe
            handle_subscription_commands(buf, socketfd);
            continue;
        }
    }
}

// this function has been inspired from the 7th lab
int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != 4, "Usage: ./subscriber <id> <ip> <port>");

    // parse port as a number
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, socket_len);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");
    
    // Connect to server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    // send id to server
    char *id = (char *)calloc(MSG_MAXSIZE, sizeof(char));
    strcpy(id, argv[1]);
    send_all(sockfd, id, MSG_MAXSIZE);

    run_client(sockfd);

    // End connection
    close(sockfd);

    return 0;

}