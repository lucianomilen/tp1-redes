// LUCIANO OTONI MILEN 2012079754

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
        if (sa->sa_family == AF_INET) {
                return &(((struct sockaddr_in*)sa)->sin_addr);
        }

        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
        int sockfd;
        struct addrinfo hints, *servinfo, *p;
        int rv, count = 0;
        char s[INET6_ADDRSTRLEN];
        size_t received_bytes = 0;
        size_t buffsize = strtol(argv[4], NULL, 10);
        char *msg_write, *msg_read;
        struct timeval begin, end;



        if (argc != 5) {
                fprintf(stderr,"[cliente] USO: <hostname> <porta> <nome_arquivo> <tamanho_buffer>\n");
                exit(1);
        }

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
                fprintf(stderr, "[cliente] getaddrinfo: %s\n", gai_strerror(rv));
                return 1;
        }

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
                if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                     p->ai_protocol)) == -1) {
                        perror("[cliente] socket falhou");
                        continue;
                }

                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                        close(sockfd);
                        perror("[cliente] nao conectou");
                        continue;
                }

                break;
        }

        if (p == NULL) {
                fprintf(stderr, "[cliente] Oh nao! falhou na conexao =(\n");
                return 2;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                  s, sizeof s);
        printf("[cliente] Fantastico! conectando a %s\n", s);
        gettimeofday(&begin, NULL); //pega o tempo inicial da conexão

        freeaddrinfo(servinfo); // all done with this structure


        msg_write = argv[3]; //parâmetro de onde vem o nome do arquivo
        FILE *fp = fopen("result.md", "w+"); //abre (cria) o arquivo resultado da transferência
        write(sockfd, msg_write, strlen(msg_write)+1); //envia para o servidor o nome do arquivo a ser lido

        //Laco que faz a troca de mensagens entre cliente e servidor.
        //printf("cliente recebe: %s\n", msg_read);
        do {
                msg_read = (char*) malloc (sizeof(char)*buffsize);
                read(sockfd, msg_read, buffsize); //lendo o que vem do servidor
                received_bytes += strlen(msg_read); //incrementa a quantidade de bytes recebidos
                if(strcmp(msg_read, "bye") == 0) { //se recebeu "bye" do servidor, fecha o arquivo e sai do loop
                        fclose(fp);
                        break;
                }

                msg_read[strlen(msg_read)-1] = '\0'; //termina a string com um '\0'
                fprintf(fp, "%s", msg_read); //escreve no arquivo de saída o que recebe do servidor
                //fwrite(msg_read, 1, strlen(msg_read), fp);
                count++;
        } while(1);

        gettimeofday(&end, NULL); //pega o tempo final da conexão
        double elapsed = (end.tv_sec - begin.tv_sec) +
                         ((end.tv_usec - begin.tv_usec)/1000000.0); //calcula o módulo do tempo gasto

        printf("[cliente] count: %d Buffer(s) = %5lu byte(s), %lf kbps (%lu bytes em %lf s)",
               count, received_bytes, ((double)received_bytes/1000)/elapsed, received_bytes, elapsed);


        close(sockfd);

        return 0;
}
