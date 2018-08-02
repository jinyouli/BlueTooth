/*
 * protocol.h
 *
 *  Created on: 2016-09-07
 *      Author: ChenGuo
 */
#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUMPROTO  0x03
#define TIMEOUT   5
#define BUFLEN    384
#define PROTO_TIMEOUT   100
#define MSG_SIZE  512

#define PROTO_TICK_TIMEOUT 200
#define PROTO_TICK  5
#define RESEDN      5

/*



*/
typedef enum _proto_status{
	STA_WAIT,
	STA_CONF,
	STA_SIGN,
	STA_WORK,
}protoStatusType;

typedef enum _proto_input_envnt{
	door_btn1 = 0,
	door_btn2,
	brightness,
	body_detect,
	warning_btn,
	door_sta1,
	door_sta2,
}protoInputEventType;

typedef struct _myprotocol{
    char           buf[BUFLEN];
    unsigned short pLen;   //length of protocol
    unsigned short jLen;   //length of json message
    unsigned short eLen;   //length of extra message
    char           da;     //destination address
    char           sa;     //source address
    unsigned short ver;   //version of protocol
    char           *json;  //json message
    char           *extra; //extra message
    char           mac[4];   //mac check
    unsigned int time;
}myProtocol;

typedef void (*protoFunc)(void *p,myProtocol *proto);

typedef struct _map{
    unsigned char isRespond;
    const char *cmd;
    protoFunc callback;
}protoMap;

typedef struct _proto_input{
	const unsigned char number;
	unsigned char value;
}protoInputType;


extern int ran;
extern char randValue[16];
extern char protoPack[BUFLEN];
extern unsigned short protoLength;
extern unsigned int receiveTimeout;
extern protoInputType protoInput[7];
extern uint8_t protoKey[16];

void ProtocolReset(void);
void ProtocolInit(void);
void ProtocolRecevier(char *ch);
void ProtocolParser(void);
void ProtocolPack(char da,char sa,const char *json,const char *extra,unsigned short lenghtOfExtra);
void ProtocolTask(void);
void ProtocolTick(void);
void ProtocolSendTouchKeyboard(char keyValue);
void ProtocolSendCardID(char type,char *ID ,unsigned char len);
void ProtocolSendInputEvent(protoInputEventType event);
void protocolPackUpdateTask(void);
void protocolChangeStatus(protoStatusType newStatus);
void ProtocolConferKey(void);

#endif
