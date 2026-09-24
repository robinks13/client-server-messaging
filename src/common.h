#ifndef COMMON_H
#define COMMON_H

#define MSG_LEN 1024
#define SERV_PORT "8080"
#define SERV_ADDR "127.0.0.1"
#define BLOC_SIZE 4096

#include "msg_struct.h"
#include <time.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <dirent.h>

struct info{
    short s;
    long l;
};

void die(int status, char* str);

int read_message(int* sockfd, struct message* msg_infos, char* buf);

void write_message(int* sockfd, struct message msg_infos, char* buf);

int write_file(int* socketfd, char* filename);

char* findfilename(char* path);

int read_file_for_serv(int* sockfd, char* filename, char* pseudo);

int read_file_for_client(int* sockfd, char* filename);

void give_files_list(char* buf);

int is_file_available(char* filename);

typedef struct client_node {
    char ip[16];       
    int port;              
    int fd;
	char pseudo[NICK_LEN];
	char password[PWD_LEN];
	int is_connected;
	char logintime[100];
    struct client_node *next; 
	char history[NICK_LEN];
} client_node;

client_node* init_node();

void add_node(client_node* n, char ip[], int port, int fd);

char* create_user_file(char* pseudo, char* filename);

void add_info_to_node(client_node* n, int fd, char* pseudo, char* password, char* logintime);

void change_pseudo(client_node* n, int fd, char* pseudo);

int pseudo_is_in_node(client_node* n, char* pseudo, char* password_ref, int* is_connected, char* history);

client_node* del_node(client_node* n, char* pseudo);

client_node* deconnect_node(client_node* n, int fd);

void free_node(client_node* n);

char* concat_nick(client_node* n);

char* get_time(char* buf, int sizeofbuf);

client_node* find_node_withbsn(client_node* n, char* pseudo);

client_node* find_node(client_node* n, char* pseudo);

client_node* find_node_withfd(client_node* n, int fd);

void give_all_active_fd(client_node* n, int* fds);

int give_fd_from_pseudo(client_node* n, char* pseudo);

int filetxt_to_buf(char* filename, char buf[]);

typedef struct ban_usr {
	char pseudo[NICK_LEN];
    struct ban_usr *next; 
} ban_usr;

ban_usr* add_ban_usr(ban_usr* n, char* pseudo);

int is_usr_ban(ban_usr* n, char* pseudo);

void free_ban_usr(ban_usr* n);

#endif