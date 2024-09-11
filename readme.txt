STEFAN MIRUNA ANDREEA 324CA

HOMEWORK 2 - TCP AND UDP CLIENT-SERVER APP FOR MESSAGES MANAGEMENT

Note: I have used the 7th lab framework.

Files:
- common.cpp and common.hpp -> only contain 2 functions, recv_all and send_all,
implemented as in the 7th lab. These functions receive / send info byte by byte
in order to avoid the unexpected behaviour of the recv() and send() functions,
that could possibly lead to unwanted concatenations or truncations.

- helpers.hpp -> contains the DIE macro for error handilng

- subscriber.cpp
	-> contains the client implementation
	
	-> in int main(), I also followed the flow from the 7th lab, as follows:
	after making sure that the number of arguments is correct, we need to 
	create the TCP socket and set the fields of the sockaddr_in structure.
	Next step is to connect the client's newly created socket to the server
	and send to the server the first message: the id of the client. Then, 
	we call the run_client function, that does most of the job basically 
	and, of course, we do not forget to close the socket at the end;

	->the run_client function processes the messages from the socket 
	comunicating with the server or from stdin. Therefore, it uses the poll
	function, used for multiplexing I/O. As our input can come from 2 
	sources, we will only have 2 elements in the poll_fds array. 0 will 
	correspond to the socket communicating with the server and 1 to stdin.
	If events are triggered on poll_fds[0], it means that we have received 
	a message from server, namely a char* obtained after processing in the 
	server the message received from udp. If the events on poll_fds[1] are 
	triggered, we need to process a command from stdin. The only accepted 
	commands are "exit", "subscribe" and "unsubscribe". If the command is 
	"exit", we need to close the socket so as to end connection. Otherwise,
	we need to send the subscription or unsubscription message to the 
	server.

	Also, we check if the format of the subscribe/unsubscribe commands is 
	correct: "subscribe <topic>". If the first word is different from 
	"subscribe" or "unsubscribe" or the topic is omitted, the command will 
	be considered invalid.
	
- server.cpp
	-> here is concentrated the main logic of the app

	-> the int main() function is also inpired from the 7th lab and (after
	 checking the number of arguments), it basically creates 2 sockets, one
	 that is going to be listening for tcp connections and the other one 
	 for udp connections, populates the sockaddr_in structures for each 
	 socket and binds the address of the server with the sockets 
	 filedescriptors. Then, the function run_server_multi_chat() is called
	 and, of course, after returning from this function, we close both 
	 sockets.

	 -> the run_server_multi_chat() function concentrates the main flow of 
	 the program, as it contains the logic for the poll() function, that 
	 we use so as to make the app more efficient, by avoiding blocking it 
	 when waiting to receive input from a specific socket. The poll 
	 function will be watching over all sockets of the server and will be 
	 triggered whenever we receive input on one of the sockets.

	 The first 3 elements of the poll_fds array will be more special:
	 
	 	- 0: for the udp socket (poll_fds[0] revents will be triggered
	 	when we receive a message from the an udp client. This will be 
	 	handled by the receive_udp_message() function.)
	 	
	 	- 1: for the TCP socket (poll_fds[1] revents will be triggered
	 	when a new tcp client is trying to connect with the server. 
	 	This will be handled by the add_new_tcp_connection() function.)

	 	- 2: for stdin (poll_fds[2] revents will be triggered when we 
	 	receive input form stdin. The only command accepted by the 
	 	server is "exit", which will lead to closing all sockets. If 
	 	the command received from keyboard does not match with "exit", 
	 	it is considered invalid and will receive an error message.)

	 	- the sockets from 3 until the end of the array are the ones 
	 	corresponding to each tcp client connected to the server. They 
	 	will be triggered when one of the clients is trying to 
	 	communicate something to the server. This situation will be 
	 	handled by the handle_input_received_from_tcp_clients() 
	 	function.

	In order to keep track of the tcp clients that are communicating with 
	the server, we will use a map called "clients", that will make the 
	association between the client_id and the file descriptor of the socket
	communicating with that client. We cannot have two clients with the 
	same id, so each time a new tcp client is trying to connect to the 
	server, we will check if that client's id already exists in the map. In
	this case, we will give an error message and the connection will be 
	refused.

	The second map that we will keep is "topics", which will make the 
	association between a string representing the topic (this is the key) 
	and a vector of strings representing the ids of the clients that have 
	subscribed to that topic (this is the value).
	
	When a new client is trying to connect to the server, poll_fds[1] is 
	triggered and the function add_new_tcp_connection() is called. Here, we
	accept the new connection and check if the client id already exists. If
	it does, we close the connection, else we proceed to adding the new 
	client in the clients map and updating the poll_fds array.
	
	If we receive input from a client (indexes from 3 to the last one in the
	poll_fds array), the function handle_input_received_from_tcp_clients() 
	is called. Here, we receive the message and check how many bytes we 
	have recived. If 0, the client has disconnected and we need to close 
	the connection with it and eliminate it from the clients map and from 
	poll_fds. If != 0, it means that we have received a subscribe or 
	unsubscribe command, so we call the subscription_handle() or 
	unsubscription_handle() function accordingly.

	If the command was "subscribe <topic>", we need to identify the string 
	topic, which is the last word in the command, and look for it in the 
	topics map. If it already exists, just add the client id in the array 
	of subscribers to that topic. If it does not exist yet, add it as a new
	entry in the topics map.
	If the command was "unsubscribe <topic>", we will do the exact opposite
	thing. We need to identify the string topic, look for it in the topics 
	map and remove the client from the subscribers list.
	
	If poll_fds[0] is triggered, it means that we have received udp message
	so we call the receive_udp_message() function. Here, we receive an udp 
	message (we have a structure for it) and we need to transform this 
	message into a char* according to the tcp message table in the 
	homework's statement. This char* message will be crated by the 
	create_message_string() function. This message must have the format 
	ip_address:port - <topic> - data_type - message_value. So we will first
	allocate memory for the message, add the ip address and the port and 
	then we will continue populating the message according to the data type.
	There will be a separate function for each data type.
	
	After finishing populating the message, we need to send it to the tcp 
	clients. We must not send the same message to a client more than once, 
	so we will use a set "sent_to_clients", that will store the ids of the 
	clients that we have already sent the message to, so as to know not to 
	send them the message again. After sending the message to a client, we 
	immediately add it to the set and we do not send any message without 
	first checking if the message has already been sent to that client.
	
	First and foremost, we use the send_udp_message_to_all_subscribers() 
	function to look for the udp message topic in the topics map and send 
	the handled message to all clients in the topic's subscribers list.
	
	Then, we need to handle a more special category of topics, namely the 
	ones containing wildcards. The send_udp_messages_wildcards() function 
	will parse each topic stored in the topics map and check if it contains
	any wildcard. If no, ignore it because if it had matched the udp message
	topic, we would have already sent the message to its clients in the 
	previous send_udp_message_to_all_subscribers() function. If it contains
	at least a wildcard character, we need to check if it matches the udp 
	message topic using the check_wildcard_match() function and if it is a 
	match, send the message to the clients.
	
	The boolean check_wildcard_match() receives as parameters the current 
	topic in the topics map (this is the one containing wildcards) and the
	topic of the udp message (this one does not contain any wildcard). The 
	function will return true if the 2 strings match and false otherwise. 
	We will keep the sequence of wildcards in the char array "wildcards" 
	and we will count how many times the '*' or the '+' characters appear 
	in the topic string. As long as there are still wildcards in the topic,
	we will engage in the following logic: We will check if the strings 
	match up until meeting the first wildcard character. If they don't, 
	there is no need to continue, as we are sure that the 2 do not match, 
	so return false. Otherwise, we need to check if they match from further
	on (from the wildcard character until the end). So, now that the parts
	that have matched are removed, we need to check if the rests match. For
	that, after removing the first word in the remaining string from the 
	topic (this is the one corresponding to the wildcard) we need to check 
	if there are still "/" delimiters in the remaining string. If not, this
	is the last word and we need to look for a perfect match. If they do 
	not match, return false. If there are still other words in the 
	remaining string, we need to separate the logic according to the type 
	of the wildcard that we are tackling. If it is '+', call the 
	handle_plus_wildcard() function and if it is '*', call the 
	handle_asterisc_wildcard() function. The 2 functions only differ 
	because the '+' wildcard can be replaced by only one word and the '*' 
	wildcard by many words. Otherwise, both of them are checking if the 
	remaining strings match until the next wildcard or, if there are no 
	more wildcards, until the end. If any of these functions returns false,
	then we have found a mismatch, so there is no point in proceeding with
	the verifications and we also return false (in the 
	check_wildcard_match() function). Otherwise, we continue with the 
	same logic until we have tackled all the wildcards in the topic string.
	
