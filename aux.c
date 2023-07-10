#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  
#include <sys/stat.h>
#include <linux/if.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "aux.h"

struct pollfd fd;

int ConexaoRawSocket(char *device, struct ifreq ir, struct sockaddr_ll endereco, struct packet_mreq mr, int *ret) {
    int soquete;

    soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL)); /*cria socket*/
    if (soquete == -1) {
        printf("Erro no Socket\n");
        exit(-1);
    }

    memset(&ir, 0, sizeof(struct ifreq)); /*dispositivo eth0*/
    memcpy(ir.ifr_name, device, sizeof(device));
    if (ioctl(soquete, SIOCGIFINDEX, &ir) == -1) {
        printf("Erro no ioctl\n");
        exit(-1);
    }

    memset(&endereco, 0, sizeof(endereco)); /*IP do dispositivo*/
    endereco.sll_family = AF_PACKET;
    endereco.sll_protocol = htons(ETH_P_ALL);
    endereco.sll_ifindex = ir.ifr_ifindex;
    if (bind(soquete, (struct sockaddr *)&endereco, sizeof(endereco)) == -1) {
        printf("Erro no bind\n");
        exit(-1);
    }

    memset(&mr, 0, sizeof(mr)); /*Modo Promiscuo*/
    mr.mr_ifindex = ir.ifr_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
        printf("Erro ao fazer setsockopt\n");
        exit(-1);
    }

    struct timeval tv;

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(soquete, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    return soquete;
}

// return 0 for client
// return 1 for server
// return -1 if invalid
int checkDest(Message msg) {

    if (msg.destAddress & 0b10) {
        return 1;
    }
    else if (msg.destAddress & 0b01) {
        return 0;
    }
    else {
        return -1;
    }
}

// return 0 for client
// return 1 for server
int checkSource(Message msg) {
    if (msg.sourceAddress == 0b10) {
        return 1;
    }
    else if (msg.sourceAddress == 0b01) {
        return 0;
    }
    else {
        return -1;
    }
}

// return 1 - sucesso
// return 0 - paridade deu errada
int checkParity(Message msg) {

    int parity;

    for (int i = 0; i < 4; i++) {
        if (i == 0) {
            parity = msg.size ^ msg.type ^ msg.sequence;
        }
        else {
            parity ^= msg.size ^ msg.type ^ msg.sequence;
        }
    }

    for (int  i = 0; i < 15; i++) {
        parity ^= msg.data[i];
    }

    if (parity == msg.parity) {
        return 1;
    }
    
    return 0;
}
// return 0 - sequência inválida
// return 1 - sequência válida
int checkSequence(int seq, Message msg) {
    if (seq == msg.sequence) {
        return 1;
    }

    return 0;
}

// return 0 - cd
// return 1 - ls
// return 2 - cat
// return 3 - line
// return 4 - lines
// return 5 - edit
// return 6 - compile
// return 7 - lcd
// return 8 - lls
// return -1 - invalid
int compareCommand(char *input) {
    if(strstr(input, "cd") != NULL && strlen(input) == strlen("cd")) {
        return 0;
    }
    else if (strstr(input, "ls") != NULL && strlen(input) == strlen("ls") + 1) {
        return 1;
    }
    else if (strstr(input, "ver") != NULL && strlen(input) == strlen("ver")) {
        return 2;
    }
    else if (strstr(input, "linha") != NULL && strlen(input) == strlen("linha")) {
        return 3;
    }
    else if (strstr(input, "linhas") != NULL && strlen(input) == strlen("linhas")) {
        return 4;
    }
    else if (strstr(input, "edit") != NULL && strlen(input) == strlen("edit")) {
        return 5;
    }
    else if (strstr(input, "compilar") != NULL && strlen(input) == strlen("compilar")) {
        return 6;
    }
    else if (strstr(input, "lcd") != NULL && strlen(input) == strlen("lcd")) {
        return 7;
    }
    else if (strstr(input, "lls") != NULL && strlen(input) == strlen("lls") + 1) {
        
        return 8;
    }

    return -1;
}

Message createLs(int dest, int seq) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }
    msg.size = 0;
    msg.sequence = seq;
    msg.type = 0b0001;
    strcpy(msg.data, "\0");

    createParity(&msg);

    return msg;
}

// substitui uma substring 'oldW' por outra 'newW' na string 's'
char * replaceWord(char *s, char *oldW, char* newW) {
    int i, cnt = 0;
    int newWlen = strlen(newW);
    int oldWlen = strlen(oldW);
  
    // Counting the number of times old word
    // occur in the string
    for (i = 0; s[i] != '\0'; i++) {
        if (strstr(&s[i], oldW) == &s[i]) {
            cnt++;
  
            // Jumping to index after the old word.
            i += oldWlen - 1;
        }
    }
  
    // Making new string of enough length
    char *result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1);
  
    i = 0;
    while (*s) {
        // compare the substring with the result
        if (strstr(s, oldW) == s) {
            strcpy(&result[i], newW);
            i += newWlen;
            s += oldWlen;
        }
        else
            result[i++] = *s++;
    }
  
    result[i] = '\0';

    return result;
}

// executa ls no diretório "currPath"
char* executeLs(char *currPath, int *error) {
    errno = 0;
    FILE *fp;
    char path[1035] = "/bin/ls \0";
    char content[1035];

    char * replace = NULL;
    replace = replaceWord(&currPath[0], " ", "\\ ");

    strncat(path, replace, 1000);

    fp = popen(path, "r");

    if (errno == EPERM)  {
        *error = 1;
        return NULL;
    }
    else  if (errno == ENOENT) {
        *error = 2;
        return NULL;
    }

    char *answer = (char*) malloc(100000000 * sizeof(char));
    answer[0] = '\0';

    while (fgets(content, sizeof(content), fp) != NULL) {
        strncat(answer, content, 1035);
    }

    pclose(fp);

    return answer;
}

// cria uma mensagem com o conteúdo de "ls"
Message createLsData(char *data, int sequence, int *EOT, int index) {
    Message msg;
    msg.mark = MARKER;

    int size = 0;
    char resulData[15];

    int pos = 0;

    for (int i = 0; i < 14; i++) {
        if (data[i] != '\0') {
            resulData[pos] = data[i];
            size++;
        }
        else {
            *EOT = 1;
            resulData[pos] = '\0';
            break;
        }
        pos++;
    }

    resulData[14] = '\0';
    
    msg.destAddress = 0b01;
    msg.sourceAddress = 0b10;
    msg.size = size;
    msg.sequence = sequence;

    msg.type = 0b1011;
    
    strcpy(msg.data, resulData);

    createParity(&msg);

    return msg;
}

// cria uma mensagem do tipo ACK
Message createACK(int dest, int seq) {
    Message msg;

    msg.mark = MARKER;
    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }
    msg.size = 0b0000;
    msg.sequence = seq;

    msg.type = 0b1000;
    strcpy(msg.data, "\0");

    createParity(&msg);

    return msg;
}

// cria uma mensagem do tipo NACK
Message createNACK(int dest, int seq) {
    Message msg;

    msg.mark = MARKER;
    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }
    msg.size = 0b0000;
    msg.sequence = seq;

    msg.type = 0b1001;
    strcpy(msg.data, "\0");

    createParity(&msg);

    return msg;
}

Message createCd(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }

    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0000;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}

char* executeCd(Message msg, int *error) {
    errno = 0;


    int size = strlen(msg.data);
    char path[size];

    snprintf(path, size, "%s", msg.data);

    chdir(path);

    // perror("chdir");

    // 1 - acesso proibido / diretório inexistente
    if (errno == EPERM)  {
        *error = 1;
        return NULL;
    }
    else  if (errno == ENOENT) {
        *error = 2;
        return NULL;
    }

    char *answer = (char*) malloc(1035 * sizeof(char));
    
    getcwd(answer, 1035);

    return answer;
}

Message createErrorMessage(int dest, int seq, int error) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }
    
    char err[2];
    err[0] = error + 48;
    err[1] = '\0';
    

    msg.size = 2;
    msg.sequence = seq;
    msg.type = 0b1111;
    strcpy(msg.data, err);

    createParity(&msg);

    return msg;
}

void printError(Message msg) {
    if (msg.data[0] == '1') {
        printf ("acesso proibido / sem permissão\n");
    }
    else if (msg.data[0] == '2') {
        printf ("diretório inexistente\n");
    }
    else if (msg.data[0] == '3') {
        printf ("arquivo inexistente\n");
    }
    else if (msg.data[0] == '4') {
        printf ("linha inexistente\n");
    }
    else {
        printf ("Erro não documentado\n");
    }
    
}

// executa cd no client
char* executeLcd(char *cdPath, int *error) {
    errno = 0;

    int size = strlen(cdPath);
    char path[size];

    snprintf(path, size, "%s", cdPath);

    chdir(path);

    // perror("chdir");

    // 1 - acesso proibido / acesso proibido
    if (errno == EPERM)  {
        *error = 1;
        return NULL;
    }
    else  if (errno == ENOENT) {
        *error = 2;
        return NULL;
    }

    char *answer = (char*) malloc(1035 * sizeof(char));
    
    // get current path
    getcwd(answer, 1035);


    return answer;
}

// executa ls no diretório "currPath" no client
void executeLls(char *currPath) {
    FILE *fp;
    char path[1035] = "/bin/ls \0";
    char content[1035];

    char * replace = NULL;
    replace = replaceWord(&currPath[0], " ", "\\ ");

    strncat(path, replace, 1000);

    fp = popen(path, "r");
    // if (fp == NULL) {
    //     printf("Failed to run command\n" );
    // }

    // cria uma string para guardar o conteúdo
    char *answer = (char*) malloc(100000000 * sizeof(char));
    answer[0] = '\0';

    while (fgets(content, sizeof(content), fp) != NULL) {
        printf ("%s", content);
    }

    pclose(fp);
}

// cria uma mensagem do tipo 'ver'
Message createCat(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }


    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0010;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}

// cria a paridade de uma mensagem
void createParity(Message *msg) {
    for (int i = 0; i < 4; i++) {
        if (i == 0) {
            msg->parity = msg->size ^ msg->sequence ^ msg->type;
        }
        else {
            msg->parity ^= msg->size ^ msg->sequence ^ msg->type;
        }
    }

    for (int  i = 0; i < 15; i++) {
        msg->parity ^= msg->data[i];
    }
}

// executa cat no diretório "currPath"
char* executeCat(char *currPath, int *error) {
    errno = 0;

    int size = strlen(currPath);
    char file[size];
    file[0] = '\0';
    snprintf(file, size, "%s", currPath);

    FILE *fp = fopen(file, "r");

    if (fp == NULL) {
        if (errno == EACCES) {
            *error = 1;
        }
        else  if (errno == ENOENT) {
            *error = 3;
        }

        return NULL;
    }

    char buffer[100000];

    char *answer = (char*) malloc(1000000 * sizeof(char));
    answer[0] = '\0';

    while ((fgets(buffer, 100000, fp)) != NULL) {
        strncat(answer, buffer, 100000);
    }

    fclose(fp);


    return answer;
}

// cria uma mensagem com o conteúdo de "ver"
Message createCatData(char *data, int sequence, int *EOT, int index) {
    Message msg;
    msg.mark = MARKER;

    int size = 0;
    char resulData[15];

    int pos = 0;
    // tenta criar uma string de tamanho 15
    // size indica qual será o tamanho que parou
    for (int i = 0; i < 14; i++) {
        if (data[i] != '\0') {
            resulData[pos] = data[i];
            size++;
        }
        else {
            *EOT = 1;
            resulData[pos] = '\0';
            break;
        }
        pos++;
    }

    resulData[14] = '\0';

    
    msg.destAddress = 0b01;
    msg.sourceAddress = 0b10;
    msg.size = size;
    msg.sequence = sequence;

    msg.type = 0b1100;

    strcpy(msg.data, resulData);

    createParity(&msg);

    return msg;
}

// cria uma mensagem do tipo 'linha'
Message createLineCommand(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }


    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0011;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}

// cria uma mensagem do tipo 'linha inicial e final'
Message createStartEndLine(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }

    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b1010;

    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}

// executa 'linha' no diretório "currPath"
char* executeLineCommand(char *currPath, char *line, int *error) {
    errno = 0;

    int size = strlen(currPath);
    char file[size];
    file[0] = '\0';
    snprintf(file, size, "%s", currPath);
    
    FILE *fp = fopen(file, "r");

    if (fp == NULL) {
        if (errno == EACCES) {
            *error = 1;
        }
        else  if (errno == ENOENT) {
            *error = 3;
        }

        return NULL;
    }

    int lines = 0;
    for (int i = 0; i < strlen(line); i++) {
        if (line[i] == ',') {
            lines++;
        }
    }

    if (lines == 0) {
        char buffer[100000];

        int match = atoi(line);

        if (match == 0) {
            *error = 4;
            return NULL;
        }
        

        char *answer = (char*) malloc(1000000 * sizeof(char));
        answer[0] = '\0';
        int count = 1;
        while ((fgets(buffer, 100000, fp)) != NULL) {
            if (count == match) {
                strncat(answer, buffer, 100000);
                break;
            }
            count++;
        }
        
        fclose(fp);

        if (match > count) {
            *error = 4;
            return answer;
        }

        return answer;
    }
    else {
        char arg1[5];
        char arg2[5];

        char *ptr = strtok(line, ",");
        arg1[0] = '\0';
        strcpy(arg1, ptr);

        arg2[0] = '\0';
        int i = 0;
        while(ptr != NULL) {
            ptr = strtok(NULL, ",");
            if (i == 0) {
                strcpy(arg2, ptr);
                break;
            }
            i++;
        }

        int match1 = atoi(arg1);
        int match2 = atoi(arg2);

        if (match1 > match2) {
            *error = 4;
            return NULL;
        }

        char buffer[100000];
        
        char *answer = (char*) malloc(1000000 * sizeof(char));
        answer[0] = '\0';

        int count = 0;
        count++;
        while ((fgets(buffer, 100000, fp)) != NULL) {
            if (count > match1 && count < match2) {
                strncat(answer, buffer, 100000);
            }
            count++;
        }

        if (match2 > count) {
            *error = 4;
            return NULL;
        }
        
        fclose(fp);

        return answer;
    }

    return NULL;
}

// cria uma mensagem indicando o fim da transmissão
Message createEOT(int dest, int sequence) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }
    msg.size = 0;
    msg.sequence = sequence;

    msg.type = 0b1101;

    strcpy(msg.data, "\0");

    createParity(&msg);

    return msg;
}

// cria uma mensagem do tipo 'linha'
Message createLinesCommand(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }


    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0100;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}


// cria uma mensagem do tipo 'edit'
Message createEdit(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }

    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0101;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}

// cria uma mensagem com o texto da mensagem do tipo "edit"
Message createTextData(char *data, int sequence, int *EOT, int index) {
    Message msg;
    msg.mark = MARKER;

    int size = 0;
    char resulData[15];

    int pos = 0;
    for (int i = 0; i < 14; i++) {
        if (data[i] != '\0') {
            resulData[pos] = data[i];
            size++;
        }
        else {
            *EOT = 1;
            resulData[pos] = '\0';
            break;
        }
        pos++;
    }

    resulData[14] = '\0';
    
    msg.destAddress = 0b10;
    msg.sourceAddress = 0b01;
    msg.size = size;
    msg.sequence = sequence;

    msg.type = 0b1100;

    strcpy(msg.data, resulData);

    createParity(&msg);

    return msg;
}

// executa 'edit' no diretório "currPath"
int executeEditLine(char *line, char *text, char *file) {
    errno = 0;

    FILE *fp;
    FILE *fTemp;

    int size = strlen(file);
    char auxFile[size];

    snprintf(auxFile, size, "%s", file);

    fp = fopen(auxFile, "r+");
    fTemp = fopen("replace.tmp", "w"); 

    if (fp == NULL || fTemp == NULL) {
        if (errno == EACCES) {
            return 1;
        }
        else  if (errno == ENOENT) {
            return 3;
        }
    }

    char buffer[100000];

    char * replace = NULL;
    replace = replaceWord(&text[0], "\"", "");

    int count = 0;
    while ((fgets(buffer, 100000, fp)) != NULL){
        count++;
        if (count == *line - 48) {
            fputs(replace, fTemp);
        }
        else {
            fputs(buffer, fTemp);
        }
    }

    // se a linha for a proxima a ser inserida no arquivo, insere-a
    if (*line - 48 == count + 1) {
        fputs(replace, fTemp);
    }
    // se não, considera como linha inexistente
    else if (*line - 48 > count || *line - 48 == 0) {
        return 4;
    }

    fclose(fp);
    fclose(fTemp);

    remove(auxFile);
    rename("replace.tmp", auxFile);

    return 0;
}

// cria uma mensagem do tipo 'linha'
Message createCompileCommand(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }


    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0110;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}


// cria uma mensagem do tipo 'linha'
Message createCompileArgument(int dest, int seq, char *path) {
    Message msg;
    msg.mark = MARKER;

    // se destino é servidor
    if (dest == 1) {
        msg.destAddress = 0b10;
        msg.sourceAddress = 0b01;
    }
    // se destino é cliente
    else {
        msg.destAddress = 0b01;
        msg.sourceAddress = 0b10;
    }


    msg.size = strlen(path);
    msg.sequence = seq;
    msg.type = 0b0110;
    strcpy(msg.data, path);

    createParity(&msg);

    return msg;
}

// cria uma mensagem contendo os argumentos do compile
Message createCompileArgs(char *data, int sequence, int *EOT, int index) {
    Message msg;
    msg.mark = MARKER;

    int size = 0;
    char resulData[15];

    int pos = 0;
    // tenta criar uma string de tamanho 15
    // size indica qual será o tamanho que parou
    for (int i = 0; i < 14; i++) {
        if (data[i] != '\0') {
            resulData[pos] = data[i];
            size++;
        }
        else {
            *EOT = 1;
            resulData[pos] = '\0';
            break;
        }
        pos++;
    }

    resulData[14] = '\0';
    
    // o destino do conteúdo de ls é sempre para o client
    msg.destAddress = 0b10;
    msg.sourceAddress = 0b01;
    msg.size = size;
    msg.sequence = sequence;

    // checa se é o final da transmissão
    msg.type = 0b0110;

    strcpy(msg.data, resulData);

    createParity(&msg);

    return msg;
}

char* executeCompile(char *currPath, char *args, int *error) {
    errno = 0;

    int size = strlen(currPath);
    char file[size];
    file[0] = '\0';
    snprintf(file, size, "%s", currPath);

    FILE *fTemp;

    fTemp = fopen(file, "r");

    // arquivo inexistente
    if (fTemp == NULL) {
        if (errno == EACCES) {
            *error = 1;
            return NULL;
        }
        else  if (errno == ENOENT) {
            *error = 3;
            return NULL;
        }
    }
    char argv[1000];
    argv[0] = '\0';

    char comp[5000] = "/usr/bin/gcc \0";
    char redirect[100000] = "2> output.temp\0";
    strncat(argv, args, 1000);
    strncat(argv, currPath, 1000);

    printf ("%s\n", argv);

    char executable[100] = "-o \0";
    char aux[100] = "\0";
    strcpy(aux, replaceWord(currPath, ".c", ""));
    strncat(executable, aux, 100);

    size = strlen(executable);
    executable[size - 1] = ' ';

    size = strlen(argv);
    argv[size - 1] = ' ';

    strncat(argv, executable, 100);
    strncat(comp, argv, 1000);

    size = strlen(comp);
    comp[size - 1] = ' ';
    
    strncat(comp, redirect, 100000);

    FILE * fp = popen(comp, "r");

    pclose(fp);

    char output[1000] = "\0";

    char *answer = (char*) malloc(1000000 * sizeof(char));
    answer[0] = '\0';

    FILE *out = fopen("output.temp", "r");

    int tempExist = 0;
    rewind(out);
    if (out != NULL) {
        tempExist = 1;
        while (fgets(output, sizeof(output), out) != NULL) {
            strncat(answer, output, 1035);
        }
    }
    fclose(out);

    pclose(fTemp);

    if (tempExist) {
        remove("output.temp");
    }

    return answer;
}

// verifica se o arquivo existe
int fileExists(char *currPath) {
    int size = strlen(currPath);
    char file[size];
    file[0] = '\0';
    snprintf(file, size, "%s", currPath);

    FILE *fTemp;

    printf ("%s\n", file);

    fTemp = fopen(file, "r");

    if (fTemp == NULL) {
        return 1;
    }
    fclose(fTemp);
    
    return 0;
}