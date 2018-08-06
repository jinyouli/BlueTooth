/*
 * protocol.c
 *
 *  Created on: 2016-09-07
 *      Author: ChenGuo
 */
 
#include "protocol.h"
#include "cJSON.h"
#include "aes.h"
#include "crc32.h"
#include "OCClass.h"
#include <stdio.h>
#include <ctype.h>

static char *lead = "SY";
static char version[2] = {0x10,0x00};
static unsigned char leadLength = 0;
myProtocol proto[NUMPROTO] = {0};

char protoPack[BUFLEN] = {0};
unsigned short protoLength = 0;
int ran = 0;
char randValue[16] = {};

 uint8_t protoR = 0;
 uint8_t protoW = 0;
static uint8_t leadIndex = 0;
static uint16_t protoLen  = 0;
static uint8_t status = 0;
uint32_t receiveTimeout = 0;

protoStatusType protoStatus = STA_WAIT;

//char *protoDefaultKey = "1234567890123456";
unsigned char protoDefaultKey[16] = {0xA1,0xC4,0xE0,0x60,0x8C,0xAB,0x0A,0x92,0x2C,0x24,0x51,0x58,0xBC,0x8F,0x9F,0x00};
uint8_t protoKey[16];
uint8_t protoTimer = 0;
uint8_t protoCount = 0;
uint8_t protoTickTimeout = PROTO_TICK_TIMEOUT;
uint8_t protoTickTimer = PROTO_TICK_TIMEOUT;
uint8_t protoTickPeriod = PROTO_TICK;
uint8_t protoTickPeriodTimer = PROTO_TICK;

uint8_t protoPackUpdateEn = 0;
uint8_t protoPackResendCount = 0;
uint32_t protoPackResendTimer = 0;
uint32_t protoTotalPack = 0;
uint32_t protoCurrPackNumber = 0;
uint32_t protoLastPackNumber = 0;
uint32_t protoFW_size = 0;
uint32_t protoFW_crc = 0;

protoInputType protoInput[7] =
{
	{1,1},
	{2,1},
	{3,1},
	{4,1},
	{5,1},
	{6,1},
	{7,1},
};

void ProtoTickReload(void);
void cb_ProtoTick(void *p,myProtocol *proto);
void cb_ProtoSign(void *p,myProtocol *proto);
void cb_ProtoRest(void *p,myProtocol *proto);
void cb_ProtoPack(void *p,myProtocol *proto);
void cb_ProtoBtn(void *p,myProtocol *proto);
void cb_ProtoConf(void *p,myProtocol *proto);
void cb_ProtoCard(void *p,myProtocol *proto);
void cb_ProtoPowerOn(void *p,myProtocol *proto);
void cb_ProtoUpdate(void *p,myProtocol *proto);
void cb_ProtoInput(void *p,myProtocol *proto);
void cb_ProtoGetInput(void *p,myProtocol *proto);
void cb_ProtoOutput(void *p,myProtocol *proto);
void cb_BlueToothOpen(void);
void HexStrToByte(const char* source, unsigned char* dest, int sourceLen);

typedef enum {
	cmdTick = 0,
	cmdSign,
	cmdRest,
	cmdPack,
	cmdBtn,
	cmdConf,
	cmdCard,
	cmdPowerOn,
	cmdUpdate,
	cmdInput,
	cmdGetInput,
	cmdOutput
}protoCmdID;

protoMap protoCmdMap[] = {
	{0,"4006",cb_ProtoConf},
	{0,"4103",cb_ProtoOutput},
	{0,NULL}//end of map
};


void ProtocolReset(void)
{
	leadIndex = 0;
	protoLen  = 0;
	status = 0;
	memset(&proto[protoW],0,sizeof(myProtocol));
}

void ProtocolInit(void)
{
	memset(&proto,0,NUMPROTO*(sizeof(myProtocol)));
	ProtocolReset();
	protoR = 0;
	protoW = 0;
	leadLength = strlen(lead);
	
	ProtocolTick();
}

void ProtocolRecevier(char *ch)
{
	char *p = proto[protoW].buf;
	unsigned short *len = &proto[protoW].pLen;

	switch (status)
	{
		case 0:
			if(*ch == lead[leadIndex]){
				p[(*len)++] = *ch;
				leadIndex++;
				if(lead[leadIndex] == '\0'){
					protoLen = 4;
					status = 1;
					leadIndex = 0;
				}
			}else{
				leadIndex = 0;
				*len = 0;
			}
			break;
		case 1:
			if(protoLen){
				p[(*len)++] = *ch;
				protoLen--;
				if(protoLen == 0){
					protoLen = *(p+leadLength+3);
					protoLen <<= 0x08;
					protoLen |= *(p+leadLength+2);

					if(protoLen <= BUFLEN - 3){
						status = 2;
					}else{
						status = 0;
					}
				}
			}
			break;
		case 2:
			if(*len != protoLen){
				p[(*len)++] = *ch;
				if(*len == protoLen){
					//proto[protoW].time = sysTimer;
					protoW++;
					if(protoW == NUMPROTO){
						protoW = 0;
					}
					ProtocolReset();
				}
			}
			break;
	}
	receiveTimeout = TIMEOUT;
}

myProtocol* GetProtocol(void)
{
	myProtocol *p = NULL;
	if(protoR ^ protoW){
		p = &proto[protoR];
		protoR++;
		if(protoR == NUMPROTO){
			protoR = 0;
		}
	}
	return p;
}


void ProtocolMessageParser(myProtocol *proto)
{
	cJSON *root,*typ,*cmd;
	root = cJSON_Parse(proto->json);
    
   // printf("我的测试===%s",proto->json);
    
	if(root != NULL){
		typ = cJSON_GetObjectItem(root,"typ");
		cmd = cJSON_GetObjectItem(root,"cmd");
		
		if((strcmp(typ->valuestring,"res") == 0) || (strcmp(typ->valuestring,"req") == 0)){
			ProtoTickReload();
			uint8_t i=0;
			while(protoCmdMap[i].callback != NULL){
				if((strcmp(cmd->valuestring,protoCmdMap[i].cmd) == 0)){
					protoCmdMap[i].callback(root,proto);
					if(strcmp(typ->valuestring,"res") == 0){
						protoCmdMap[i].isRespond = 0;
					}
				}
				i++;
			}
		}
		cJSON_Delete(root);
	}
}

unsigned long exchangeHL(unsigned long *d)//0x12345678-->0x78563412
{
    unsigned long tmp = 0;
    unsigned char *p1 = (unsigned char*)&tmp;
    unsigned char *p2 = (unsigned char*)d;
    *p1 = *(p2+3);
    *(p1+1) = *(p2+2);
    *(p1+2) = *(p2+1);
    *(p1+3) = *p2;
    return tmp;
}

char* ProtocolVerify(const char *buf,unsigned short len,char *mac,char *defaultMac)
{
	uint8_t i;
    unsigned long crc1 = Crc32_ComputeBuf(0,buf,len);
    crc1 = exchangeHL(&crc1);
	unsigned long crc2 = ~crc1;
	
	unsigned char *p = (unsigned char*)&crc1;
	unsigned char tmp1[4];
	unsigned char tmp2[4];
	
	for(i=0;i<4;i++){
		tmp1[3-i] = *(p+i);
	}
	p = (unsigned char*)&crc2;
	for(i=0;i<4;i++){
		tmp2[3-i] = *(p+i);
	}
	
	unsigned char key[16];
	unsigned char src[16];
	unsigned char enc[16];
    
//    printf("tmp1 == ");
//    for (int i=0; i<4; i++) {
//        printf("%02X ",tmp1[i]);
//    }
//    printf("\n");
//
//    printf("tmp2 == ");
//    for (int i=0; i<4; i++) {
//        printf("%02X ",tmp2[i]);
//    }
//    printf("\n");
    
	memcpy(&src[0],tmp1,4);
	memcpy(&src[4],tmp2,4);
	memcpy(&src[8],tmp1,4);
	memcpy(&src[12],tmp2,4);
    
    printf("%8X  ",crc1);
    
	if((protoStatus == STA_WAIT) || (protoStatus == STA_CONF)){
		memset(key,0xff,16);
	}else{
		memcpy(key,protoKey,16);
	}
    
//    printf("key == ");
//    for (int i=0; i<16; i++) {
//        printf("%02X ",key[i]);
//    }
//    printf("\n");
//
//    printf("src == ");
//    for (int i=0; i<16; i++) {
//        printf("%02X ",src[i]);
//    }
//    printf("\n");
	
	mbedtls_aes_context ctx;
	mbedtls_aes_init(&ctx);
	mbedtls_aes_setkey_enc(&ctx,(const unsigned char*)key,128);
	mbedtls_aes_crypt_ecb(&ctx,MBEDTLS_AES_ENCRYPT,src,enc);
	
//    printf("enc1 == ");
//    for(i=0;i<16;i++){
//        printf("%02X  ",enc[i]);
//    }
//    printf("\n");
    
	for(i=0;i<4;i++){
		mac[i] = enc[i] ^ enc[i+4] ^ enc[i+8] ^ enc[i+12];
	}

//    printf("mac == ");
//    for(i=0;i<4;i++){
//        printf("%02X  ",mac[i]);
//    }
//    printf("\n");
    
	memset(key,0xff,16);
	mbedtls_aes_free(&ctx);
	mbedtls_aes_init(&ctx);
	mbedtls_aes_setkey_enc(&ctx,(const unsigned char*)key,128);
	mbedtls_aes_crypt_ecb(&ctx,MBEDTLS_AES_ENCRYPT,src,enc);
    
	for(i=0;i<4;i++){
		defaultMac[i] = enc[i] ^ enc[i+4] ^ enc[i+8] ^ enc[i+12];
	}
	
	return mac;
}

void ProtocolParser(void)
{
	myProtocol *p = GetProtocol();
	
	if(receiveTimeout){
		receiveTimeout--;
		if(receiveTimeout == 0){
			ProtocolReset();
		}
	}
	if(p != NULL){
		char macCheck[4];
		char default_mac[4];
		char *mac = (p->buf + p->pLen - 4);
		int ret1 = memcmp(ProtocolVerify(p->buf,p->pLen - 4,macCheck,default_mac),mac,4);
		int ret2 = memcmp(default_mac,mac,4);

		if((ret1 == 0) || (ret2 == 0))
		{
			char *d = (char *)p + leadLength;
			p->da = *d;
			p->sa = *(d + 1);
			p->ver = *(d + 4);
			p->ver <<= 0x08;
			p->ver |= *(d + 5);
			p->jLen = *(d + 7);
			p->jLen <<= 0x08;
			p->jLen |= *(d + 6);
			p->json = d + 8;		
			memcpy(p->mac,p->buf + p->pLen - 4,4);
			
			p->eLen = p->pLen - p->jLen - leadLength - 10;
			if(p->eLen){
				unsigned short i = 0;
				unsigned short j = p->pLen - 4;
				for(i=0;i<p->eLen;i++){
					p->buf[j-i] = p->buf[j-i-1];
				}
				p->extra = p->json + p->jLen - 1;
				*(p->extra + p->eLen) = '\0';
			}else{
				p->extra = NULL;
			}
			
			*(p->json + p->jLen -2) = '\0';
			
			ProtocolMessageParser(p);
		}
	}
}

void ProtocolPack(char da,char sa,const char *json,const char *extra,unsigned short lenghtOfExtra)
{
	memset(protoPack,0,sizeof(protoPack));
	unsigned short lenghtOfJson = strlen(json) + 2;
	
	memcpy(protoPack,lead,leadLength);
	protoLength = leadLength;
	
	protoPack[protoLength++] = da;
	protoPack[protoLength++] = sa;
	protoLength += 2;
	
	protoPack[protoLength++] = version[0];
	protoPack[protoLength++] = version[1];
	
	protoPack[protoLength++] = lenghtOfJson & 0x00ff;
	protoPack[protoLength++] = lenghtOfJson >> 0x08;
	memcpy(&protoPack[protoLength],json,lenghtOfJson - 2);
	protoLength += lenghtOfJson - 2;
	
	if(extra != NULL){
		memcpy(&protoPack[protoLength],extra,lenghtOfExtra);
		protoLength += lenghtOfExtra;
	}
	
	protoLength += 4;
	protoPack[leadLength+2] = protoLength & 0x00ff;
	protoPack[leadLength+3] = protoLength >> 0x08;
	
	char mac[4];//package message check
	char def[4];//package message check
//	ConsoleSend(USART1,protoPack,protoLength-4);
	ProtocolVerify(protoPack,protoLength-4,mac,def);
	memcpy(&protoPack[protoLength-4],mac,4);
	
#ifdef DEBUG
	unsigned char i=0;
	char buf[512] = {0};
	for(i=0;i<protoLength;i++){
		sprintf(buf+i*3,"%02x ",protoPack[i]);
	}
	sprintf(buf+protoLength*3,"\n");
#endif
    
}

void cb_ProtoConf(void *p,myProtocol *proto)
{
    cJSON *root = (cJSON *)p,*cnt,*snr,*rand;
    
    cnt = cJSON_GetObjectItem(root,"cnt");
    snr = cJSON_GetObjectItem(cnt,"snr");
    rand = cJSON_GetObjectItem(cnt,"rand");
    char msg[MSG_SIZE];
    sprintf(msg,"{\"typ\":\"res\",\"cmd\":\"4006\",\"rsc\":\"0\",\"cnt\":{\"snr\":\"%s\"}}",rand->valuestring);
    ProtocolPack(0x00,0x01,msg,NULL,0);
    
    objcSayHello();
    
    HexStrToByte(rand->valuestring, randValue, 32);
    
    //printf("随机数值 == %s \n",rand->valuestring);
//    for (int i=0; i<16; i++) {
//        printf("随机值randValue == %02X \n",randValue[i]);
//    }
}

//十六进制字符串转换为字节流
void HexStrToByte(const char* source, unsigned char* dest, int sourceLen)
{
    short i;
    unsigned char highByte, lowByte;
    
    for (i = 0; i < sourceLen; i += 2)
    {
        highByte = toupper(source[i]);
        lowByte  = toupper(source[i + 1]);
        
        if (highByte > 0x39)
            highByte -= 0x37;
        else
            highByte -= 0x30;
        
        if (lowByte > 0x39)
            lowByte -= 0x37;
        else
            lowByte -= 0x30;
        
        dest[i / 2] = (highByte << 4) | lowByte;
    }
    return ;
}

void ProtocolConferKey(void)
{
    unsigned char key[16];
	unsigned char enc[16];

    for (int i=0; i<16; i++) {
        enc[i] = randValue[i];
    }
    
//    for (int i=0; i<16; i++) {
//        printf("随机值enc == %d \n",enc[i]);
//    }
    
	mbedtls_aes_context ctx;
	mbedtls_aes_init(&ctx);
	memcpy(key,protoDefaultKey,16);
	mbedtls_aes_setkey_enc(&ctx,(const unsigned char*)key,128);
	mbedtls_aes_crypt_ecb(&ctx,MBEDTLS_AES_ENCRYPT,enc,protoKey);
    
    
	
//    char str[33];
//    char msg[MSG_SIZE];
//
//    memset(str,0,sizeof(str));
//    memset(msg,0,sizeof(msg));
//    for(i=0;i<16;i++){
//        sprintf(str+i*2,"%02X",enc[i]);
//    }
//    sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"snr\":\"%08X\",\"rand\":\"%s\"}}",\
		protoCmdMap[cmdConf].cmd,chipIDcrc,str);
	//ProtocolPack(0x00,0x01,msg,NULL,0);
//	ConsoleSend(USART1,protoPack,protoLength);
}

void ProtocolSignIn(void)
{
	char msg[MSG_SIZE];
//    sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"snr\":\"%08X\",\"hwv\":\"%s\",\"fwv\":\"%s\"}}",\
//        protoCmdMap[cmdSign].cmd,chipIDcrc,getHardwareVersion(),getSoftwareVersion());
	ProtocolPack(0x00,0x01,msg,NULL,0);
	//ConsoleSend(USART1,protoPack,protoLength);
}

void ProtocolTick(void)
{
	char msg[MSG_SIZE];
//    sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"snr\":\"%08X\"}}",protoCmdMap[cmdTick].cmd,chipIDcrc);
	ProtocolPack(0x00,0x01,msg,NULL,0);
	//ConsoleSend(USART1,protoPack,protoLength);
}

void protocolChangeStatus(protoStatusType newStatus)
{
	protoStatus = newStatus;
	protoTimer = 0;
	protoCount = 0;
	if(newStatus == STA_CONF){
		protoTickPeriod = PROTO_TICK;
		protoTickTimeout = PROTO_TICK_TIMEOUT;
		protoTickTimer = PROTO_TICK_TIMEOUT;
		
		protoPackUpdateEn = 0;
	}
}

void ProtocolTask(void)
{
	if(protoTimer){
		protoTimer--;
	}else{
		switch (protoStatus)
		{
			case STA_CONF:
				ProtocolConferKey();
				protoTimer = protoCount*2 + 1;
				protoCount++;
				if(protoCount == RESEDN){
					#if 0
					protocolChangeStatus(STA_WAIT);
					#else
					//sysReset();
					#endif
				}
				break;
			case STA_SIGN:
				ProtocolSignIn();
				protoTimer = protoCount*2 + 1;
				protoCount++;
				if(protoCount == RESEDN){
					#if 0
					protocolChangeStatus(STA_WAIT);
					#else
					//sysReset();
					#endif
				}
				break;
			case STA_WAIT:
				break;
			case STA_WORK:
				break;
		}
	}
	
	if(protoTickPeriodTimer){
		protoTickPeriodTimer--;
	}else{
		ProtocolTick();
		protoTickPeriodTimer = protoTickPeriod;
	}
	
	if(protoStatus == STA_WORK){
		if(protoTickTimer){
			protoTickTimer--;
		}else{
			//expowerReset();
			protocolChangeStatus(STA_WAIT);
		}
	}
}

void ProtocolSendTouchKeyboard(char keyValue)
{
	if(protoStatus == STA_WORK){
		char msg[MSG_SIZE];
//        sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"snr\":\"%08X\",\"vkc\":%d}}",protoCmdMap[cmdBtn].cmd,chipIDcrc,keyValue);
		ProtocolPack(0x00,0x01,msg,NULL,0);
		//ConsoleSend(USART1,protoPack,protoLength);
	}
}

void ProtocolSendCardID(char type,char *ID ,unsigned char len)
{
	if(protoStatus == STA_WORK){
		unsigned char i=0;
		char id[32];
		memset(id,0,32);
		for(i=0;i<len;i++){
			sprintf(id+i*2,"%02X",ID[i]);
		}
		char msg[MSG_SIZE];
//        sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"typ\":%d,\"cid\":\"%s\",\"snr\":\"%08X\"}}",protoCmdMap[cmdCard].cmd,type,id,chipIDcrc);
		ProtocolPack(0x00,0x01,msg,NULL,0);
		//ConsoleSend(USART1,protoPack,protoLength);
	}
}

void ProtoTickReload(void)
{
	if(protoStatus == STA_WORK){
		protoTickTimer = protoTickTimeout;
	}
	protoTickPeriodTimer = protoTickPeriod;
}


void ProtocolSendInputEvent(protoInputEventType event)
{
	if(protoStatus == STA_WORK){
		char msg[MSG_SIZE];
//        sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"snr\":\"%08X\",\"ion\":%d,\"iov\":%d}}",\
//            protoCmdMap[cmdInput].cmd,chipIDcrc,protoInput[event].number,protoInput[event].value);
		ProtocolPack(0x00,0x01,msg,NULL,0);
		//ConsoleSend(USART1,protoPack,protoLength);
	}
}

void ProtocolGetPack(uint32_t packNumber)
{
	if(protoStatus == STA_WORK){
		char msg[MSG_SIZE];
//        sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"%s\",\"cnt\":{\"snr\":\"%08X\",\"hwv\":\"%s\",\"fwv\":\"%s\",\"pno\":%d}}",\
//            protoCmdMap[cmdPack].cmd,chipIDcrc,getHardwareVersion(),getSoftwareVersion(),packNumber);
		ProtocolPack(0x00,0x01,msg,NULL,0);
		//ConsoleSend(USART1,protoPack,protoLength);
	}
}

void protocolPackUpdateTask(void)
{
	if(protoPackUpdateEn){
        if(protoPackResendTimer){
            protoPackResendTimer--;
            if(protoPackResendTimer == 0){
                protoPackResendCount++;
                protoPackResendTimer = protoPackResendCount*100;
                if(protoPackResendCount == 10){
                    protoPackUpdateEn = 0;
                    //ConsolePrint(USART1,"Pack update timeout\n");
                }
                ProtocolGetPack(protoCurrPackNumber);
            }
        }
	}
}

void cb_ProtoTick(void *p,myProtocol *proto)
{
	cJSON *root = (cJSON *)p,*cnt,*snr,*rsc;
	rsc = cJSON_GetObjectItem(root,"rsc");
	cnt = cJSON_GetObjectItem(root,"cnt");
	snr = cJSON_GetObjectItem(cnt,"snr");
	char id[9];
	
//    memset(id,0,sizeof(id));
//    //sprintf(id,"%08X",chipIDcrc);
//
//    if((strcmp(snr->valuestring,id) == 0) && (strcmp(rsc->valuestring,"0") == 0)){
//        ProtoTickReload();
//        if(protoStatus == STA_WAIT){
//            protocolChangeStatus(STA_CONF);
//        }
//    }
}

void cb_ProtoSign(void *p,myProtocol *proto)
{
	cJSON *root = (cJSON *)p,*cnt,*snr,*mti;
	cJSON *fwv,*psz,*psm,*crc;
	cnt = cJSON_GetObjectItem(root,"cnt");
	snr = cJSON_GetObjectItem(cnt,"snr");
	mti = cJSON_GetObjectItem(cnt,"mti");
	
	fwv = cJSON_GetObjectItem(cnt,"fwv");
	psz = cJSON_GetObjectItem(cnt,"psz");
	psm = cJSON_GetObjectItem(cnt,"psm");
	crc = cJSON_GetObjectItem(cnt,"crc");
	char id[9];
	
//    memset(id,0,sizeof(id));
//    //sprintf(id,"%08X",chipIDcrc);
//    if((strcmp(snr->valuestring,id) == 0)){
//        if(mti != NULL){
//            protoTickTimeout = mti->valueint;
//            protoTickTimer = protoTickTimeout;
//        }
//        if(strcmp(fwv->valuestring,getSoftwareVersion())){
//            if((psz != NULL) && (psm != NULL) && (crc != NULL)){
//                protoTotalPack = psm->valueint;
//                protoFW_size = psz->valueint;
//                protoFW_crc = crc->valuedouble;
//                protoPackUpdateEn = 1;
//                protoCurrPackNumber = 0;
//                protoLastPackNumber = 1;
//                protoPackResendTimer = 0;
//                prepareEraseBIN(ADDR_SW_BIN1);
//                eraseBIN(ADDR_SW_BIN1);
//                writeBIN(&protoFW_crc,4);
//                writeBIN(&protoFW_size,4);
//            }
//        }
//        protocolChangeStatus(STA_WORK);
//    }
}

void cb_ProtoRest(void *p,myProtocol *proto)
{
	cJSON *root = (cJSON *)p,*cnt,*dly;
	cnt = cJSON_GetObjectItem(root,"cnt");
	dly = cJSON_GetObjectItem(cnt,"dly");
	
//    char msg[MSG_SIZE];
//    sprintf(msg,"{\"typ\":\"res\",\"cmd\":\"%s\",\"rsc\":\"0\"}",protoCmdMap[cmdRest].cmd);
//    ProtocolPack(0x00,0x01,msg,NULL,0);
//    //ConsoleSend(USART1,protoPack,protoLength);
//
//    if(dly != NULL ){
//        if(dly->valueint){
//            //addTimeTask((callback)sysReset,0,dly->valueint,LOOP_TASK);
//        }else{
//            //sysReset();
//        }
//    }
}

void cb_ProtoPack(void *p,myProtocol *proto)
{
	cJSON *root = (cJSON *)p,*rsc,*cnt,*pno;

	rsc = cJSON_GetObjectItem(root,"rsc");
	cnt = cJSON_GetObjectItem(root,"cnt");
	pno = cJSON_GetObjectItem(cnt,"pno");
//#if 1
//    if((strcmp(rsc->valuestring,"0") == 0)){
//        if(pno->valueint == protoCurrPackNumber){
//            //writeBIN(proto->extra,proto->eLen);
//            protoCurrPackNumber++;
//            if(protoCurrPackNumber == protoTotalPack){
//                protoPackUpdateEn = 0;
////                if(Bin_CRC_Check(ADDR_SW_BIN1)){
////                    setUpdateFlag();
////                }
//            }
//        }
//    }
//#endif
}

void cb_ProtoBtn(void *p,myProtocol *proto)
{

}

void cb_ProtoCard(void *p,myProtocol *proto)
{

}

void cb_ProtoPowerOn(void *p,myProtocol *proto)
{
//    char msg[MSG_SIZE];
//    sprintf(msg,"{\"typ\":\"res\",\"cmd\":\"%s\",\"rsc\":\"0\"}",protoCmdMap[cmdPowerOn].cmd);
//    ProtocolPack(0x00,0x01,msg,NULL,0);
////    ConsoleSend(USART1,protoPack,protoLength);
//
//    protocolChangeStatus(STA_CONF);
}

void cb_ProtoUpdate(void *p,myProtocol *proto)
{

}

void cb_ProtoInput(void *p,myProtocol *proto)
{

}

void cb_ProtoGetInput(void *p,myProtocol *proto)
{
    if(protoStatus == STA_WORK){
//        char msg[MSG_SIZE];
//        sprintf(msg,"{\"typ\":\"res\",\"cmd\":\"%s\",\"rsc\":\"0\",\"cnt\":{\"snr\":\"%08X\",\"iovs\":[%d,%d,%d,%d,%d,%d,%d]}}",\
//            protoCmdMap[cmdGetInput].cmd,chipIDcrc,protoInput[door_btn1].value,protoInput[door_btn2].value,protoInput[brightness].value,\
//            protoInput[body_detect].value,protoInput[warning_btn].value,protoInput[door_sta1].value,protoInput[door_sta2].value);
//        ProtocolPack(0x00,0x01,msg,NULL,0);
//        ConsoleSend(USART1,protoPack,protoLength);
    }
}

void cb_ProtoOutput(void *p,myProtocol *proto)
{
    cJSON *root = (cJSON *)p,*cnt,*snr;
    
    cnt = cJSON_GetObjectItem(root,"cnt");
    snr = cJSON_GetObjectItem(cnt,"snr");
    char msg[MSG_SIZE];
    sprintf(msg,"{\"typ\":\"res\",\"cmd\":\"4103\",\"rsc\":\"0\",\"cnt\":{\"snr\":\"%s\"}}",snr->valuestring);
    ProtocolPack(0x00,0x01,msg,NULL,0);
    
    printf("蓝牙反馈获取到==%s",snr->valuestring);
    
    objcReceiveBluetooth();
    
//    if(protoStatus == STA_WORK){
//        char id[9];
//        cJSON *root = (cJSON *)p,*cnt,*snr,*ion,*iov;
//        cnt = cJSON_GetObjectItem(root,"cnt");
//        snr = cJSON_GetObjectItem(cnt,"snr");
//        ion = cJSON_GetObjectItem(cnt,"ion");
//        iov = cJSON_GetObjectItem(cnt,"iov");
//
//        memset(id,0,sizeof(id));
////        sprintf(id,"%08X",chipIDcrc);
//        if((strcmp(snr->valuestring,id) == 0)){
//            if((ion != NULL) && (iov != NULL)){
//                if(iov->valueint){
//                    switch(ion->valueint){
//                        case 1:
//                            if(iov->valueint == 1){
//                                //ioClassSet(&relay1,1000,1100,1);
//                            }else{
//                                //ioClassSet(&relay1,iov->valueint,iov->valueint+100,1);
//                            }
//                            break;
//                        case 2:
//                            if(iov->valueint == 1){
//                                //ioClassSet(&relay2,1000,1100,1);
//                            }else{
////                                ioClassSet(&relay2,iov->valueint,iov->valueint+100,1);
//                            }
//                            break;
//                        case 3:
//                            //ioClassSet(&beep,iov->valueint,iov->valueint+100,1);
//                            break;
//                    }
//                    char msg[MSG_SIZE];
//                    sprintf(msg,"{\"typ\":\"res\",\"cmd\":\"%s\",\"rsc\":\"0\"}",protoCmdMap[cmdOutput].cmd);
//                    ProtocolPack(0x00,0x01,msg,NULL,0);
//                    //ConsoleSend(USART1,protoPack,protoLength);
//                }
//            }
//        }
//    }
}

void cb_BlueToothOpen()
{
    
}


