#include <sys/types.h>
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
char currPath[1000];

int error = 0;

char arg1[15];
char arg2[15];
char arg3[100000];

char textData[1000000];

int main() {
    char *temp = NULL;
    int index = 0;
    int EOT = 0;

    // Diretório atual
    char path[1035];
    getcwd(path, 1035);

    struct ifreq ir;
    struct sockaddr_ll endereco;
    struct packet_mreq mr;

    int socket = ConexaoRawSocket(DEVICE, ir, endereco, mr, &ret);

    if (socket < 0) {
        return 1;
    }

    Message sendingMsg;
    Message receivingMsg;

    Message reserveMsg;

    // printf("Esperando mensagens à serem recebidas.\n");

    long long i = 0;

    unsigned int sendSequence = 0;
    unsigned int recSequence = 0;

    int timeout = 0;

    // Inicia Loop de recebimentos
    while (1) {
        timeout = 0;
        
        i = 0;
        
        receivingMsg.mark = 0;

        while (receivingMsg.mark != MARKER) {
            if (i == 1) {
                reserveMsg = sendingMsg;

                // sendingMsg = createNACK(2, sendSequence);
                // send(socket, &sendingMsg, sizeof(sendingMsg), 0);
            }
            recv(socket, &receivingMsg, sizeof(receivingMsg), 0);
            i = 1;
        }

        // Checa a se a mensagem tem destino o servidor
        if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {

            // Checa sequência
            if (checkSequence(recSequence, receivingMsg)) {

                // Checa paridade
                if (checkParity(receivingMsg)) {
                    // Cria um switch case para saber qual o tipo de mensagem
                    switch (receivingMsg.type) {
                        // cd
                        case 0:
                            while (1) {
                                recSequence++;
                                recSequence = recSequence % 16;

                                char *temp = NULL;
                                temp = executeCd(receivingMsg, &error);

                                // Se a execução do cd retornou um erro, envia uma mensagem de erro para o client
                                if (error > 0) {
                                    sendingMsg = createErrorMessage(2, sendSequence, error);
                                    error = 0;

                                    reserveMsg = sendingMsg;

                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                    sendSequence++;
                                    sendSequence = sendSequence % 16;
                                }
                                else {
                                    sendingMsg = createACK(2, sendSequence);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                    reserveMsg = sendingMsg;

                                    sendSequence++;
                                    sendSequence = sendSequence % 16;

                                    strcpy(path, temp);
                                }

                                free(temp);

                                break;
                            }
                            break;

                        // ls
                        case 1:
                            reserveMsg = sendingMsg;

                            recSequence++;
                            recSequence = recSequence % 16;

                            sendingMsg = createACK(2, sendSequence);
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                            sendSequence++;
                            sendSequence = sendSequence % 16;

                            temp = NULL;
                            temp = executeLs(&path[0], &error);
                            
                            if (error > 0) {
                                reserveMsg = sendingMsg;

                                sendingMsg = createErrorMessage(2, sendSequence, error);

                                error = 0;

                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            else {
                                index = 0;
                                EOT = 0;

                                // envia uma sequência de bytes para o client
                                while (1) {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createLsData(&temp[index], sendSequence, &EOT, index);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                    sendSequence++;
                                    sendSequence = sendSequence % 16;

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
                                        else if (receivingMsg.mark == MARKER) {
                                            
                                            if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                                if (checkSequence(recSequence, receivingMsg)) {
                                                    if (checkParity(receivingMsg)) {

                                                        // Se não receber ACK, reenvia a mensagem  novamente
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

                                                        sendingMsg = createNACK(2, sendSequence);
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

                                free(temp);

                                reserveMsg = sendingMsg;

                                sendingMsg = createEOT(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                sendSequence++;
                                sendSequence = sendSequence % 16;

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
                                    else if (receivingMsg.mark == MARKER) {
                                        
                                        if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                            if (checkSequence(recSequence, receivingMsg)) {
                                                if (checkParity(receivingMsg)) {

                                                    // Se não receber ACK reenvia novamente
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

                                                    sendingMsg = createNACK(2, sendSequence);
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

                        // cat
                        case 2:
                            reserveMsg = sendingMsg;

                            sendingMsg = createACK(2, sendSequence);
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                            sendSequence++;
                            sendSequence = sendSequence % 16;

                            recSequence++;
                            recSequence = recSequence % 16;

                            temp = NULL;
                            temp = executeCat(&receivingMsg.data[0], &error);

                            if (error > 0) {
                                reserveMsg = sendingMsg;

                                sendingMsg = createErrorMessage(2, sendSequence, error);
                                
                                error = 0;

                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);


                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            else {
                                index = 0;
                                EOT = 0;

                                // envia uma sequência de bytes para o client
                                while (1) {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createCatData(&temp[index], sendSequence, &EOT, index);
                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                    sendSequence++;
                                    sendSequence = sendSequence % 16;

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
                                        else if (receivingMsg.mark == MARKER) {
                                            
                                            if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                                if (checkSequence(recSequence, receivingMsg)) {
                                                    if (checkParity(receivingMsg)) {
                                                            recSequence++;
                                                            recSequence = recSequence % 16;

                                                            // Se não receber ACK, reenvia a mensagem  novamente
                                                            if (receivingMsg.type == 0b1000) {
                                                                break;
                                                            }
                                                            else if (receivingMsg.type == 0b1001) {
                                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                                            }
                                                    }
                                                    else {
                                                        reserveMsg = sendingMsg;

                                                        sendingMsg = createNACK(2, sendSequence);
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

                                free(temp);

                                sendingMsg = createEOT(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                reserveMsg = sendingMsg;

                                sendSequence++;
                                sendSequence = sendSequence % 16;

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
                                    else if (receivingMsg.mark == MARKER) {
                                       
                                        if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
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

                                                    sendingMsg = createNACK(2, sendSequence);
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

                        // line
                        case 3:
                            reserveMsg = sendingMsg;

                            sendingMsg = createACK(2, sendSequence);
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            
                            sendSequence++;
                            sendSequence = sendSequence % 16;

                            recSequence++;
                            recSequence = recSequence % 16;

                            // copia o caminho do arquivo para a variável arg1
                            strcpy(arg2, receivingMsg.data);

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
                                else if (receivingMsg.mark == MARKER) {
                                   
                                    if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                        if (checkSequence(recSequence, receivingMsg)) {
                                            if (checkParity(receivingMsg)) {
                                                // espera receber os dados das linhas
                                                if (receivingMsg.type == 0b1010) {
                                                    
                                                    // copia as descrição da linha para uma variável
                                                    strcpy(arg1, receivingMsg.data);
                                                    recSequence++;
                                                    recSequence = recSequence % 16;

                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);
                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;

                                                    break;
                                                }
                                            }
                                            else {
                                                reserveMsg = sendingMsg;

                                                sendingMsg = createNACK(2, sendSequence);
                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                            }
                                        }
                                        else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                        }
                                    }
                                    
                                }
                            }

                            temp = NULL;
                            temp = executeLineCommand(&arg2[0], &arg1[0], &error);

                            if (error > 0) {
                                reserveMsg = sendingMsg;

                                sendingMsg = createErrorMessage(2, sendSequence, error);
                                
                                error = 0;

                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            else {
                                index = 0;
                                EOT = 0;

                                // envia uma sequência de bytes para o client
                                while (1) {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createCatData(&temp[index], sendSequence, &EOT, index);
                                    sendSequence++;
                                    sendSequence = sendSequence % 16;

                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                    
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
                                        else if (receivingMsg.mark == MARKER) {
                                            
                                            if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
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

                                                        sendingMsg = createNACK(2, sendSequence);
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

                                free(temp);

                                reserveMsg = sendingMsg;
                                
                                sendingMsg = createEOT(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;

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
                                    else if (receivingMsg.mark == MARKER) {
                                        
                                        if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                            if (checkSequence(recSequence, receivingMsg)) {
                                                if (checkParity(receivingMsg)) {

                                                    // Se não receber ACK, começa a reenviar tudo novamente
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

                                                    sendingMsg = createNACK(2, sendSequence);
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

                        // lines
                        case 4:
                            recSequence++;
                            recSequence = recSequence % 16;
                            
                            reserveMsg = sendingMsg;

                            sendingMsg = createACK(2, sendSequence);
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            
                            sendSequence++;
                            sendSequence = sendSequence % 16;
                            // copia o caminho do arquivo para a variável arg1
                            strcpy(arg2, receivingMsg.data);

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
                                else if (receivingMsg.mark == MARKER) {
                                    
                                    if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                        if (checkSequence(recSequence, receivingMsg)) {
                                            if (checkParity(receivingMsg)) {

                                                // espera receber os dados das linhas
                                                if (receivingMsg.type == 0b1010) {
                                                    
                                                    // copia as descrição da linha para uma variável
                                                    strcpy(arg1, receivingMsg.data);

                                                    recSequence++;
                                                    recSequence = recSequence % 16;

                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);
                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;

                                                    break;
                                                }
                                            }
                                             else {
                                                reserveMsg = sendingMsg;

                                                sendingMsg = createNACK(2, sendSequence);
                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                            }
                                        }
                                        else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                        }
                                    }
                                   
                                }
                            }

                            temp = NULL;
                            temp = executeLineCommand(&arg2[0], &arg1[0], &error);

                            if (error > 0) {
                                reserveMsg = sendingMsg;
                                
                                sendingMsg = createErrorMessage(2, sendSequence, error);
                                
                                error = 0;
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            else {
                                index = 0;
                                EOT = 0;

                                // envia uma sequência de bytes para o client
                                while (1) {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createCatData(&temp[index], sendSequence, &EOT, index);

                                    sendSequence++;
                                    sendSequence = sendSequence % 16;

                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

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
                                        else if (receivingMsg.mark == MARKER) {
                                            
                                            if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
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

                                                        sendingMsg = createNACK(2, sendSequence);
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

                                free(temp);
                                
                                reserveMsg = sendingMsg;

                                sendingMsg = createEOT(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;

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
                                    else if (receivingMsg.mark == MARKER) {
                                        
                                        if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
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

                                                    sendingMsg = createNACK(2, sendSequence);
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

                        // edit
                        case 5:
                            recSequence++;
                            recSequence = recSequence % 16;

                            reserveMsg = sendingMsg;

                            error = fileExists(&receivingMsg.data[0]);
                            
                            if (error) {
                                sendingMsg = createErrorMessage(2, sendSequence, 3);

                                error = 0;

                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                                
                                break;
                            }
                            else {
                                sendingMsg = createACK(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }

                            // copia o caminho do arquivo para a variável arg2
                            strcpy(arg2, receivingMsg.data);

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
                                else if (receivingMsg.mark == MARKER) {
                                    
                                    if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                        if (checkSequence(recSequence, receivingMsg)) {
                                            if (checkParity(receivingMsg)) {
                                                // espera receber os dados das linhas
                                                if (receivingMsg.type == 0b1010) {
                                                    
                                                    // copia a descrição da linha para uma variável
                                                    strcpy(arg1, receivingMsg.data);
                                                    recSequence++;
                                                    recSequence = recSequence % 16;

                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);
                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;

                                                    break;
                                                }
                                            }
                                            else {
                                                reserveMsg = sendingMsg;

                                                sendingMsg = createNACK(2, sendSequence);
                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                            }
                                        }
                                        else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                        }
                                    }
                                   
                                }
                            }
                            textData[0] = '\0';

                            // recebe as respostas do client
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
                                else if (receivingMsg.mark == MARKER) {
                                    
                                    if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                        if (checkSequence(recSequence, receivingMsg)) {
                                            if (checkParity(receivingMsg)) {

                                                strncat(textData, receivingMsg.data, 15);

                                                recSequence++;
                                                recSequence = recSequence % 16;

                                                // verifica se é o fim da transmissao
                                                if (receivingMsg.type == 0b1100) {
                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);

                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                                    
                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;
                                                }
                                                else if (receivingMsg.type == 0b1101) {
                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);

                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;

                                                    break;
                                                }
                                            }
                                            else {
                                                reserveMsg = sendingMsg;

                                                sendingMsg = createNACK(2, sendSequence);
                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                            }
                                        }
                                        else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                        }
                                    }
                                    
                                }
                            }

                            error = executeEditLine(&arg1[0], &textData[0], &arg2[0]);
                            
                            if (error == 0) {
                                reserveMsg = sendingMsg;

                                sendingMsg = createACK(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                
                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            else {
                                reserveMsg = sendingMsg;
                                
                                sendingMsg = createErrorMessage(2, sendSequence, error);

                                error = 0;

                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            
                            break;

                        // compile
                        case 6:
                            reserveMsg = sendingMsg;

                            sendingMsg = createACK(2, sendSequence);
                            send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                            
                            sendSequence++;
                            sendSequence = sendSequence % 16;

                            recSequence++;
                            recSequence = recSequence % 16;

                            arg2[0] = '\0';
                            // copia o caminho do arquivo para a variável arg1
                            strcpy(arg2, receivingMsg.data);

                            textData[0] = '\0';

                            // recebe as respostas do client
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
                                else if (receivingMsg.mark == MARKER) {
                                    
                                    if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                        if (checkSequence(recSequence, receivingMsg)) {
                                            if (checkParity(receivingMsg)) {

                                                strncat(textData, receivingMsg.data, 15);

                                                recSequence++;
                                                recSequence = recSequence % 16;

                                                // verifica se é o fim da transmissao
                                                if (receivingMsg.type == 0b0110) {
                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);

                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                                    
                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;
                                                }
                                                else if (receivingMsg.type == 0b1101) {
                                                    reserveMsg = sendingMsg;

                                                    sendingMsg = createACK(2, sendSequence);

                                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                                    sendSequence++;
                                                    sendSequence = sendSequence % 16;

                                                    break;
                                                }
                                            }
                                            else {
                                                reserveMsg = sendingMsg;

                                                sendingMsg = createNACK(2, sendSequence);
                                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                            }
                                        }
                                        else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);
                                        }
                                    }
                                    
                                }
                            }

                            temp = NULL;
                            temp = executeCompile(&arg2[0], &textData[0], &error);

                            if (error > 0) {
                                reserveMsg = sendingMsg;

                                sendingMsg = createErrorMessage(2, sendSequence, error);
                                
                                error = 0;

                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);

                                sendSequence++;
                                sendSequence = sendSequence % 16;
                            }
                            else {
                                index = 0;
                                EOT = 0;

                                // envia uma sequência de bytes para o client
                                while (1) {
                                    reserveMsg = sendingMsg;

                                    sendingMsg = createCatData(&temp[index], sendSequence, &EOT, index);
                                    sendSequence++;
                                    sendSequence = sendSequence % 16;

                                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);

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
                                        else if (receivingMsg.mark == MARKER) {
                                            
                                            if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
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

                                                        sendingMsg = createNACK(2, sendSequence);
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

                                free(temp);

                                reserveMsg = sendingMsg;

                                sendingMsg = createEOT(2, sendSequence);
                                send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                                sendSequence++;
                                sendSequence = sendSequence % 16;
                                

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
                                    else if (receivingMsg.mark == MARKER) {
                                        
                                        if (checkDest(receivingMsg) == 1 && checkSource(receivingMsg) == 0) {
                                            if (checkSequence(recSequence, receivingMsg)) {
                                                if (checkParity(receivingMsg)) {

                                                    // Se não receber ACK, começa a reenviar tudo novamente
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

                                                    sendingMsg = createNACK(2, sendSequence);
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

                        // NACK
                        case 9:
                            // renvia a última mensagem
                            send(socket, &reserveMsg, sizeof(reserveMsg), 0);

                        // Error
                        default:
                            printf("Erro ao receber mensagem\n");
                            break;
                    }
                }
                else {
                    reserveMsg = sendingMsg;

                    sendingMsg = createNACK(2, sendSequence);
                    send(socket, &sendingMsg, sizeof(sendingMsg), 0);
                }
            }
            else if (receivingMsg.sequence == (recSequence - 1) % 16) {
                send(socket, &reserveMsg, sizeof(reserveMsg), 0);
            }
        }
    }
}