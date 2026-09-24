#include "msg_struct.h"
#include "common.h"

void die(int status, char* str)
{
    if (status == -1)
    {
        perror(str);
        exit(EXIT_FAILURE);
    }
}

int read_message(int* sockfd, struct message* msg_infos, char* buf){
	int ret;
	// read message size
	int nb_to_read = sizeof(struct message);
	int nb_received = 0;
	while(nb_to_read != nb_received){
		ret = read(*sockfd, (char *)msg_infos + nb_received, nb_to_read - nb_received);
		if (ret == 0) {
			printf("client fd=%d disconnected while reading message\n", *sockfd);
			return 0;
		}
		die(ret, "on read size of message in echo function\n");
		nb_received += ret; 
	}

	// read new message
	nb_received = 0;
	int msg_size = msg_infos->pld_len;
	while(msg_size != nb_received){
		ret = read(*sockfd, buf + nb_received, msg_size - nb_received);
		die(ret, "on read client message in echo function\n");
		nb_received += ret;  
	}
	//printf("Received !\n");
	return msg_infos->pld_len == 0 ? -1 : nb_received;
}

void write_message(int* sockfd, struct message msg_infos, char* buf){
	int ret;
	// write message size
	int next_msg_size = msg_infos.pld_len;
	int nb_to_write = sizeof(struct message);
	int nb_send = 0;
	while(nb_to_write != nb_send){
		ret = write(*sockfd, (char *)&msg_infos + nb_send, nb_to_write - nb_send);
		if (ret == 0) {
			printf("client fd=%d disconnected while reading message\n", *sockfd);
			close(*sockfd);
			*sockfd = -1;
			break;
		}
		die(ret, "on write size of message\n");
		nb_send += ret; 
	}
	

	// Write new message
	nb_send = 0;
	while(next_msg_size != nb_send){
		ret = write(*sockfd, buf + nb_send, next_msg_size - nb_send);
		die(ret, "on write client message\n");
		nb_send += ret;  
	}
	//printf("Message sent!\n");
}

int write_file(int* socketfd, char* filename){
	FILE* fp = fopen(filename, "rb");

	struct stat info;
	stat(filename, &info);

	int sizeoffile = info.st_size;
	write(*socketfd, &sizeoffile, sizeof(int));

	char buf[BLOC_SIZE];
	int next_msg_size;
	while((next_msg_size = fread(buf, 1, BLOC_SIZE, fp)) > 0){
		int nb_send = 0;
		while(next_msg_size != nb_send){
			int ret = write(*socketfd, buf + nb_send, next_msg_size - nb_send);
			if(ret == -1){
				return 0;
			}
			nb_send += ret;  
		}
	}
	fclose(fp);
	return 1;
}

char* findfilename(char* path){
	char* last_slash = strrchr(path, '/');
	if(last_slash != NULL){
		return last_slash + 1;
	}else{
		return path;
	}
}

int read_file_for_serv(int* sockfd, char* filename, char* pseudo){
	int sizeoffile;
	read(*sockfd, &sizeoffile, sizeof(int));
	char filepath[400];
	char realfilename[300];
	strcpy(realfilename, findfilename(filename));
	sprintf(filepath, "files/%s_%s", pseudo, realfilename);
	// On vérifie que le fichier n'existe pas déja
	if (access(filepath, F_OK) == 0)
	{
		return -1;
	}
	
	FILE* fp = fopen(filepath, "wb");
	if(!fp){
		return 0;
	}
	char buf[BLOC_SIZE];
	while (sizeoffile > 0){
		int to_read;
		if(sizeoffile > BLOC_SIZE){
			to_read = BLOC_SIZE;
		}else{
			to_read = sizeoffile;
		}
		int alrdread = 0;
		while(alrdread != to_read){
			int ret = read(*sockfd, buf + alrdread, to_read - alrdread);
			if(ret == -1){
				return 0;
			}
			alrdread += ret;
		}
		fwrite(buf, 1, to_read, fp);
		sizeoffile -= to_read;
	}
	fclose(fp);
	return 1;
}

int read_file_for_client(int* sockfd, char* filename){
	int sizeoffile;
	read(*sockfd, &sizeoffile, sizeof(int));
	
	char filepath[400];
	sprintf(filepath, "downloads/%s", filename);
	FILE* fp = fopen(filepath, "wb");
	if(!fp){
		return 0;
	}
	char buf[BLOC_SIZE];
	while (sizeoffile > 0){
		int to_read;
		if(sizeoffile > BLOC_SIZE){
			to_read = BLOC_SIZE;
		}else{
			to_read = sizeoffile;
		}
		int alrdread = 0;
		while(alrdread != to_read){
			int ret = read(*sockfd, buf + alrdread, to_read - alrdread);
			if(ret == -1){
				return 0;
			}
			alrdread += ret;
		}
		fwrite(buf, 1, to_read, fp);
		sizeoffile -= to_read;
	}
	fclose(fp);
	return 1;
}

void give_files_list(char* buf)
{
	DIR* dir = opendir("files");

	buf[0] = '\0';
	struct dirent* dirent = readdir(dir);
	while (dirent != NULL)
	{
		if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
		{
			dirent = readdir(dir);
			continue;
		}
		
		strcat(buf, " - ");
		strcat(buf, dirent->d_name);
		strcat(buf, "\n");

		dirent = readdir(dir);
	}
	
	closedir(dir);
}

int is_file_available(char* filename)
{
	DIR* dir = opendir("files");
	filename[strlen(filename) - 1] = '\0';
	struct dirent* dirent = readdir(dir);
	while (dirent != NULL)
	{
		if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
		{
			dirent = readdir(dir);
			continue;
		}

		if (strcmp(dirent->d_name, filename) == 0)
		{
			return 1;
		}

		dirent = readdir(dir);
	}
	
	closedir(dir);
	return 0;
}

client_node* init_node(){
	client_node* n = malloc(sizeof(client_node));
	n->next = NULL;
	return n;
}

void add_node(client_node* n, char ip[], int port, int fd){
	client_node* tmp = n;
	while (tmp->next != NULL)
	{
		tmp = tmp->next;
	}

	client_node* new_n = malloc(sizeof(client_node));
	tmp->next = new_n;
	for (int i = 0; i < 15; i++)
	{
		new_n->ip[i] = ip[i];
	}	
	new_n->port = port;
	new_n->fd = fd;
	new_n->next = NULL;
	new_n->is_connected = 1;
}

char* create_user_file(char* pseudo, char* filename){
    sprintf(filename, "history/%s.txt", pseudo);
	if(access(filename, F_OK) != 0){
		FILE* fp = fopen(filename, "w+");
		if (fp == NULL){
			perror("Erreur lors de la création du fichier utilisateur");
    	}
		fclose(fp);
	}
	return filename;
}

void rename_user_file(char* old_pseudo, char* new_pseudo){
	char old_filename[140];
	char new_filename[140];
	sprintf(old_filename, "history/%s", old_pseudo);
	sprintf(new_filename, "history/%s", new_pseudo);
	rename(old_filename, new_filename);
}

void add_info_to_node(client_node* n, int fd, char* pseudo, char* password, char* logintime){
	client_node* tmp = n;
	while (tmp != NULL && tmp->fd != fd)
	{
		tmp = tmp->next;
		if (tmp == NULL){
			perror("add_info_to_node : Erreur fd non présent dans la chaine");
			return ;
		}
	}
	strcpy(tmp->pseudo, pseudo);
	strcpy(tmp->password, password);
	strcpy(tmp->logintime, logintime);
}

void change_pseudo(client_node* n, int fd, char* pseudo){
	client_node* tmp = n;
	while (tmp->fd != fd)
	{
		tmp = tmp->next;
		if (tmp == NULL){
			perror("change_pseudo : Erreur fd non présent dans la chaine");
			return ;
		}
	}
	strcpy(tmp->pseudo, pseudo);
}

int pseudo_is_in_node(client_node* n, char* pseudo, char* password_ref, int* is_connected, char* history){
	client_node* tmp = n;
	while (tmp != NULL)
	{
		if (strcmp(tmp->pseudo, pseudo) == 0)
		{
			strcpy(password_ref, tmp->password);
			*is_connected = tmp->is_connected;
			strcpy(history, tmp->history);
			return 1;
		}
		
		tmp = tmp->next;
	}
	return 0;
}

client_node* del_node(client_node* n, char* pseudo){
	client_node* tmp = n;
	client_node* prec = NULL;
	while (tmp != NULL && (strcmp(tmp->pseudo, pseudo) != 0))
	{	
		prec = tmp;
		tmp = tmp->next;
	}

	if (tmp == NULL){
		perror("del_node : pseudo introuvable dans la chaine\n");
		exit(EXIT_FAILURE);
	}

	if (prec == NULL)
	{
		n = tmp->next;
	}
	else
	{
		prec->next = tmp->next;
	}
	
	free(tmp);
	return n;
}

client_node* deconnect_node(client_node* n, int fd){
	client_node* tmp = n;
	while (tmp != NULL && tmp->fd != fd)
	{	
		tmp = tmp->next;
	}
	
	if (tmp == NULL){
		perror("deconnect_node : fd introuvable dans la chaine\n");
		exit(EXIT_FAILURE);
	}

	close(tmp->fd);
	tmp->fd = -1;
	tmp->port = -1;
	tmp->is_connected = 0;

	return n;
}

void free_node(client_node* n){
	client_node* tmp = n;
	client_node* prec;

	while (tmp != NULL)
	{
		prec = tmp;
		tmp = tmp->next;
		free(prec);
	}	
}

char* concat_nick(client_node* n){
	if (n == NULL){
		return NULL;
	}

	int len_of_res = 0;
	client_node* tmp = n;
	while(tmp != NULL){
		if (tmp->is_connected)
		{
			len_of_res += strlen(tmp->pseudo) + 3;
			tmp = tmp->next;
		}
		else
		{
			len_of_res += strlen(tmp->pseudo) + 3 + 16;
			tmp = tmp->next;
		}
		
	}

	char* res = malloc(len_of_res + 1);	
	res[0] = '\0';
	tmp = n;
	while(tmp != NULL){
		strcat(res, "- ");
		strcat(res, tmp->pseudo);

		if (tmp->is_connected)
		{
			strcat(res, "\n");
		}
		else
		{
			strcat(res, " (not connected)\n");
		}

		tmp = tmp->next;
	}
	// si temps attention 1024 char par mess
	return res;
}

char* get_time(char* buf, int sizeofbuf){
	time_t now;
	struct tm* timeinfo;
	time(&now);
	timeinfo = localtime(&now);
	strftime(buf, sizeofbuf, "%Y/%m/%d@%H:%M", timeinfo);
	return buf;
}

client_node* find_node_withbsn(client_node* n, char* pseudo){
	client_node* tmp = n;
	while(tmp != NULL){
		int len = strlen(tmp->pseudo);
    	char *copieavbsn = malloc(len + 2); // +2 : '\n' + '\0'
    	strcpy(copieavbsn, tmp->pseudo);
		copieavbsn[len] = '\n';
		copieavbsn[len + 1] = '\0';
		if(strcmp(copieavbsn, pseudo) == 0){
			return tmp;
		}
		tmp = tmp->next;
		free(copieavbsn);
	}
	return NULL;
}

client_node* find_node(client_node* n, char* pseudo){
	client_node* tmp = n;
	while(tmp != NULL){
		if(strcmp(tmp->pseudo, pseudo) == 0){
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

client_node* find_node_withfd(client_node* n, int fd){
	client_node* tmp = n;
	while(tmp != NULL){
		if(tmp->fd == fd){
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

void give_all_active_fd(client_node* n, int* fds){
	client_node* tmp = n;
	int i = 0;
	while (tmp != NULL)
	{
		if (tmp->is_connected)
		{
			fds[i] = tmp->fd;
			i++;
		}
		tmp = tmp->next;
	}
	fds[i] = -1;
}

int give_fd_from_pseudo(client_node* n, char* pseudo){
	client_node* tmp = n;
	while (tmp != NULL)
	{
		if (strcmp(tmp->pseudo, pseudo) == 0)
		{
			return tmp->fd;
		}
		
		tmp = tmp->next;
	}
	return -1;
}

int filetxt_to_buf(char* filename, char buf[]){
	FILE* fp = fopen(filename, "r");
	buf[0] = '\0';
	int pos = 0;
	char line[MSG_LEN];
	while(fgets(line, MSG_LEN, fp) != NULL){
		int lenofline = strlen(line);
		if(pos + lenofline + 1 > MSG_LEN){
			return 0;
		}
		strcat(buf, line);
		pos += lenofline;
	}
	fclose(fp);
	return 1;
}

ban_usr* add_ban_usr(ban_usr* n, char* pseudo){
	ban_usr* new_n = malloc(sizeof(ban_usr));
	strcpy(new_n->pseudo, pseudo);
	new_n->next = n;

	return new_n;
}

int is_usr_ban(ban_usr* n, char* pseudo){
	ban_usr* tmp = n;
	while ( tmp != NULL )
	{
		if (strcmp(tmp->pseudo, pseudo) == 0)
		{
			return 1;
		}
		
		tmp = tmp->next;
	}
	
	return 0;
}

void free_ban_usr(ban_usr* n){
	ban_usr* tmp = n;
	ban_usr* prec;

	while (tmp != NULL)
	{
		prec = tmp;
		tmp = tmp->next;
		free(prec);
	}	
}
