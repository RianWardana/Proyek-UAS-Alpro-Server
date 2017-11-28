#include<stdio.h>
#include<string.h>
#include<stdlib.h>

// Untuk server
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

// Untuk akses ke database MYSQL
#include <my_global.h>
#include <mysql.h>

// Untuk fitur multithreading (-lpthread)
#include<pthread.h>
void *connection_handler(void *);

// int msgCount = 0;
// int *msgCountPtr = &msgCount;

void finish_with_error(MYSQL *con) {
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);        
}

int main(int argc, char *argv[]) {
    int socket_desc, client_sock, c, *new_sock;
    struct sockaddr_in server, client;
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8080);
     
    //Bind
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
        puts("Connection accepted");
         
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;
         
        if( pthread_create(&sniffer_thread, NULL,  connection_handler, (void*) new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
         
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }
     
    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
     
    return 0;
}
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc) {
        

    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int recv_size;
    char *message, client_message[16];
    char *query = malloc(64);

    //Receive a message from client
    while( (recv_size = recv(sock, client_message, 16, 0)) > 0 ) {
        // Koneksi dengan database MYSQL
        MYSQL *con = mysql_init(NULL);
        if (mysql_real_connect(con, "localhost", "admin", "adminpass", "alpro", 0, NULL, 0) == NULL) {
            finish_with_error(con);
        }

        sprintf(query, "SELECT * FROM msg WHERE `Id` = 1;");

        if (mysql_query(con, query))  {
            finish_with_error(con);
        }

        MYSQL_RES *result = mysql_store_result(con);

        if (result == NULL) {
            finish_with_error(con);
        }

        int num_fields = mysql_num_fields(result);
        MYSQL_ROW row;

        int i;
        while ((row = mysql_fetch_row(result))) { 
            for(i = 0; i < num_fields; i++) { 
                printf("%s ", row[i] ? row[i] : "NULL"); 
            } 
            printf("\n"); 
        }

        mysql_free_result(result);
        mysql_close(con);

        write(sock, row[0], strlen(client_message));
    }

    if(recv_size == 0) {
        puts("Client disconnected");
        fflush(stdout);
    }

    else if(recv_size == -1) {
        perror("recv failed");
    }
         
    //Free the socket pointer
    free(socket_desc);
    
	close(sock);
    return 0;
}
