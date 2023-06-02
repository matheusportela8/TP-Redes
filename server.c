#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024
#define MSGSZ 500
#define STR_MIN 8
#define MIN_EQ_ID 1
#define MAX_EQ_ID 15

int aux_peer = 0; //auxiliar que verifica se o servidor tem um peer

//cada equipamento tem um ID
struct equipment{
    unsigned id;
};

//cada cluster pode ter até 15 equipamentos
struct cluster{
    struct equipment equipments[15];
    int qtd;
};

void usage(){
    printf("Chamada correta: ./server <v4> <port>\n");
    exit(EXIT_FAILURE);
}

//Funcoes pra construir mensagens de controle ERROR e OK ja em formato de string
void build_error_msg(char *msg_out, unsigned codigo){
    
    //parse int->str
    char *code_aux = malloc(sizeof(STR_MIN));
    sprintf(code_aux, "%02u", codigo);

    strcpy(msg_out, "ERROR(");
    strcat(msg_out, code_aux);
    strcat(msg_out, ")");
    strcat(msg_out, "\n");

    free(code_aux);
}
void build_ok_msg(char *msg_out, unsigned codigo){
    
    //parse int->str
    char *code_aux = malloc(sizeof(STR_MIN));
    sprintf(code_aux, "%02u", codigo);

    strcpy(msg_out, "OK ");
    strcat(msg_out, code_aux);
    strcat(msg_out, ")");
    strcat(msg_out, "\n");

    free(code_aux);
}

//Funcao que retorna um id de 1 a 15 disponivel para o novo equipamento adicionado
unsigned get_id_valid(struct cluster clusters){
    for(int i = MIN_EQ_ID - 1; i <= MAX_EQ_ID - 1; i++){
        if(clusters.equipments[i].id == 0) {
            return i + 1;
        }
    }
    return 0;
}

//Funcao que trata a mensagem que chega do cliente e retorna a mensagem de controle ou de RESPONSE para o cliente na variavel msg_out
void process_request(char *request, char *response, struct cluster clusters){

    char *token = strtok(request, " "); //token = type
    unsigned req_type = parse_msg_type(token);

    switch (req_type){
        case REQ_ADD:
            //verifica se a quantidade de maxima de conexoes foi atingida, se sim retorna erro
            if(clusters.qtd>=15){
                build_error_msg(response, 4);
                return;
            }
            else {
                if(aux_peer == 0) {
                    int EqId = get_id_valid(clusters); //atribui a EqId um ID entre 1 e 15
                    clusters.equipments[EqId - 1].id = EqId; //adiciona o equipamento na base de dados
                    printf("Equipment %02u added\n", EqId);
                }
            }
        break;
        
        default:

        break;
    }
}

//struct com os dados do cliente que sera passado na hora de fazer a troca de mensagens
struct client_data {
    int csock;
    struct sockaddr_storage storage;
};

// thread para que o server consiga lidar com mutiplos clientes em paralelo
void * client_thread(void *data, struct cluster clusters) {

    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    // Transforma o endereco de sockaddr pra string
    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);

    //Recebe mensagem do cliente
    char req_msg[MSGSZ];
    memset(req_msg, 0, MSGSZ);
    size_t count = recv(cdata->csock, req_msg, MSGSZ - 1, 0);
    if(count == 0){
        //Se nao receber nada é porque a conexão foi fechada
        close(cdata->csock); // Encerra a conexao com aquele cliente
    }
    printf("%s", req_msg); //Imprime a mensagem na tela

    //Processa a mensagem recebida e guarda a mensagem de resposta ao cliente numa string
    char *res_msg = malloc(MSGSZ);
    process_request(req_msg, res_msg, clusters);

    //Envia a mensagem de resposta
    count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
    if (count != strlen(res_msg) + 1) {
        logexit("erro ao enviar mensagem de resposta");
    }

    free(res_msg);
    res_msg = NULL;

    close(cdata->csock);

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv){
    if(argc < 3){
        usage();
    }

    //base de dados do servidor
    struct cluster database;

    //inicia a quantidade de dispostivos no cluster como 0
    database.qtd = 0;

    //inicia todos os ids do cluster como 0
    for(int i = MIN_EQ_ID - 1; i <= MAX_EQ_ID - 1; i++){
        database.equipments[i].id = 0;
    }
    
    // Recebe versao e porta na variavel storage
    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage();
    }

    // inicializa o socket
    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("erro ao inicializar socket");
    }

    //reutiliza a mesma porta em duas execucoes consecutivas 
    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    // Salva o endereco de storage em addr
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    
    // bind atribui o endereço ao socket
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("erro no bind");
    }
 
    // Espera conexao do equipamento
    // Recebe o socket e o maximo de conexoes pendentes possiveis para aquele socket (15)
    if (0 != listen(s, 15)) {
        logexit("erro no listen");
    }
    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1) {

        // Aceita conexao do cliente
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        // A funcao accept aceita a conexao pelo socket s e cria outro socket pra comunicar com o cliente 
        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("erro ao conectar com o cliente");
        }

        //Inicializa o client_data e armazena as informacoes do csock e do storage
        struct client_data *cdata = malloc(sizeof(*cdata));
	    if (!cdata) {
		    logexit("malloc");
	    }
	    cdata->csock = csock;
	    memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));

        //Dispara thread para tratar a conexao do cliente que chegou
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread(cdata, database), cdata);
    }

    exit(EXIT_SUCCESS);
}