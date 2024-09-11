// STEFAN MIRUNA ANDREEA 324CA
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include <netinet/tcp.h>

using namespace std;

#include "common.hpp"
#include "helpers.hpp"

#define MAX_CONNECTIONS 32
#define MSG_MAXSIZE 1700
#define PAYLOAD_MAXlENGTH 1500
#define TOPIC_MAXLENGTH 50
#define WILDCARDS_MAXLENGTH 50
#define CLIENT_ID_MAXLENGTH 10

// map that makes the association between the client id and the socket file descriptor
map<string, int> clients;

// map that makes the association between the topic and the clients that subscribed to it
map<string, vector<string>> topics;

struct udp_message {
    char topic[TOPIC_MAXLENGTH];
    uint8_t type;
    char payload[PAYLOAD_MAXlENGTH];
};

// function that allocates memory for the udp message and adds the ip address and port to the message
char *start_message(struct sockaddr_in udp_addr, struct udp_message *msg)
{
    char *message = (char *)calloc(MSG_MAXSIZE, sizeof(char));
    DIE(message == NULL, "Error allocating memory for message\n");

    char *ip = inet_ntoa(udp_addr.sin_addr);
    uint16_t port = ntohs(udp_addr.sin_port);

    // add ip address and port to the message
    strcat(message, ip);
    strcat(message, ":");
    strcat(message, to_string(port).c_str());
    strcat(message, " - ");
    strcat(message, msg->topic);
    strcat(message, " - ");
    return message;
}

void handle_message_type_INT(char *message, struct udp_message *msg)
{
    uint8_t int_sign;
    int32_t int_value = 0;


    strcat(message, "INT - ");    
    memcpy(&int_sign, msg->payload, sizeof(uint8_t));
    memcpy(&int_value, msg->payload + sizeof(uint8_t), sizeof(uint32_t));
            
    if (int_sign == 1) {
        // the value should be negative, so add the minus sign
        int_value = - ntohl(int_value);
    } else {
        // the value is positive
        int_value = ntohl(int_value);
    }

    strcat(message, std::to_string(int_value).c_str());
}

void handle_message_type_SHORT_REAL(char *message, struct udp_message *msg)
{
    uint16_t short_real;
    double short_real_value;
    strcat(message, "SHORT_REAL - ");
            
    // the message payload represent the abs value of the number * 100
    memcpy(&short_real, msg->payload, sizeof(uint16_t));
    short_real_value = ntohs(short_real) / 100.0;

    strcat(message, (to_string(short_real_value)).c_str());
}

void handle_message_type_FLOAT(char *message, struct udp_message *msg)
{
    uint8_t float_sign;
    uint32_t all_digits_concatenated_module = 0;
    uint8_t power;
    double float_value;

    strcat(message, "FLOAT - ");   
    memcpy(&float_sign, msg->payload, sizeof(uint8_t));   
    memcpy(&all_digits_concatenated_module, msg->payload + sizeof(uint8_t), sizeof(uint32_t));
    all_digits_concatenated_module = ntohl(all_digits_concatenated_module);
            
    memcpy(&power, msg->payload + sizeof(uint8_t) + sizeof(uint32_t), sizeof(uint8_t));

    float_value = (double)all_digits_concatenated_module / pow(10, power);

    if (float_sign == 1)
        float_value = -float_value;

    strcat(message, std::to_string(float_value).c_str());
}

void handle_message_type_STRING(char *message, struct udp_message *msg)
{
    strcat(message, "STRING - ");
    strcat(message, msg->payload);
}

void continue_message_according_to_type(char *message, struct udp_message *msg)
{
    switch(msg->type) {
        case 0: {
            handle_message_type_INT(message, msg);
            break;
        }
        case 1: {
            handle_message_type_SHORT_REAL(message, msg);
            break;
        }
        case 2: {
            handle_message_type_FLOAT(message, msg);
            break;
        }
        case 3: {
            handle_message_type_STRING(message, msg);
            break;
        }
        default: {
            break;
        }
    }
}

// function that creates the message string that will be sent to the clients
char *create_message_string(struct sockaddr_in udp_addr, struct udp_message *msg)
{
    // the message should have the format: ip_address:port - <topic> - data_type - message_value
    char *message = start_message(udp_addr, msg);

    uint8_t int_sign;
    int32_t int_value;
    uint16_t short_real;
    double short_real_value;
    uint8_t float_sign;
    uint32_t all_digits_concatenated_module;
    uint8_t power;
    double float_value;

    continue_message_according_to_type(message, msg);

    strcat(message, "\n");
    return message;

}

// this function is called when we receive input from keyboard (fd 2 is triggered in poll function)
// we are only interested to see if we received the "exit" command. If so, return true, else false
bool need_to_exit()
{
    // read the input from keyboard
    char buffer[MSG_MAXSIZE + 1];
    memset(buffer, 0, MSG_MAXSIZE + 1);

    fgets(buffer, sizeof(buffer), stdin);

    if (strncmp(buffer, "exit", 4) == 0) {
        return true;
    }

    return false;
}

/* function that looks for the client_id in the clients map
and returns true if it finds, false otherwise */
bool does_client_ID_already_exist(char *client_id)
{
    string client_id_string;
    for (int i = 0; i < CLIENT_ID_MAXLENGTH && client_id[i] != '\0'; i++) {
        client_id_string.push_back(client_id[i]);
    }

    if (clients.find(client_id_string) == clients.end()) {
        return false;
    }

    fprintf(stdout, "Client %s already connected.\n", client_id);
    return true;
}

/* function that returns only the topic string from the received buffer
(of type "subscribe <topic>" or "unsubscribe <topic>") */
string get_topic(char *buffer)
{
    char *copy_buffer = (char *)calloc(MSG_MAXSIZE, sizeof(char));
    DIE(copy_buffer == NULL, "Error allocating memory for copy_buffer\n");
    strcpy(copy_buffer, buffer);
    
    // get the first word of the command ("subscribe" or "unsubscribe")
    char *subscription = strtok(copy_buffer, " ");

    // get the second word of the command, namely the topic
    subscription = strtok(NULL, " ");

    string topic;
    // transform the topic from char * to string
    for (int i = 0; i < TOPIC_MAXLENGTH && subscription[i] != '\n'; i++) {
        topic.push_back(subscription[i]);
    }

    return topic;
}

/* function that returns the client_id string corresponding to
the socket filedescriptor received as parameter */
string get_client_id_from_sockfd(int sockfd)
{
    // parse the clients map in order to get the client_id (key) associated with the
    // given socket file descriptor (value)
    string client_id = "";
    for (auto it = clients.begin(); it != clients.end(); it++) {
        if (it->second == sockfd) {
            client_id = it->first;
            break;
        }
    }

    return client_id;

}


void subscription_handle(char *buffer, int sockfd)
{
    string client_id = get_client_id_from_sockfd(sockfd);
    string topic = get_topic(buffer);

    // check if the topic already exists in the map
    if (topics.find(topic) == topics.end()) {
        // if it doesn't exist, add it to the map
        topics.insert({topic, vector<string>()});
    }

    // parse the vector of subscribers to that topic and look for the client_id
    for (int i = 0; i < topics[topic].size(); i++) {
        if (topics[topic][i] == client_id) {
            // if the client_id is already subscribed to the topic, return
            return;
        }
    }

    // if the client_id is not subscribed to the topic, add it to the vector
    topics[topic].push_back(client_id);
}

void unsubscription_handle(char *buffer, int sockfd)
{
    string client_id = get_client_id_from_sockfd(sockfd);
    string topic = get_topic(buffer);

    // check if the topic exists in the map
    if (topics.find(topic) == topics.end()) {
        // if it doesn't exist, return
        return;
    }

    // parse the vector of subscribers to that topic and look for the client_id
    for (int i = 0; i < topics[topic].size(); i++) {
        if (topics[topic][i] == client_id) {
            // if the client_id is subscribed to the topic, remove it from the vector
            topics[topic].erase(topics[topic].begin() + i);
            return;
        }
    }
}

// function that counts how many times the characters '+' and '*' can be found in the topic
int get_number_of_wildcards(string topic, char *wildcards)
{
    int count = 0;
    for (int i = 0; i < topic.size(); i++) {
        if (topic[i] == '+') {
            wildcards[count] = '+';
            count++;
        } else {
            if (topic[i] == '*') {
                wildcards[count] = '*';
                count++;
            }
        }
    }

    return count;
}

/* function that looks for matches after the '+' character and
returns false if there is no match, true otherwise */
bool handle_plus_wildcard(string &rest_of_topic_without_first_word, string &after_wildcard_character,
                            string rest_of_topic_udp_msg, char *wildcards, int k, int count)
{
    // only take what remains after the character "/" from the rest_of_topic
    // get rid of the first word from the rest_of_topic_udp_msg
    rest_of_topic_without_first_word = rest_of_topic_udp_msg.substr(rest_of_topic_udp_msg.find("/"), rest_of_topic_udp_msg.size());

    // check if there is another wildcard in the topic
    if (after_wildcard_character.find("+") != string::npos
        || after_wildcard_character.find("*") != string::npos) {
        
        // check if the string between the wildcards matches the corresponding string from the udp message topic
        string between_wildcards = after_wildcard_character.substr(0, after_wildcard_character.find(wildcards[k - count + 1]));
        string first_word_without_slash = rest_of_topic_without_first_word.substr(1, rest_of_topic_without_first_word.size());
        first_word_without_slash = first_word_without_slash.substr(0, first_word_without_slash.find("/"));

        // check the match without considering the "/" delimiters
        if (first_word_without_slash != between_wildcards.substr(1, between_wildcards.size() - 2)) {
            return false;
        }
                                
        // if we are here, it means that the 2 strings match up until this point,
        // so we need to prepare the variables for a new verification of the next wildcard
        rest_of_topic_without_first_word = rest_of_topic_without_first_word
                                            .substr(rest_of_topic_without_first_word.find(between_wildcards) + between_wildcards.size(),
                                            rest_of_topic_without_first_word.size());
                                
        after_wildcard_character = after_wildcard_character.substr(between_wildcards.size(),
                                    after_wildcard_character.size());
    } else {
        /* if we are here, it means that there are no other wildcards in the topic,
        so the rest of the topic should match perfectly the rest of the topic from the udp message */
        if (rest_of_topic_without_first_word != after_wildcard_character) {
            // if the rest of the topic is equal to after_wildcard_character, it is a match, otherwise no
            return false;
        }
    }

    return true;
}

/* function that looks for matches after the '*' character and
returns false if there is no match, true otherwise */
bool handle_asterisc_wildcard(string &after_wildcard_character, string &rest_of_topic_without_first_word,
                                string rest_of_topic_udp_msg, char *wildcards, int k, int count)
{
    // check if "*" is on the last position of the string, so it matches everything
    if (after_wildcard_character == "")
        return true;

    // compare remainings until match or until end of word

    rest_of_topic_without_first_word = rest_of_topic_udp_msg;

    // variable that indicates if we have found a match
    bool match = false;

    while (1) {
        // if there are still wildcards in the rest of the string,
        // we just need to make sure that we find a match until the following wildcard
        if (after_wildcard_character.find("+") != string::npos
            || after_wildcard_character.find("*") != string::npos) {
            // check if the string between the wildcards matches the corresponding string from the udp message topic
            string between_wildcards = after_wildcard_character.substr(0,
                                        after_wildcard_character.find(wildcards[k - count + 1]));

            // check if the corresponding string from the udp message topic contains the string between the wildcards
            if (rest_of_topic_without_first_word.find(between_wildcards) == string::npos)
                return false;

            // if we reached this point, it means that we have found a match,
            // so we need to prepare the variables for the next verification
            rest_of_topic_without_first_word = rest_of_topic_without_first_word.
                                                substr(rest_of_topic_without_first_word.find(between_wildcards) + between_wildcards.size(),
                                                rest_of_topic_without_first_word.size());
            after_wildcard_character = after_wildcard_character.
                                        substr(between_wildcards.size(), after_wildcard_character.size());
            match = true;
            break;
        }

        // if we reached this point, it means that there are no more wildcards in the topic

        // check if the entire remaining strings match
        if (rest_of_topic_without_first_word == after_wildcard_character.substr(1, after_wildcard_character.size())) {
            match = true;
            break;
        }

        // if we are here, the strings do not match entirely, so we need to eliminate the
        // first words from the rest of topic progresively until we find a match
    
        rest_of_topic_without_first_word = rest_of_topic_without_first_word.substr(rest_of_topic_without_first_word.find("/"),
                                            rest_of_topic_without_first_word.size());

        string eliminate_slash = rest_of_topic_without_first_word.substr(1, rest_of_topic_without_first_word.size());
        rest_of_topic_without_first_word = eliminate_slash;

        // check if this is the last word in the topic, aka there are no more "/" characters in the string
        if (rest_of_topic_without_first_word.find("/") == string::npos) {
            // compare the remaining string without considering the "/" delimiters
            if (rest_of_topic_without_first_word == after_wildcard_character.substr(1,
                                                    after_wildcard_character.size())) {
                // we have found a match
                match = true;
            }
            break;
        }
    }

    /* if the match variable remained false, it means that the strings do not match
    until this point, so there is no point in continuing with further checks */
    if (match == false)
        return false;

    return true;
}

/* function that compares the topic (taken from the map and containing at
least one wildcard character) with the topic of the udp message (this one does
not contain wildcards) */
bool check_wildcard_match(string topic, string udp_msg_topic_string)
{   
    // here, we will store the sequence of wildcards
    char wildcards[WILDCARDS_MAXLENGTH];

    // identify how many times the '+' and the '*' characters appear in the topic
    int count = get_number_of_wildcards(topic, wildcards);

    string remaining_topic = topic;
    string remaining_topic_udp_msg = udp_msg_topic_string;
    string rest_of_topic_without_first_word;
    int k = count;

    while (count > 0) {
        // check if the 2 strings match until wildcard
        string until_wildcard_character = remaining_topic.
                                            substr(0, remaining_topic.find(wildcards[k - count]));
        string after_wildcard_character = remaining_topic.
                                            substr(remaining_topic.find(wildcards[k - count]) + 1,
                                            remaining_topic.size() - 1);

        // get the first n characters of the current topic, where n = strlen(until_wildcard_character)
        string first_n_characters = remaining_topic_udp_msg.substr(0, until_wildcard_character.size());

        // if the 2 strings do not match until the wildcard, there is no need to continue
        if (first_n_characters != until_wildcard_character) {
            return false;
        }
                
        // store in rest_of_topic the string formed by skipping one word from the current topic
        // check if the rest of the topic is equal to after_wildcard_character

        string rest_of_topic_udp_msg = remaining_topic_udp_msg.substr(until_wildcard_character.size(),
                                        remaining_topic_udp_msg.size());

        // check if this is the last word in the topic
        if (rest_of_topic_udp_msg.find("/") == string::npos) {
            // if there is no more "/", it means that the rest of the topic is the last word
            if (first_n_characters != until_wildcard_character) {
                // if the rest of the topic is not equal to after_wildcard_character, it is not a match
                return false;
            } else {
                return true;
            }
        }

        // check matches after wildcards
        if (wildcards[k - count] == '+') {
            if (!handle_plus_wildcard(rest_of_topic_without_first_word, after_wildcard_character,
                                        rest_of_topic_udp_msg, wildcards, k, count))
                return false;
        } else {
            if (!handle_asterisc_wildcard(after_wildcard_character, rest_of_topic_without_first_word,
                                            rest_of_topic_udp_msg, wildcards, k, count))
                return false;
        }
                    
        remaining_topic = after_wildcard_character;
        remaining_topic_udp_msg = rest_of_topic_without_first_word;
        count --;
    }

    return true;
}

void close_all_sockets(int num_sockets, struct pollfd *poll_fds)
{
    for (int i = 3; i < num_sockets; i++) {
        shutdown(poll_fds[i].fd, SHUT_RDWR);
        close(poll_fds[i].fd);
    }
}

// function that returns just the topic of the udp message
string get_udp_message_topic_string(struct udp_message *msg)
{
    string msg_topic_str;
    for (int k = 0; k < TOPIC_MAXLENGTH && msg->topic[k] != '\0'; k++) {
        msg_topic_str.push_back(msg->topic[k]);
    }
    return msg_topic_str;
}

/* function that sends the message to all the clients that subscribed to the topic
(here are tackled only the topics that match perfectly, the wildcards are not tackled) */
void send_udp_message_to_all_subscribers(string msg_topic_str, char *message, set<string> &sent_to_clients)
{
    for (int j = 0; j < topics[msg_topic_str].size(); j++) {
        if (clients.find(topics[msg_topic_str][j]) != clients.end()) {
            /* once the message is sent to a client, add it to the set of
            clients that received the message so as not to send to the same client twice */
            sent_to_clients.insert(topics[msg_topic_str][j]);

            int rc = send_all(clients[topics[msg_topic_str][j]], message, MSG_MAXSIZE);
            DIE(rc < 0, "Error sending message to client\n");
        }  
    }
}

// function that tackles only the topics containing wildcards
// its purpose is to send the message to the wildcard toics from the map that match the message topic
void send_udp_messages_wildcards(string msg_topic_str, char *message, set<string> &sent_to_clients)
{
    // parse the whole topics map and check if the current topic contains wildcards
    for (auto it = topics.begin(); it != topics.end(); it++) {
        string topic = it->first;
        if (topic.find("+") != string::npos || topic.find("*") != string::npos) {

            if (check_wildcard_match(topic, msg_topic_str)) {
                // match found => send the message to all subscribers who haven't been sent the message yet
                for (int j = 0; j < topics[topic].size(); j++) {
                    if (clients.find(topics[topic][j]) != clients.end()
                        && sent_to_clients.find(topics[topic][j]) == sent_to_clients.end()) {
                        sent_to_clients.insert(topics[topic][j]);
                        int rc = send_all(clients[topics[topic][j]], message, MSG_MAXSIZE);
                        DIE(rc < 0, "Error sending message to client\n");
                    }
                }
            }
        }
                
    }
}

// this function is called when we receive input from the UDP socket
void receive_udp_message(int fd)
{
    struct udp_message *msg = (struct udp_message *)malloc(sizeof(struct udp_message));
    DIE(msg == NULL, "Error allocating memory for udp message\n");

    struct sockaddr_in udp_addr;
    socklen_t udp_socklen = sizeof(udp_addr);
    recvfrom(fd, msg, sizeof(struct udp_message), 0, (sockaddr *)&udp_addr, &udp_socklen);

    char *message = create_message_string(udp_addr, msg); 
    string msg_topic_str = get_udp_message_topic_string(msg);

    /* the message should not be sent to a client more than once, so we will use
    a set that stores all the clients that we have already sent a message to */
    set<string> sent_to_clients;

    // send message to all the clients that subscribed to the topic
    send_udp_message_to_all_subscribers(msg_topic_str, message, sent_to_clients);


    // look for the topics containing wildcards and check if they match the msg_topic_str
    send_udp_messages_wildcards(msg_topic_str, message, sent_to_clients);
}

// function that returns the client id if it is valid, NULL otherwise
char *read_client_id_and_check_validity(int new_client_fd)
{
    char *client_id;
    client_id = (char *)calloc(MSG_MAXSIZE, sizeof(char));
    int rc = recv_all(new_client_fd, client_id, MSG_MAXSIZE);
    DIE(rc < 0, "Error receiving client ID\n");

    // check if the client ID already exists
    if (does_client_ID_already_exist(client_id)) {
        close(new_client_fd);
        return NULL;
    }

    return client_id;
}

/* function that updates the clients map and the poll_fds array
after accepting the connection with a new client */
void add_new_client_to_datastructures(char *client_id, int new_client_fd,
                                        int &num_sockets, struct pollfd *poll_fds)
{
    clients.insert({client_id, new_client_fd});

    // add the new client to the poll_fds
    poll_fds[num_sockets].fd = new_client_fd;
    poll_fds[num_sockets].events = POLLIN;
    num_sockets++;
}

void print_new_connection_confirmation(struct sockaddr_in client_addr, char *client_id)
{
    char *client_ip = inet_ntoa(client_addr.sin_addr);
    uint16_t client_port = ntohs(client_addr.sin_port);
    fprintf(stdout, "New client %s connected from %s:%d.\n", client_id, client_ip, client_port);
}

void accept_new_tcp_connection(struct sockaddr_in &client_addr, socklen_t &client_len, int tcp_listenfd, int &new_client_fd)
{
    client_len = sizeof(client_addr);
    new_client_fd = accept(tcp_listenfd, (struct sockaddr *)&client_addr, &client_len);
    DIE(new_client_fd < 0, "Error accepting connection\n");
}

void add_new_tcp_connection(int tcp_listenfd, int &num_sockets, struct pollfd *poll_fds)
{
    // we received a new connection request on the tcp socket, so we need to accept it
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int new_client_fd;
    accept_new_tcp_connection(client_addr, client_len, tcp_listenfd, new_client_fd);

    char *client_id = read_client_id_and_check_validity(new_client_fd);

    // check if the client_id is NULL, which means that the client_id already exists
    if (client_id == NULL)
        return;

    /* if we have reached this point, it means that the client_id is valid
    and we can add the client to the map */
    add_new_client_to_datastructures(client_id, new_client_fd, num_sockets, poll_fds);

    print_new_connection_confirmation(client_addr, client_id);
}

void update_poll_fds_after_client_disconnection(int i, int &num_sockets, struct pollfd *poll_fds)
{
    // remove the client from the poll_fds
    for (int j = i; j < num_sockets - 1; j++) {
        poll_fds[j] = poll_fds[j + 1];
    }

    num_sockets--;
}

void close_tcp_client_connection(struct pollfd *poll_fds, int i, int &num_sockets)
{
    close(poll_fds[i].fd);

    // search in the clients map for the element that has the value poll_fds[i].fd and remove it
    string client_id = get_client_id_from_sockfd(poll_fds[i].fd);

    cout<<"Client "<<client_id<<" disconnected.\n";
                    
    clients.erase(client_id);

    update_poll_fds_after_client_disconnection(i, num_sockets, poll_fds);
    
}


void identify_command_from_client(struct pollfd *poll_fds, int i, char *buffer)
{
    if (strncmp(buffer, "subscribe", strlen("subscribe")) == 0) {
        subscription_handle(buffer, poll_fds[i].fd);
    } else {
        if (strncmp(buffer, "unsubscribe", strlen("unsubscribe")) == 0)
            unsubscription_handle(buffer, poll_fds[i].fd);
    }
}

void handle_input_received_from_tcp_clients(struct pollfd *poll_fds, int i, int &num_sockets)
{
    char buffer[MSG_MAXSIZE];
    memset(buffer, 0, MSG_MAXSIZE);

    int rc = recv_all(poll_fds[i].fd, buffer, MSG_MAXSIZE);
    DIE(rc < 0, "Error receiving message from client\n");

    if (rc != 0) {
        // decide if this is a subscribe or unsubscribe message
        identify_command_from_client(poll_fds, i, buffer);
    } else {
        // if we are here, it means that the client disconnected
        // so we need to close the connection and remove the client from the map
        close_tcp_client_connection(poll_fds, i, num_sockets);
    }
}


void initialize_poll_fd(int index, struct pollfd *poll_fds, int fd)
{
    poll_fds[index].fd = fd;
    poll_fds[index].events = POLLIN;
}

void initialize_poll_fds(struct pollfd *poll_fds, int tcp_listenfd, int udp_listenfd)
{
    initialize_poll_fd(0, poll_fds, udp_listenfd);
    initialize_poll_fd(1, poll_fds, tcp_listenfd);
    initialize_poll_fd(2, poll_fds, STDIN_FILENO);
}

void run_server_multi_chat(int tcp_listenfd, int udp_listenfd)
{
    int rc = listen(tcp_listenfd, MAX_CONNECTIONS);
    DIE(rc < 0, "Error listening for TCP connections\n");

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_sockets = 3;

    // set the poll_fds for the 3 file descriptors:
    // 0: udp connections
    // 1: tcp connections
    // 2: stdin (in case we receive the exit command)

    initialize_poll_fds(poll_fds, tcp_listenfd, udp_listenfd);

    while (1) {
        rc = poll(poll_fds, num_sockets, -1);
        DIE(rc < 0, "poll failed\n");

        // identify the file descriptor that has events

        // check if we have received input from the UDP socket
        if (poll_fds[0].revents & POLLIN) {
            receive_udp_message(poll_fds[0].fd);
            continue;
        }

        // check if we have received input from the TCP socket
        if (poll_fds[1].revents & POLLIN) {
            add_new_tcp_connection(tcp_listenfd, num_sockets, poll_fds);
            continue;
            
        }

        // check if we have received input from stdin
        if (poll_fds[2].revents & POLLIN) {
            if (!need_to_exit()) {
            // we received something else than the "exit" command
            fprintf(stderr, "Error: EXIT is the only command accepted for the server\n");
            continue;
            }

            close_all_sockets(num_sockets, poll_fds);
            return;      
        }

        // check if we have received input from any of the sockets communicating with the tcp clients
        for (int i = 3 ; i < num_sockets; i++) {
            if (poll_fds[i].revents & POLLIN) {
                // check if we have received input from the clients through the TCP sockets
                handle_input_received_from_tcp_clients(poll_fds, i, num_sockets);
            }
        }
    }
    
}

// this function has been inspired from the 7th lab
int main(int argc, char *argv[])
{
    // check arguments
    if (argc != 2) {
    printf("\n Usage: %s <ip> <port>\n", argv[0]);
    return -1;
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // parse server port as a number
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // We manage both TCP and UDP clients, so we need two sockets, one for each type

    // Create the TCP socket
    const int tcp_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_listenfd < 0, "Error creating socket listening for TCP connections");

    // Create the UDP socket
    const int udp_listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_listenfd < 0, "Error creating socket listening for UDP connections");

    /* Update the 2 serv_addr structures with the address of the server,
    the address family and the port for connection */
    struct sockaddr_in tcp_serv_addr;
    tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;

    struct sockaddr_in udp_serv_addr;
    udp_serv_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t socket_len = sizeof(struct sockaddr_in);
    
    memset(&tcp_serv_addr, 0, socket_len);
    tcp_serv_addr.sin_family = AF_INET;
    tcp_serv_addr.sin_port = htons(port);

    memset(&udp_serv_addr, 0, socket_len);
    udp_serv_addr.sin_family = AF_INET;
    udp_serv_addr.sin_port = htons(port);

    // Associate the address of the server with the created sockets using bind
    rc = bind(tcp_listenfd, (const struct sockaddr *)&tcp_serv_addr, sizeof(tcp_serv_addr));
    DIE(rc < 0, "Failed to bind tcp socket\n");
    rc = bind(udp_listenfd, (const struct sockaddr *)&udp_serv_addr, sizeof(udp_serv_addr));
    DIE(rc < 0, "Failed to bind udp socket\n");

    run_server_multi_chat(tcp_listenfd, udp_listenfd);

    close(tcp_listenfd);
    close(udp_listenfd);

    return 0;
    
}