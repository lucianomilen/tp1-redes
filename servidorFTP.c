// LUCIANO OTONI MILEN 2012079754

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10

#define MAXBUF 100

void sigchld_handler(int s)
{
        // waitpid() might overwrite errno, so we save and restore it:
        int saved_errno = errno;

        while(waitpid(-1, NULL, WNOHANG) > 0) ;

        errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
        if (sa->sa_family == AF_INET) {
                return &(((struct sockaddr_in*)sa)->sin_addr);
        }

        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv)
{
        if (argc != 3) {
                fprintf(stderr,"[servidor] USO: <porta> <tamanho_buffer>\n");
                exit(1);
        }
        int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
        struct addrinfo hints, *servinfo, *p;
        struct sockaddr_storage their_addr; // connector's address information
        socklen_t sin_size;
        struct sigaction sa;
        size_t buffsize = strtol(argv[2], NULL, 10);
        int yes = 1;
        char s[INET6_ADDRSTRLEN];
        int rv;
        char *msg_write, msg_read[100];
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP



        printf("Tamanho do buffer: %lu\n", buffsize);
        if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
                fprintf(stderr, "[servidor] getaddrinfo: %s\n", gai_strerror(rv));
                return 1;
        }

        // loop through all the results and bind to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
                if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                     p->ai_protocol)) == -1) {
                        perror("[servidor] falha no socket");
                        continue;
                }

                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                               sizeof(int)) == -1) {
                        perror("[servidor] setsockopt falhou");
                        exit(1);
                }

                if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                        close(sockfd);
                        perror("[servidor] bind");
                        continue;
                }

                break;
        }

        freeaddrinfo(servinfo); // all done with this structure

        if (p == NULL)  {
                fprintf(stderr, "[servidor] falhou no bind\n");
                exit(1);
        }

        if (listen(sockfd, BACKLOG) == -1) {
                perror("[servidor] escuta");
                exit(1);
        }

        sa.sa_handler = sigchld_handler; // reap all dead processes
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
                perror("[servidor] sigaction");
                exit(1);
        }

        printf("[servidor] Servidor rodando, aguardando conexao...\n");

        while(1) { // laço que faz a troca entre servidor e cliente
                sin_size = sizeof their_addr;
                new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
                if (new_fd == -1) {
                        perror("accept");
                        continue;
                }

                inet_ntop(their_addr.ss_family,
                          get_in_addr((struct sockaddr *)&their_addr),
                          s, sizeof s);
                printf("[servidor] Fantastico! Conexao foi estabelecida com %s\n", s);

                read(new_fd, msg_read, MAXBUF); //recebe o nome do arquivo que deve ser enviado

                FILE *fp = fopen(msg_read, "rb"); //abre o arquivo que o cliente pediu
                msg_write = (char*) malloc (sizeof(char)*(buffsize)); //aloca msg_write para tamanho do buffer

                long bytes_read = 0; //conta quantos bytes foram lidos pela fread()
                while(bytes_read != EOF) { //enquanto não chegar no fim do arquivo
                        bytes_read = fread(msg_write, 1, buffsize, fp);
                        if(bytes_read == 0) { //se a mensagem for diferente de 0 e não negativa e se não foi lido nada do arquivo
                                break;
                        }
                        msg_write[bytes_read] = '\0'; //termina a string com um '\0'
                        write(new_fd, msg_write, buffsize); //escreve mensagem no cliente
                }
                write(new_fd, "bye", buffsize); //"bye" sinaliza para fechar a conexão no lado do cliente
                close(new_fd); //fecha a conexão no lado do servidor
                break;
        }

        return 0;
}
