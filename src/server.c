#include "common.h"
#include "msg_struct.h"

int handle_bind(char* serv_port){
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, serv_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int is_a_legal_word(char* log, char* message, struct message* msg_infos_send, int* fd, char* word_type){
	int len = strlen(log);
	if (len > 0 && log[len-1] == '\n')
	{
		log[len-1] = '\0';
		len --;
	}

	if (len == 0)
	{
		sprintf(message, "[Serveur] : Erreur %s vide", word_type);
		
		msg_infos_send->pld_len = strlen(message);
		write_message(fd, *msg_infos_send, message);
		return 0;
	}
	if (len >= NICK_LEN)
	{

		sprintf(message, "[Serveur] : Erreur %s trop long (>%d caractères)", word_type, NICK_LEN);

		msg_infos_send->pld_len = strlen(message);
		write_message(fd, *msg_infos_send, message);
		return 0;
	}
	return 1;
}

void handle_command(struct pollfd* pfd, client_node *head, ban_usr** ban_list, struct message msg_infos, char* buf_rcv, int nb_byte_rcv)
{	
	if(nb_byte_rcv == 0 || strcmp(buf_rcv , "/quit\n") == 0){
		if (nb_byte_rcv == 0)
		{
			printf("client disconnected via Ctrl+C fd=%d\n", pfd->fd);
		} else {
			printf("client disconnected via /quit fd=%d\n", pfd->fd);
		}
		close(pfd->fd);
		deconnect_node(head, pfd->fd);
		pfd->fd = -1;
		pfd->events = 0;
		pfd->revents = 0;

	}else{
		char buf_send[MSG_LEN];
		struct message msg_infos_send;

		if (strcmp(msg_infos.nick_sender, "") == 0 && msg_infos.type != LOGIN)
		{
			sprintf(buf_send, "[Server] : please login with /login <pseudo> <password>");

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = LOGIN;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			return;
		}
		
		switch (msg_infos.type)
		{
		case LOGIN:
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = LOGIN_FAIL;
			strcpy(msg_infos_send.infos, "");

			// On regarde si les pseudo respecte les différentes régles
			if (!is_a_legal_word(msg_infos.infos, buf_send, &msg_infos_send, &(pfd->fd), "pseudo"))
			{
				return;
			}
			
			int length = strlen(msg_infos.infos);
			for (int i = 0; i < length; i++)
			{
				if (!isalnum(*(msg_infos.infos+i)))
				{
					strcpy(buf_send, "[Server] : Erreur votre pseudo ne doit contenir que des lettres ou des chiffres");
					msg_infos_send.pld_len = strlen(buf_send);
					
					write_message(&(pfd->fd), msg_infos_send, buf_send);
					return;
				}
			}

			// On regarde si le pseudo est présent ou non et si l'utilisateur qui l'utilise est connecté ou non
			char password_ref[PWD_LEN];
			int is_connected = 0;
			char history[NICK_LEN];
			int pseudo_is_in_n = pseudo_is_in_node(head, msg_infos.infos, password_ref, &is_connected, history);
			if (pseudo_is_in_n && is_connected)
			{
				strcpy(buf_send, "[Server] : Erreur votre pseudo est déjà utilisé par un autre utilisateur");
				msg_infos_send.pld_len = strlen(buf_send);
				
				write_message(&(pfd->fd), msg_infos_send, buf_send);
				return;
			}

			// On regarde si le mdp est au bon format
			char* password = buf_rcv + 7 + strlen(msg_infos.infos) + 1;
			if (!is_a_legal_word(password, buf_send, &msg_infos_send, &(pfd->fd), "password"))
			{
				return;
			}

			if (is_usr_ban(*ban_list, msg_infos.infos))
			{
				strcpy(buf_send, "[Server] : Erreur ce pseudo est ban");
				msg_infos_send.pld_len = strlen(buf_send);
				
				write_message(&(pfd->fd), msg_infos_send, buf_send);
				return;
			}

			char timeis[100]; 
			// On ajoute les différentes informations de l'utilisateur
			add_info_to_node(head, pfd->fd, msg_infos.infos, password, get_time(timeis, sizeof(timeis)));

			// Si le pseudo est déja présent, on regarde si les mdp sont bien équivalent 
			if (pseudo_is_in_n && (strcmp(password_ref, password) != 0))
			{
				strcpy(buf_send, "[Server] : Erreur mauvais password");
				msg_infos_send.pld_len = strlen(buf_send);
				
				write_message(&(pfd->fd), msg_infos_send, buf_send);
				return;
			}
			else if (pseudo_is_in_n)
			{	
				// On supprime l'ancien noeud de l'utilisateur
				client_node* logger= find_node_withfd(head->next, pfd->fd);
				strcpy(logger->history, history);
				head->next = del_node(head->next, msg_infos.infos);
			}else{
				// premiere connexion
				client_node* logger= find_node(head->next, msg_infos.infos);
				char filename[NICK_LEN];
				strcpy(logger->history, create_user_file(logger->pseudo, filename));
			}
			sprintf(buf_send, "[Server] : Welcome on the chat %s", msg_infos.infos);
			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = LOGINOK;
			strcpy(msg_infos_send.infos, msg_infos.infos);

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			client_node* logger= find_node(head->next, msg_infos.infos);
			int achv = filetxt_to_buf(logger->history, buf_send);
			if(achv == 0){
				sprintf(buf_send, "Historique trop long pour ếtre affiché");
				msg_infos_send.pld_len = strlen(buf_send);
				strcpy(msg_infos_send.nick_sender,"");
				msg_infos_send.type = HISTORY_SEND;
				strcpy(msg_infos_send.infos, msg_infos.infos);

				write_message(&(pfd->fd), msg_infos_send, buf_send);
			}else if(achv == 1){
				msg_infos_send.pld_len = strlen(buf_send);
				strcpy(msg_infos_send.nick_sender,"");
				msg_infos_send.type = HISTORY_SEND;
				strcpy(msg_infos_send.infos, msg_infos.infos);

				write_message(&(pfd->fd), msg_infos_send, buf_send);	
			}
			break;

		case NICKNAME_NEW:
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = NICKNAME_NEW;
			strcpy(msg_infos_send.infos, msg_infos.nick_sender);

			if (!is_a_legal_word(msg_infos.infos, buf_send, &msg_infos_send, &(pfd->fd), "password"))
			{
				return;
			}

			int len = strlen(msg_infos.infos);
			for (int i = 0; i < len-1; i++)
			{
				if (!isalnum(*(msg_infos.infos+i)))
				{
					strcpy(buf_send, "[Server] : Erreur votre pseudo ne doit contenir que des lettres ou des chiffres");
					msg_infos_send.pld_len = strlen(buf_send);
					
					write_message(&(pfd->fd), msg_infos_send, buf_send);
					return;
				}
			}

			char password_reff[PWD_LEN];
			int pseudo_is_in_no;
			char history2[NICK_LEN];
			if (pseudo_is_in_node(head, msg_infos.infos, password_reff, &pseudo_is_in_no, history2))
			{
				strcpy(buf_send, "[Server] : Erreur votre pseudo est déjà utilisé par un autre utilisateur");
				msg_infos_send.pld_len = strlen(buf_send);
				
				write_message(&(pfd->fd), msg_infos_send, buf_send);
				return;
			}
			sprintf(buf_send, "[Server] : Welcome on the chat %s", msg_infos.infos);

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = 0;

			strcpy(msg_infos_send.infos, msg_infos.infos);
			change_pseudo(head, pfd->fd, msg_infos.infos);
			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;

		case NICKNAME_LIST:
			char* res = concat_nick(head->next);
			sprintf(buf_send, "[Server] : Online users are\n%s", res);
			free(res);

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = NICKNAME_LIST;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;

		case NICKNAME_INFOS:
			client_node* user = find_node_withbsn(head, msg_infos.infos);
			if(user != NULL){
				if (user->is_connected)
				{
					sprintf(buf_send, "[Server] : %s connected since %s with IP address %s and port number %d", user->pseudo, user->logintime, user->ip, user->port);
				}
				else
				{
					sprintf(buf_send, "[Server] : %s is not connected. Last connection : %s", user->pseudo, user->logintime);					
				}
			}else{
				sprintf(buf_send, "[Server] : This user do not exist\n");
			}
			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = NICKNAME_INFOS;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;

		case BROADCAST_SEND:
			sprintf(buf_send, "[ALL / From : %s] %s", msg_infos.nick_sender, buf_rcv + 8);
			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = BROADCAST_SEND;
			strcpy(msg_infos_send.infos, "");

			int fds[100];
			give_all_active_fd(head->next, fds);

			int i = 0;
			while (fds[i] != -1)
			{
				if (fds[i] != pfd->fd)
				{
					write_message(&fds[i], msg_infos_send, buf_send);
					//actualise l'historique
					client_node* receiver= find_node_withfd(head->next, fds[i]);
					FILE* fp = fopen(receiver->history, "a");
					fprintf(fp, "[%s] -> [<all>] : %s", msg_infos.nick_sender, buf_rcv + 8);
					fclose(fp);
				}
				else{
					client_node* messager= find_node_withfd(head->next, fds[i]);
					FILE* fp = fopen(messager->history, "a");
					fprintf(fp, "[<you>] -> [<all>] : %s", buf_rcv + 8);
					fclose(fp);
				}
				i++;
			}
			break;

		case UNICAST_SEND:
			sprintf(buf_send, "[%s] %s", msg_infos.nick_sender, buf_rcv + 5 + strlen(msg_infos.infos) + 1);
			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = UNICAST_SEND;
			strcpy(msg_infos_send.infos, "");

			int fd_destination = give_fd_from_pseudo(head->next, msg_infos.infos);

			if (fd_destination == -1)
			{
				sprintf(buf_send, "[Server] Pseudo %s inconnu ou utilisateur non connecté", msg_infos.infos);
				msg_infos_send.pld_len = strlen(buf_send);

				write_message(&(pfd->fd), msg_infos_send, buf_send);
				return;
			}

			client_node* messager= find_node(head->next, msg_infos.nick_sender);
			FILE* fp = fopen(messager->history, "a");
			fprintf(fp, "[<you>] -> [%s] : %s", msg_infos.infos, buf_rcv + 5 + strlen(msg_infos.infos) + 1);
			fclose(fp);

			client_node* receiver= find_node(head->next, msg_infos.infos);
			FILE* fp2 = fopen(receiver->history, "a");
			fprintf(fp2, "[%s] -> [<you>] : %s", msg_infos.nick_sender, buf_rcv + 5 + strlen(msg_infos.infos) + 1);
			fclose(fp2);
			
			write_message(&fd_destination, msg_infos_send, buf_send);
			break;

		case FILE_UPLOAD:
			int success = read_file_for_serv(&(pfd->fd), buf_rcv, msg_infos.nick_sender);
			if(success == 1){
				sprintf(buf_send, "[Server] Fichier %s uploadé", buf_rcv);
			}else if(success == -1){
				sprintf(buf_send, "[Server] Echec fichier trop lourd %s", buf_rcv);
			}else{
				sprintf(buf_send, "[Server] Echec de l'upload du fichier %s", buf_rcv);
			}

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = ECHO_SEND;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;

		case FILE_LIST:
			give_files_list(buf_send);

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = FILE_LIST;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;
		
		case FILE_DOWNLOAD:
			if (!is_file_available(buf_rcv + 9))
			{
				strcpy(buf_send, "[Server] Ce fichier n'est pas disponible");
			}

			strcpy(buf_send, "[Server] Envoie du fichier");
			
			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = FILE_DOWNLOAD;
			strcpy(msg_infos_send.infos, buf_rcv + 9);

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			char filepath[300];
			sprintf(filepath, "files/%s", buf_rcv + 9);
			write_file(&(pfd->fd), filepath);
			break;

		case BAN_USER:

			char* user_to_ban = buf_rcv + 5;
			user_to_ban[strlen(user_to_ban)-1] = '\0';
			client_node* user_node = find_node(head->next, user_to_ban);

			if (strcmp(msg_infos.nick_sender, "admin") != 0)
			{
				strcpy(buf_send, "[Server] Erreur : Vous n'avez pas les droits pour utiliser cette commande");
			}
			else if (strlen(user_to_ban) == 0)
			{
				strcpy(buf_send, "[Server] Erreur : Aucun pseudo -> /ban <user>");
			}
			else if (user_node == NULL || !(user_node->is_connected))
			{
				strcpy(buf_send, "[Server] Erreur : Aucun utilisateur avec ce pseudo");
			}
			else if (strcmp(user_to_ban, "admin") == 0)
			{
				strcpy(buf_send, "[Server] Erreur : Vous ne pouvez pas vous ban vous-même");
			}
			else
			{
				struct message msg_infos_ban;
				strcpy(buf_send, "[Server] : You have been banned by the administrator");
				msg_infos_ban.pld_len = strlen(buf_send);
				strcpy(msg_infos_ban.nick_sender, "");
				msg_infos_ban.type = YOU_ARE_BAN;
				strcpy(msg_infos_ban.infos, "");

				write_message(&(user_node->fd), msg_infos_ban, buf_send);

				*ban_list = add_ban_usr(*ban_list, user_to_ban);

				strcpy(buf_send, "[Server] : L'utilisateur a bien été banni");
			}
			
			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = BAN_USER;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;

		case FILE_REQUEST:

			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = FILE_REQUEST;
			strcpy(msg_infos_send.infos, "");

			char* filename = buf_rcv + 6 + strlen(msg_infos.infos) + 1;

			if (!is_a_legal_word(msg_infos.infos, buf_send, &msg_infos_send, &(pfd->fd), "pseudo") ||
				!is_a_legal_word(filename, buf_send, &msg_infos_send, &(pfd->fd), "filename"))
			{
				return;
			}

			client_node* n = find_node(head, msg_infos.infos);

			if (n == NULL || !(n->is_connected))
			{
				strcpy(buf_send, "[Server] L'utilisateur n'existe pas ou n'est pas connecté");
				msg_infos_send.pld_len = strlen(buf_send);
			}
			else
			{
				sprintf(buf_send,"[Server] %s veut vous envoyer un fichier : %s", msg_infos.nick_sender, filename);

				msg_infos_send.pld_len = strlen(buf_send);
				strcpy(msg_infos_send.infos, msg_infos.nick_sender);
			}
			
			write_message(&(n->fd), msg_infos_send, buf_send);
			break;

		case CMD_LIST:
			strcpy(buf_send, "[Server] : Voici la liste des commandes :\n /quit\n /nick <user_name>\n /who\n /whois <user_name> <message>\n /msgall\n /msg <user_name> <message>\n /sendfile <path_to_file>\n /listfiles\n /getfile <path_to_file>\n");

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = ECHO_SEND;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;
		case ECHO_SEND:
			sprintf(buf_send, "[Server Echo] : La commande suivant n'existe pas : %s Pour avoir la liste des commandes faites /listcmd", buf_rcv);

			msg_infos_send.pld_len = strlen(buf_send);
			strcpy(msg_infos_send.nick_sender,"");
			msg_infos_send.type = ECHO_SEND;
			strcpy(msg_infos_send.infos, "");

			write_message(&(pfd->fd), msg_infos_send, buf_send);
			break;
			
		default:
			break;
		}

		//print_client_list(head);
		//print_ban_usr(*ban_list);
	}

}

void handle_connection(int sfd){
	struct pollfd sock_array[100];
    sock_array[0].fd = sfd;
    sock_array[0].events = POLLIN;
    sock_array[0].revents = 0;
    for(int i = 1; i<100; i+=1){
        sock_array[i].fd = -1;
        sock_array[i].events = 0;
        sock_array[i].revents = 0;
    }

	client_node* head = init_node();
	ban_usr* ban_list = NULL;

	add_node(head, "", -1, -1);
	char timeis[100]; 
	add_info_to_node(head, -1, "admin", "adminpass", get_time(timeis, sizeof(timeis)));
	client_node* logger= find_node(head->next, "admin");
	char filename[NICK_LEN];
	strcpy(logger->history, create_user_file("admin", filename));
	deconnect_node(head, -1);


	while(1){
		int ret = poll(sock_array, 100, -1);
		die(ret, "poll");
		printf("\nPoll returned\n");
		//printf("socket event %d\n", sock_array[1].revents);
		for(int i = 0; i<100; i+=1){
			if (sock_array[i].fd == sfd && (sock_array[i].revents & POLLIN) == POLLIN){
				//new in coming connection
				//can call accept that will return with new fd
				struct sockaddr_in cli;
                socklen_t len = sizeof(struct sockaddr_in);
				int connfd = accept(sfd, (struct sockaddr *) &cli, &len);
				die(connfd, "on accept");

				char client_ip[16];
				inet_ntop(AF_INET, &(cli.sin_addr), client_ip, INET_ADDRSTRLEN);
				add_node(head, client_ip, ntohs(cli.sin_port), connfd);


				//find an empty slot in sock_array to fill it in with new fd
				for (int j = 1; j < 100; j++) {
					if (sock_array[j].fd == -1) {
						sock_array[j].fd = connfd;
						sock_array[j].events = POLLIN | POLLHUP;
						sock_array[j].revents = 0;
						printf("new connexion fd=%d\n", connfd);
						break;
					}
				}
				
				char* buf = "[Server] : please login with /login <pseudo> <password>";

				struct message msg_infos;
				msg_infos.pld_len = strlen(buf);
				strcpy(msg_infos.nick_sender,"");
				msg_infos.type = LOGIN;
				strcpy(msg_infos.infos, "");

				write_message(&connfd, msg_infos, buf);

			}else if (sock_array[i].fd != sfd && (sock_array[i].revents & POLLIN) == POLLIN){
				
				char buf_rcv[MSG_LEN]={0};
				struct message msg_infos;
				int nb_byte_rcv = read_message(&sock_array[i].fd, &msg_infos, buf_rcv);
				
				handle_command(&sock_array[i], head, &ban_list, msg_infos, buf_rcv, nb_byte_rcv);
				
			}else if(sock_array[i].revents & POLLHUP){
				close(sock_array[i].fd);
				sock_array[i].fd = -1;
				sock_array[i].events = 0;
				sock_array[i].revents = 0;
				printf("client disconnected via pollhup fd=%d\n", sock_array[i].fd);
			//should check on POLLHUPactivity for disconnection
			}
			sock_array[i].revents = 0;
		}	
    }     

	free_node(head);
	free_ban_usr(ban_list);
}

int main(int argc, char* argv[]) {
	if (argc != 2)
	{
		printf("Mauvais nombres d'arguments :\n%s <server_port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	int sfd;
	sfd = handle_bind(argv[1]);
	if ((listen(sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	handle_connection(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}

	

    
        
