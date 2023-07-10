#ifndef __AUX__
#define __AUX__

#define MARKER 0b01111110
#define SERVER "10"
#define CLIENT "01"
#define DEVICE "lo"
#define TIMEOUT 10

struct ifreq;
struct sockaddr_ll;
struct packet_mreq;

#include <stdlib.h>

// estrutura da mensagem
typedef struct {
    char mark;
    unsigned int destAddress : 2;
    unsigned int sourceAddress : 2;
    unsigned int size : 4;
    unsigned int sequence : 4;
    unsigned int type : 4;
    char data[15];
    char parity;
} Message;

int ConexaoRawSocket(char *device, struct ifreq ir, struct sockaddr_ll endereco, struct packet_mreq mr, int *ret);
int checkDest(Message msg);
int checkSource(Message msg);
int checkSequence(int seq, Message msg);
int compareCommand(char *input);
char *replaceWord(char *s, char *oldW, char* newW);
void printError(Message msg);
int fileExists(char *currPath);

// paridade
int checkParity(Message msg);
void createParity(Message *msg);

// ls
char* executeLs(char *currPath, int *error);
Message createLs(int dest, int seq);
Message createLsData(char *data, int sequence, int *EOT, int index);

// ack e nack
Message createACK(int dest, int seq);
Message createNACK(int dest, int seq);

// cd
Message createCd(int dest, int seq, char *path);
char* executeCd(Message msg, int *error);

// error
Message createErrorMessage(int dest, int seq, int error);

// lcd
char *executeLcd(char *cdPath, int *error);

// lls
void executeLls(char *currPath);

// cat
Message createCat(int dest, int seq, char *path);
char* executeCat(char *currPath, int *error);
Message createCatData(char *data, int sequence, int *EOT, int index);

// line
Message createLineCommand(int dest, int seq, char *path);
Message createStartEndLine(int dest, int seq, char *path);
char* executeLineCommand(char *currPath, char *line, int *error);

// lines
Message createLinesCommand(int dest, int seq, char *path);

// EOT
Message createEOT(int dest, int sequence);

// edit
Message createEdit(int dest, int seq, char *path);
Message createTextData(char *data, int sequence, int *EOT, int index);
int executeEditLine(char *line, char *text, char *file);

// compile
Message createCompileCommand(int dest, int seq, char *path);
Message createCompileArgs(char *data, int sequence, int *EOT, int index);
char* executeCompile(char *currPath, char *args, int *error);

// Recebe a mensagem, atrelado com timeout
int receiveMessage(Message *msg, int socket);

#endif