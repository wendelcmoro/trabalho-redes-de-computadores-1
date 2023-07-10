#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "aux.h"

int ret;
char answer[1000000000];
int error = 0;

int main() {
    // diretório atual
    char path[1035];
    getcwd(path, 1035);

    struct ifreq ir;
    struct sockaddr_ll endereco;
    struct packet_mreq mr;

    int socket = ConexaoRawSocket(DEVICE, ir, endereco, mr, &ret);
    if (socket < 1) {
        return 1;
    }

    Message sendingMsg;
    Message receivingMsg;

    Message reserveMsg;

    reserveMsg.sequence = -2;

    // indíce da mensagem recebida/enviada
    int i = 0;
    int count = 0;
    int receivedError = 0;

    int index = 0;
    int EOT = 0;
    char input[10000];

    char auxInput[10000];

    unsigned int sendSequence = 0;
    unsigned int recSequence = 0;

    char *ptr;
    char arg1[40];
    char arg2[40];
    char arg3[40];
    char arg4[40];
    char data[15];
    char command[1000];

    int timeout = 0;

    // Tratando Client
    while (1) {
        timeout = 0;

        receivedError = 0;

        sendingMsg.mark = 0;

        printf ("> ");

        fgets(input, 1000, stdin);
        
        strcpy(auxInput, input);
        ptr = strtok(input, " ");

        char command[1000];

        command[0] = '\0';
        strcpy(command, ptr);
        
        switch (compareCommand(&command[0])) {
            // cd
            case 0:
                arg1[0] = '\0';

                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        break;
                    }
                }

                strcpy(arg1, &auxInput[i + 1]);

                sendingMsg = createCd(1, sendSequence, &arg1[0]);

                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                
                sendSequence++;
                sendSequence = sendSequence % 16;

                reserveMsg = sendingMsg;

                // Aguarda um ACK, NACK ou uma mensagem de erro
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    // se recv retornar valor negativo, reenvia a mensagem
                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER ) {

                        // Se a origem da mensagem é o servidor
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    // Recebeu um ack
                                    if (receivingMsg.type == 0b1000) {

                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    // Recebeu um nack
                                    else if (receivingMsg.type == 0b1001) {
                                        // reenvia a mensagem
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }

                                    // Recebeu um erro
                                    else if (receivingMsg.type == 0b1111) {
                                        printError(receivingMsg);

                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                        
                    }
                    
                }
                
                break;

            // ls
            case 1:
                reserveMsg = sendingMsg;

                sendingMsg = createLs(1, sendSequence);

                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                
                sendSequence++;
                sendSequence = sendSequence % 16;

                // Aguarda um ACK, caso contrário reenvia o ls
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
                    
                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                    }
                }

                answer[0] = '\0';


                // recebe as respostas do servidor
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {
                                    strncat(answer, receivingMsg.data, 15);

                                    // verifica se é o fim da transmissao
                                    if (receivingMsg.type == 0b1011) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);

                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        
                                        sendSequence++;
                                        sendSequence = sendSequence % 16;
                                    }
                                    else if (receivingMsg.type == 0b1101) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        printf ("\n%s\n", answer);

                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                        sendSequence++;
                                        sendSequence = sendSequence % 16;

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                    }
                }
                break;

            // cat
            case 2:
                arg1[0] = '\0';

                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        break;
                    }
                }

                strcpy(arg1, &auxInput[i + 1]);

                reserveMsg = sendingMsg;

                sendingMsg = createCat(1, sendSequence, &arg1[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                
                sendSequence++;
                sendSequence = sendSequence % 16;

                // Aguarda um ACK, caso contrário reenvia o cat
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {

                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == recSequence - 1) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                    }
                }

                answer[0] = '\0';


                // recebe as respostas do servidor
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    strncat(answer, receivingMsg.data, 15);

                                    recSequence++;
                                    recSequence = recSequence % 16;

                                    // verifica se é o fim da transmissao
                                    if (receivingMsg.type == 0b1100) {
                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        
                                        sendSequence++;
                                        sendSequence = sendSequence % 16;

                                        
                                    }
                                    else if (receivingMsg.type == 0b1111) {
                                        printError(receivingMsg);

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1101) {
                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                        sendSequence++;
                                        sendSequence = sendSequence % 16;

                                        printf ("\n");

                                        count  = 1;
                                        if (strlen(answer) > 1) {
                                            printf ("%d ", count);
                                        }
                                        count++;
                                        for (i = 0; i < strlen(answer); i++) {
                                            printf ("%c", answer[i]);
                                            if (answer[i] == '\n' && answer[i + 1] != '\0') {
                                                printf ("%d ", count);
                                                count++;
                                            }
                                        }

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == recSequence - 1) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                        
                    }
                }
                break;

            // line
            case 3:
                arg1[0] = '\0';
                arg2[0] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 2) {
                        break;
                    }
                }
                strcpy(arg2, &auxInput[i + 1]);
                auxInput[i] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 1) {
                        break;
                    }
                }
                strcpy(arg1, &auxInput[i + 1]);
                auxInput[i] = '\0';

                reserveMsg = sendingMsg;

                // Envia a mensagem com o arquivo
                sendingMsg = createLineCommand(1, sendSequence, &arg2[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                sendSequence++;
                sendSequence = sendSequence % 16;

                // Aguarda um ACK, caso contrário reenvia o comando
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
                    
                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                    }
                }

                reserveMsg = sendingMsg;

                // Envia a mensagem com as linhas
                sendingMsg = createStartEndLine(1, sendSequence, &arg1[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                
                sendSequence++;
                sendSequence = sendSequence % 16;

                reserveMsg = sendingMsg;

                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type = 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                    
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                        
                    }
                }

                answer[0] = '\0';


                // recebe as respostas do servidor
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    strncat(answer, receivingMsg.data, 15);

                                    recSequence++;
                                    recSequence = recSequence % 16;

                                    // verifica se é o fim da transmissao
                                    if (receivingMsg.type == 0b1100) {

                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        
                                        sendSequence++;
                                        sendSequence = sendSequence % 16;
                                    }
                                    else if (receivingMsg.type == 0b1111) {
                                        printError(receivingMsg);

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1101) {
                                        printf ("\n%s\n", answer);

                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                        sendSequence++;
                                        sendSequence = sendSequence % 16;

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                            
                        }
                        
                    }
                }
                break;

            // lines
            case 4:
                arg1[0] = '\0';
                arg2[0] = '\0';
                arg3[0] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 3) {
                        break;
                    }
                }
                strcpy(arg3, &auxInput[i + 1]);
                auxInput[i] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 2) {
                        break;
                    }
                }
                strcpy(arg2, &auxInput[i + 1]);
                auxInput[i] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 1) {
                        break;
                    }
                }
                strcpy(arg1, &auxInput[i + 1]);
                auxInput[i] = '\0';

                reserveMsg = sendingMsg;

                // Envia a mensagem com o arquivo
                sendingMsg = createLinesCommand(1, sendSequence, &arg3[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                sendSequence++;
                sendSequence = sendSequence % 16;

                // Aguarda um ACK, caso contrário reenvia o comando
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
                    
                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                       
                    }
                }

                // Envia a mensagem com as linhas
                data[15] = '\0';
                strcpy(data, arg1);

                int argSize = strlen(data);
                data[argSize] = ',';
                data[argSize + 1] = '\0';
                
                strncat(data, arg2, 15);

                reserveMsg = sendingMsg;

                sendingMsg = createStartEndLine(1, sendSequence, &data[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                
                sendSequence++;
                sendSequence = sendSequence % 16;

                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type = 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;
                                    
                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                       
                    }
                }

                answer[0] = '\0';


                // recebe as respostas do servidor
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    strncat(answer, receivingMsg.data, 15);

                                    recSequence++;
                                    recSequence = recSequence % 16;

                                    // verifica se é o fim da transmissao
                                    if (receivingMsg.type == 0b1100) {

                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);

                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        
                                        sendSequence++;
                                        sendSequence = sendSequence % 16;
                                    }
                                    else if (receivingMsg.type == 0b1111) {
                                        printError(receivingMsg);

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1101) {
                                        reserveMsg = sendingMsg;

                                        printf ("\n%s\n", answer);

                                        sendingMsg = createACK(1, sendSequence);

                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                        sendSequence++;
                                        sendSequence = sendSequence % 16;

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                            
                        }
                        
                    }
                }
                break;

            // edit
            case 5:
                arg1[0] = '\0';
                arg2[0] = '\0';
                arg3[0] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == '"') {
                        break;
                    }
                }
                strcpy(arg3, &auxInput[i]);
                auxInput[i] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 2) {
                        break;
                    }
                }
                strcpy(arg2, &auxInput[i + 1]);
                auxInput[i] = '\0';

                count = 0;
                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        count++;
                    }
                    if (count == 1) {
                        break;
                    }
                }
                strcpy(arg1, &auxInput[i + 1]);
                auxInput[i] = '\0';

                // Envia a mensagem com o arquivo
                reserveMsg = sendingMsg;

                sendingMsg = createEdit(1, sendSequence, &arg2[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                sendSequence++;
                sendSequence = sendSequence % 16;

                // Aguarda um ACK, caso contrário reenvia o comando
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
                    
                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                    else if (receivingMsg.type == 0b1111) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        receivedError = 1;

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1 % 16)) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                       
                    }
                }

                if (receivedError) {
                    printError(receivingMsg);
                }
                else {
                    // Envia a mensagem com as linhas
                    reserveMsg = sendingMsg;
                    data[15];

                    data[15] = '\0';
                    strcpy(data, arg1);

                    sendingMsg = createStartEndLine(1, sendSequence, &data[0]);
                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                    
                    sendSequence++;
                    sendSequence = sendSequence % 16;

                    while (1) {
                        timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
                        
                        if (timeout <= 0) {
                            if (receivingMsg.sequence == sendingMsg.sequence) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                            else {
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            }
                        }
                        else if (receivingMsg.mark == MARKER) {
                            
                            if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                                if (checkSequence(recSequence, receivingMsg)) {
                                    if (checkParity(receivingMsg)) {

                                        if (receivingMsg.type = 0b1000) {
                                            recSequence++;
                                            recSequence = recSequence % 16;

                                            break;
                                        }
                                        else if (receivingMsg.type == 0b1001) {
                                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        }
                                    }
                                    else {
                                        reserveMsg = sendingMsg;

                                        sendingMsg = createNACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                    send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                }
                            }
                        
                        }
                    }
                    index = 0;
                    EOT = 0;

                    // envia uma sequência de bytes para o servidor
                    while (1) {
                        reserveMsg = sendingMsg;

                        sendingMsg = createTextData(&arg3[index], sendSequence, &EOT, index);
                        sendSequence++;
                        sendSequence = sendSequence % 16;

                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                        while (1) {
                            recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                            // espera um ack
                            if (timeout <= 0) {
                                if (receivingMsg.sequence == sendingMsg.sequence) {
                                    send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                }
                                else {
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.mark == MARKER) {
                                
                                if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                                    if (checkSequence(recSequence, receivingMsg)) {
                                        if (checkParity(receivingMsg)) {
                                            // Se não receber o ACK, reenvia a ultima mensagem
                                            if (receivingMsg.type == 0b1000) {
                                                recSequence++;
                                                recSequence = recSequence % 16;
                                                break;
                                            }
                                            else if (receivingMsg.type == 0b1001) {
                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                            }
                                        }
                                        else {
                                            reserveMsg = sendingMsg;

                                            sendingMsg = createNACK(1, sendSequence);
                                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        }
                                    }
                                    else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                        send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                    }
                                }
                            
                            }
                        }

                        if (EOT) {
                            break;
                        }
                        index += 14;
                    }

                    reserveMsg = sendingMsg;
                    
                    sendingMsg = createEOT(1, sendSequence);
                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                    sendSequence++;
                    sendSequence = sendSequence % 16;

                    while (1) {
                        timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                        // espera um ack
                        if (timeout <= 0) {
                            if (receivingMsg.sequence == sendingMsg.sequence) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                            else {
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            }
                        }
                        else if (receivingMsg.mark == MARKER) {
                            if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                                if (checkSequence(recSequence, receivingMsg)) {
                                    if (checkParity(receivingMsg)) {

                                        if (receivingMsg.type == 0b1000) {
                                            recSequence++;
                                            recSequence = recSequence % 16;
                                            break;
                                        }
                                        else if (receivingMsg.type == 0b1001) {
                                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        }
                                        
                                    }
                                    else {
                                        reserveMsg = sendingMsg;
                                        
                                        sendingMsg = createNACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                    send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                }
                            }
                        
                        }
                    }

                    // Aguarda um ACK, caso contrário reenvia a ultima mensagem
                    while (1) {
                        timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                        if (timeout <= 0) {
                            if (receivingMsg.sequence == sendingMsg.sequence) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                            else {
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            }
                        }
                        else if (receivingMsg.mark == MARKER) {
                                
                            if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                                if (checkSequence(recSequence, receivingMsg)) {
                                    if (checkParity(receivingMsg)) {

                                        if (receivingMsg.type == 0b1000) {
                                            recSequence++;
                                            recSequence = recSequence % 16;

                                            break;
                                        }
                                        else if (receivingMsg.type == 0b1111) {
                                            printError(receivingMsg);

                                            recSequence++;
                                            recSequence = recSequence % 16;

                                            break;
                                        }
                                        else if (receivingMsg.type == 0b1001) {
                                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        }
                                        
                                    }
                                    else {
                                        reserveMsg = sendingMsg;
                                        
                                        sendingMsg = createNACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                    send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                }
                            }
                            
                        }
                    }
                }

                break;

            // compile
            case 6:
                arg1[0] = '\0';
                arg2[0] = '\0';

                int p1 = 0;
                for (p1 = 0; p1 < strlen(auxInput); p1++) {
                    if (strcmp(&auxInput[p1], ".c") == 0) {
                        break;
                    }
                }

                i = 0;
                // procura pelo delimitador de início do arquivo .c
                for (int p = p1; p >= 2; p--) {
                    if (auxInput[p - 1] == ' ' && auxInput[p - 2] != '\\') {
                        i = p;
                        break;
                    }
                }

                strcpy(arg2, &auxInput[i]);

                auxInput[i] = '\0';

                int p2 = 0;
                for (p2 = 0; p2 < strlen(auxInput); p2++) {
                    if (auxInput[p2 - 1] == ' ' || auxInput[p2] == '\0') {
                        break;
                    }
                }

                strcpy(arg1, &auxInput[p2]);

                reserveMsg = sendingMsg;

                // Envia a mensagem com o arquivo
                sendingMsg = createCompileCommand(1, sendSequence, &arg2[0]);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                sendSequence++;
                sendSequence = sendSequence % 16;

                // Aguarda um ACK, caso contrário reenvia o comando
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
                    
                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                       
                    }
                }

                index = 0;
                EOT = 0;

                // envia uma sequência de bytes para o servidor contendo os argumentos
                while (1) {
                    reserveMsg = sendingMsg;

                    sendingMsg = createCompileArgs(&arg1[index], sendSequence, &EOT, index);
                    sendSequence++;
                    sendSequence = sendSequence % 16;

                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                    while (1) {
                        timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                        if (timeout <= 0) {
                            if (receivingMsg.sequence == sendingMsg.sequence) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                            else {
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            }
                        }
                        else if (receivingMsg.mark == MARKER) {
                            
                            if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                                if (checkSequence(recSequence, receivingMsg)) {
                                    if (checkParity(receivingMsg)) {
                                        // Se não receber o ACK, reenvia a ultima mensagem
                                        if (receivingMsg.type == 0b1000) {
                                            recSequence++;
                                            recSequence = recSequence % 16;
                                            break;
                                        }
                                        else if (receivingMsg.type == 0b1001) {
                                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        }
                                    }
                                    else {
                                        reserveMsg = sendingMsg;

                                        sendingMsg = createNACK(1, sendSequence);
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                }
                                else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                    send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                }
                            }
                           
                        }
                    }

                    if (EOT) {
                        break;
                    }
                    index += 14;
                }

                reserveMsg = sendingMsg;
                
                sendingMsg = createEOT(1, sendSequence);
                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                sendSequence++;
                sendSequence = sendSequence % 16;

                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    if (receivingMsg.type == 0b1000) {
                                        recSequence++;
                                        recSequence = recSequence % 16;
                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1001) {
                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    }
                                    
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                       
                    }
                }

                answer[0] = '\0';


                // recebe as respostas do servidor
                while (1) {
                    timeout = recv(socket, &receivingMsg, sizeof(receivingMsg), 0);

                    if (timeout <= 0) {
                        if (receivingMsg.sequence == sendingMsg.sequence) {
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                        }
                        else {
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                        }
                    }
                    else if (receivingMsg.mark == MARKER) {
                        
                        if (checkDest(receivingMsg) == 0 && checkSource(receivingMsg) == 1) {
                            if (checkSequence(recSequence, receivingMsg)) {
                                if (checkParity(receivingMsg)) {

                                    strncat(answer, receivingMsg.data, 15);

                                    recSequence++;
                                    recSequence = recSequence % 16;

                                    // verifica se é o fim da transmissao
                                    if (receivingMsg.type == 0b1100) {

                                        reserveMsg = sendingMsg;

                                        sendingMsg = createACK(1, sendSequence);

                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                        
                                        sendSequence++;
                                        sendSequence = sendSequence % 16;
                                    }
                                    else if (receivingMsg.type == 0b1111) {
                                        printError(receivingMsg);

                                        break;
                                    }
                                    else if (receivingMsg.type == 0b1101) {
                                        reserveMsg = sendingMsg;

                                        printf ("\n%s\n", answer);

                                        sendingMsg = createACK(1, sendSequence);

                                        send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                        sendSequence++;
                                        sendSequence = sendSequence % 16;

                                        break;
                                    }
                                }
                                else {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createNACK(1, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                }
                            }
                            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                            }
                        }
                       
                    }
                }
                break;

            // lcd
            case 7:
                arg1[0] = '\0';

                for (i = 0; i < strlen(auxInput); i++) {
                    if (auxInput[i] == ' ') {
                        break;
                    }
                }

                strcpy(arg1, &auxInput[i + 1]);

                char *temp = NULL;
                temp = executeLcd(&arg1[0], &error);

                if (temp == NULL) {
                    if (error == 1) {
                        printf ("acesso proibido / sem permissão\n");
                    }
                    else if (error == 2) {
                        printf ("diretório inexistente\n");
                    }
                    else if (error == 3) {
                        printf ("arquivo inexistente\n");
                    }
                    else if (error == 4) {
                        printf ("linha inexistente\n");
                    }
                    else {
                        printf ("Erro não documentado\n");
                    }
                }
                else {
                    strcpy(path, temp);
                }

                break;

            // lls
            case 8:
                printf ("\n");

                executeLls(&path[0]);

                break;

            // Invalid
            default:
                printf("Comando inválido\n");
                break;
        }

        printf ("\n");
    }
}