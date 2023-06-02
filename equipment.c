#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024
#define MSGSZ 500
#define STR_MIN 8

void usage(){
    printf("Chamada correta: ./equipment <server IP> <port>\n");
    exit(EXIT_FAILURE);
}

// --- LOGICA DA INTERFACE --- //
// - A interface do equipamento vai pegar um comando do teclado e transformar numa mensagem de requisicao, segundo:
//  <devId>, <locId> e <valuei> sao int 
//  MENSAGEM     COMANDO      
//  REQ_ADD      REQ_ADD
//  REQ_REM      REQ_REM(IdEq)   
//  REQ_INF      ReqInf(IdEq1, IdEq2)

// - A interface vai receber uma mensagem de resposta do servidor e imprimir um aviso na tela segundo:
// MENSAGEM     PRINT
// ERROR 01     Equipment not found
// ERROR 02     Source equipment not found
// ERROR 03     Target equipment not found
// ERROR 04     Equipment limit exceeded
// ERROR 06     Peer limit exceeded
// ERROR 07     Peer not found
// OK 01        Successful removal
// RES_ADD      device <devId> in ComId: <valor1> <valor>
// RES_LIST     local <locId>: <devId> <valor1> <valor2> <devId2> <valor1> <valor2>
// RES_INF     local <locId>: <devId> <valor1> <valor2> <devId2> <valor1> <valor2>

unsigned process_command(char *comando, char *msg_out){
    
    //Inicialmente, token tem a primeira palavra do comando, caso hajam parâmetros
    char *token = strtok(comando, "(");
    
    //caso REQ_ADD
    if(!strcmp(comando, "REQ_ADD")){
        
        strcpy(msg_out, "REQ_ADD"); 
        
    }
    
    //caso REQ_REM
    else if(!strcmp(token, "REQ_REM")){
    
        strcpy(msg_out, "REQ_REM"); 

        token = strtok(NULL, ")"); //token = IdEq
        strcat(msg_out, " ");
        strcat(msg_out, token);
    }

    //caso REQ_INF
    else if(!strcmp(token, "REQ_INF")){

        strcpy(msg_out, "REQ_INF"); 

        token = strtok(NULL, ","); //token = IdEq1
        
        strcat(msg_out, " ");
        strcat(msg_out, token);

        token = strtok(NULL, ")"); //token = IdEq2

        strcat(msg_out, " ");
        strcat(msg_out, token);
    }
    //caso erro
    else{
        return 0;
    }

    return 1;
}

//Transforma uma mensagem em um aviso no terminal do cliente
void process_res_msg(char *msg_in, char *str_out, char *req_msg){
    char *token = strtok(msg_in, "("); //token = type
    int type = parse_msg_type(token);
    unsigned code = 0;
    char *id_aux = malloc(STR_MIN);

    switch (type){

        case ERROR:
            token = strtok(NULL, ")"); //token = codigo do erro
            code = atoi(token);

            switch (code){
                case 1:
                    strcpy(str_out,"Equipment not found\n");
                break;

                case 2:
                    strcpy(str_out,"Source equipment not found\n");
                break;

                case 3:
                    strcpy(str_out,"Target equipment not found\n");
                break;

                case 4:
                    strcpy(str_out,"Equipment limit exceeded\n");
                break;

                case 6:
                    strcpy(str_out,"Peer limit exceeded\n");
                break;

                case 7:
                    strcpy(str_out,"Peer not found\n");
                break;

                default:
                    
                break;
            }
        break;

        case OK:
            token = strtok(NULL, ")"); //token = codigo do erro
            code = atoi(token);

            switch (code){

                case 1:
                    strcpy(str_out,"Successful removal\n");
                break;

                default:
                    
                break;
            }
        break;

        case RES_ADD:
            token = strtok(NULL, ")"); //token = Equipment Id
            strcpy(id_aux, token);
            
            strcpy(str_out, "Equipment ");
            strcat(str_out, id_aux);
            strcat(str_out, " added\n");
        break;

        case RES_LIST:
            

        break;

        case RES_INF:
            

        break;

        default:
            break;
    }
}

int main(int argc, char **argv){
    if(argc < 3){
        usage();
    }
    
    // - CONEXAO COM SERVIDOR - //

    // Recebe versao e porta na variavel storage
    struct sockaddr_storage storage;
    if(0 != addrparse(argv[1], argv[2], &storage)){
        usage();
    }

    // inicializa o socket
    int s = socket(storage.ss_family, SOCK_STREAM, 0);
    if(s == -1){
        logexit("erro ao incializar socket");
    }

    //Salva o endereco de storage em addr, que eh do tipo correto (addr_storage nao eh suportado)
    struct sockaddr *addr = (struct sockaddr *)(&storage);

    //Abre uma conexao no socket em um determinado endereco
    if(connect(s, addr, sizeof(storage)) != 0){
        logexit("erro ao conectar com servidor");
    }

    //Transforma addr de volta pra string e printa
    char addrstr[BUFSZ];
	addrtostr(addr, addrstr, BUFSZ);

    printf("conectado a %s\n", addrstr);

    // - TROCA DE MENSAGENS - //
    unsigned total = 0;
    while(1){
        char *buf = malloc(BUFSZ);
        memset(buf, 0, BUFSZ-1);
        fgets(buf, BUFSZ-1, stdin);
        buf = strtok(buf, "\n");

        //msg_buf guarda a mensagem que vai ser enviada ao server
        char *msg_buf = malloc(MSGSZ);
        unsigned correto = process_command(buf, msg_buf);
        if(!correto)
            break;  //se receber mensagem com algum erro, sai do loop
        if(!strcmp(buf, "close connection"))
            break;  //se receber o comando close connection, sai do loop

        // envia mensagem ao servidor
        size_t count = send(s, msg_buf, strlen(msg_buf)+1, 0); //envia como string
        if (count != strlen(msg_buf)+1) {
            logexit("send");
        }

        // Recebe mensagem de resposta
        char *buf_res = malloc(MSGSZ);
        count = recv(s, buf_res + total, BUFSZ - total, 0);
        if(count == 0){
            //Se nao receber nada, significa que a conexão foi fechada
            break;
        }
        total += count; //Desloca o buffer pra receber o proximo byte

        //Trata mensagem de resposta e imprime aviso na tela
        char *warn = malloc(BUFSZ);
        process_res_msg(buf_res, warn, msg_buf);
        
        printf("%s", warn);

        // Libera a memoria
        free(buf);
        buf = NULL;
        free(msg_buf); //desaloca o espaco da msg
        msg_buf = NULL;
        free(buf_res);
        buf_res = NULL;
        free(warn);
        warn = NULL;

        total = 0;
    }
    //Ao sair do loop, fecha o socket e finaliza a conexao
    close(s);

    //Termina o programa
    exit(EXIT_SUCCESS);
}


