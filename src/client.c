#include "common.h"
#include "msg_struct.h"

int handle_connect(char *serv_addr, char *serv_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(serv_addr, serv_port, &hints, &result) != 0) {
		perror("getaddrinfo()");

		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

void copy_pseudo_from_command(char* buf, char* pseudo)
{
	int i = 0;
	while (buf[i] != ' ' && buf[i] != '\0' && buf[i] != '\n')
	{
		pseudo[i] = buf[i];
		i++;
	}
	pseudo[i] = '\0';
}

void echo_client(struct pollfd sock_array[]) {
	char buff_rcv[MSG_LEN];
	char buff_send[MSG_LEN];
	int msg_to_send = 0;
	char pseudo[NICK_LEN] = "";

	while (1) {
		int ret = poll(sock_array, 2, -1);
		die(ret, "poll");

		if ((sock_array[0].revents & POLLIN) == POLLIN && (msg_to_send == 0))
		{
			// Cleaning memory
			memset(buff_send, 0, MSG_LEN);
			// Gettingstruct message from client
			int n = 0;
			while ((buff_send[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
			msg_to_send = 1;
		}
		if ((sock_array[1].revents & POLLIN) == POLLIN)
		{
			// Cleaning memory
			memset(buff_rcv, 0, MSG_LEN);
			// Receiving message
			struct message msg_infos;
			int nb_byte_rcv = read_message(&sock_array[1].fd, &msg_infos, buff_rcv);
			if (nb_byte_rcv == 0)
			{
				return;
			}
			
			if (msg_infos.type == LOGIN || msg_infos.type == LOGINOK || msg_infos.type == LOGIN_FAIL || msg_infos.type == NICKNAME_NEW)
			{
				printf("%s\n", buff_rcv);
				strcpy(pseudo, msg_infos.infos);
			} else if (msg_infos.type == YOU_ARE_BAN)
			{
				printf("%s\n", buff_rcv);
				exit(EXIT_SUCCESS);
			
			} else if (msg_infos.type == FILE_DOWNLOAD)
			{
				printf("%s\n", buff_rcv);
				
				if (strstr(buff_rcv, "Envoie") != NULL)
				{
					read_file_for_client( &sock_array[1].fd, msg_infos.infos);	
				}
			} else if (msg_infos.type == FILE_REQUEST)
			{
				printf("%s\n", buff_rcv);
				printf("Voulez-vous acceptez ce fichier ? (y/n)\n");

				memset(buff_send, 0, MSG_LEN);
				int n = 0;
				while ((buff_send[n++] = getchar()) != '\n') {}

				msg_infos.pld_len = 0;
				strcpy(msg_infos.nick_sender, pseudo);
				if (strcmp(buff_send, "y") == 0)
				{
					msg_infos.type = FILE_ACCEPT;
				}
				else
				{
					msg_infos.type = FILE_REJECT;
				}
				//sprintf(msg_infos.infos, "%s:%d", ip, port);
				
			} else {
				printf("%s\n",buff_rcv);
			}
		}
		else if ((sock_array[1].revents & POLLOUT) == POLLOUT && (msg_to_send == 1))
		{	
			struct message msg_infos;
			msg_infos.pld_len = strlen(buff_send);
			strcpy(msg_infos.nick_sender, pseudo);
			msg_infos.type = -1;
			strcpy(msg_infos.infos, "");

			if (strncmp(buff_send, "/login ", 7) == 0)
			{
				msg_infos.type = LOGIN;
				copy_pseudo_from_command(buff_send + 7, msg_infos.infos);
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if (strncmp(buff_send, "/nick ", 6) == 0){
				msg_infos.type = NICKNAME_NEW;
				strcpy(msg_infos.infos, buff_send + 6);
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if(strcmp(buff_send , "/who\n") == 0){
				msg_infos.type = NICKNAME_LIST;
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if(strncmp(buff_send , "/whois ", 7) == 0){
				msg_infos.type = NICKNAME_INFOS;
				strcpy(msg_infos.infos, buff_send + 7);
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if(strncmp(buff_send , "/msgall ", 8) == 0){
				msg_infos.type = BROADCAST_SEND;
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if(strncmp(buff_send , "/msg ", 5) == 0){
				msg_infos.type = UNICAST_SEND;
				copy_pseudo_from_command(buff_send + 5, msg_infos.infos);
				write_message(&sock_array[1].fd, msg_infos, buff_send);
				
			} else if(strncmp(buff_send , "/sendfile ", 10) == 0){
				char filename[NICK_LEN];
				strcpy(filename, buff_send + 10);
				int len = strlen(filename);
				filename[len -1] = '\0';
				FILE* fp = fopen(filename, "rb");
				if (fp == NULL){
					perror("[Client] ERREUR: Impossible d'ouvrir le fichier");
					msg_to_send = 0;
					continue;
				}
				fclose(fp);

				struct stat info;
				stat(filename, &info);

				int sizeoffile = info.st_size;
				if (sizeoffile > 1000000000){
					printf("[Client] Fichier trop volumineux\n");
					msg_to_send = 0;
					continue;
				}
				msg_infos.type = FILE_UPLOAD;
				msg_infos.pld_len =  strlen(filename);
				write_message(&sock_array[1].fd, msg_infos, filename);
				printf("[Client] Envoi du fichier %s au serveur...\n", filename);

				if (write_file(&sock_array[1].fd, filename)){
					printf("[Client] Données du fichier envoyées\n");
				} else {
					printf("[Client] Echec de l'envoi\n");
				}

			} else if(strncmp(buff_send , "/listfiles", 10) == 0){
				msg_infos.type = FILE_LIST;
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if(strncmp(buff_send , "/getfile ", 9) == 0){
				msg_infos.type = FILE_DOWNLOAD;
				write_message(&sock_array[1].fd, msg_infos, buff_send);

			} else if(strncmp(buff_send , "/ban ", 5) == 0){
				msg_infos.type = BAN_USER;
				write_message(&sock_array[1].fd, msg_infos, buff_send);
				
			} else if(strncmp(buff_send , "/send ", 6) == 0){
				copy_pseudo_from_command(buff_send + 6, msg_infos.infos);
				msg_infos.type = FILE_REQUEST;
				write_message(&sock_array[1].fd, msg_infos, buff_send);
			} else if(strcmp(buff_send , "/listcmd\n") == 0){
				msg_infos.type = CMD_LIST;
				write_message(&sock_array[1].fd, msg_infos, buff_send);
			}else if(strcmp(buff_send , "/quit\n") == 0){
				write_message(&sock_array[1].fd, msg_infos, buff_send);
				close(sock_array[1].fd);
				//liberé la mémoire allouée aux structures de données
				exit(EXIT_SUCCESS);
			}else{
				msg_infos.type = ECHO_SEND;
				write_message(&sock_array[1].fd, msg_infos, buff_send);
			}
			msg_to_send = 0;
		}
	}
}



int main(int argc, char *argv[]) {
	if (argc != 3)
	{
		printf("Mauvais nombres d'arguments :\n%s <server_name> <server_port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	int sfd;
	sfd = handle_connect(argv[1], argv[2]);
	printf("Connecting to server ... done !\n");

	struct pollfd sock_array[2];
	sock_array[0].fd = 0;
	sock_array[0].events = POLLIN;
	sock_array[0].revents = 0;
	sock_array[1].fd = sfd;
	sock_array[1].events = POLLIN | POLLOUT;
	sock_array[1].revents = 0;

	echo_client(sock_array);

	close(sock_array[1].fd);
	return EXIT_SUCCESS;
}