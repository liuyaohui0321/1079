#ifndef __CMD_H_
#define __CMD_H_

#include "xbasic_types.h"
#include "xil_types.h"
#include "fat/ff.h"
#include "stdio.h"
/************2023.9.11 加**************/
#include "xparameters.h"
#include "simple_dma.h"

extern XAxiDma AxiDma;

#define OFFSET_SIZE             2*2*2*2*2*1024*1024   // 32M
#define SRC_ID		0x0  //
#define DEST_ID		0x0

#define CMD_PACK_LEN		(1024*10)
//#define Data_Pack_Length	(1024*9)

//#define A201_DATA_LEN		8//(2052)
//#define A201_DATA_LEN		26
#define A201_DATA_LEN		14
//#define A201_DATA_LEN		50
//#define A202_DATA_LEN		(16)
//#define A201_DATA_LEN		20
#define A202_DATA_LEN		12
//#define A201_DATA_LEN		24
//#define A204_DATA_LEN		(6164)
#define A204_DATA_LEN		40

//#define A205_DATA_LEN		(4100)
#define A205_DATA_LEN		14
#define A206_DATA_LEN		(2352)
#define A207_DATA_LEN		(2350)

#define B201_DATA_LEN		(4)

//#define D201_DATA_LEN		(2056)
#define D201_DATA_LEN		20
#define D202_DATA_LEN		(0)
#define D203_DATA_LEN		(2048)
#define D204_DATA_LEN		(2056)
#define D205_DATA_LEN		(2056)
#define D206_DATA_LEN		(2048)
#define D207_DATA_LEN		(16)
#define D208_DATA_LEN		(16)

#define F201_DATA_LEN		(32)

#define     MSG_MAX_DATA_LEN    6164

#define		MSG_QUERY		12	 //wfeng


//#define NEW_FILE			0X01
//#define NEW_FOLDER			0X02
//#define DEL_FILE			0X03
//#define DEL_FOLDER			0X04
//#define RENAME_FILE			0X05
//#define RENAME_FOLDER		0X06
//#define MOVE_FOLDER			0X07
//#define MOVE_FILE			0X08
//#define OPEN_FILE			0X09
//#define CLOSE_FILE			0X10
//#define COPY_FILE			0X11
//#define COPY_FOLDER			0X12
//#define GET_DIR				0X13

#define NEW_FILE			1   //1.31号改   客户要把16进制数改成10进制数
#define NEW_FOLDER			2
#define DEL_FILE			3
#define DEL_FOLDER			4
#define RENAME_FILE			5
#define RENAME_FOLDER		6
#define MOVE_FOLDER			7
#define MOVE_FILE			8
#define OPEN_FILE			9
#define CLOSE_FILE			10
#define COPY_FILE			11
#define COPY_FOLDER			12
#define GET_DIR				13

//#define DISK_FORMAT			0X01
//#define DISK_REMOUNT		0X02
//#define DISK_UNMOUNT		0X03

#define DISK_FORMAT			1
#define DISK_REMOUNT		2
#define DISK_UNMOUNT		3

#define FILE_ATTRIBUTE		0X0
#define FOLDER_ATTRIBUTE	0X01


#define		MSG_NULL		 	0x00
//#define		MSG_SERIAL			0x01	/
#define		MSG_WARNING			0x02
//
//#define		MSG_CMD_INPUT		0x10
//

#define		WARNING_MSG_OVERFLOW	0x01
//#define		WARNING_DISK_OVERFLOW	0x02
//#define	    WARNING_FILE_OVERFLOW   0x03
//#define		WARNING_DISK_W_ERR		0x04
//#define		WARNING_DISK_R_ERR		0x05
//#define		WARNING_DMA_TX_ERR		0x06
//#define		WARNING_DMA_RX_ERR		0x07


//时间信息结构体
typedef struct timeNode
{
	u8  type;
	u8	name[1024];		//名称
	u8	time[48];		//时间
}__attribute__((__packed__))TimeStruct;

//创建时间链表
typedef struct createtimeNode
{
	TimeStruct data;
	struct createtimeNode *next;
}CreateTimeNode,*CreateTimeList;

//修改时间链表
typedef struct changetimeNode
{
	TimeStruct data;
	struct changetimeNode *next;
}ChangeTimeNode,*ChangeTimeList;

//访问时间链表
typedef struct accesstimeNode
{
	TimeStruct data;
	struct accesstimeNode *next;
}AccessTimeNode,*AccessTimeList;

typedef	struct
{
	u32 HandType;			//
	u32 HandId;			//
	u32	DataLen;
	u32	PackNum;
	u8 	MsgData[MSG_MAX_DATA_LEN];

}__attribute__((__packed__)) StructMsg;


typedef	struct
{
	u8 Start;
	u8 End;
	StructMsg MsgQuery[MSG_QUERY];
}__attribute__((__packed__)) StructMsgQuery;

void MsgQueryInit(void);
void GetMessage(StructMsg *pMsg);
void SendMessage(StructMsg *pMsg);

extern	StructMsg		  	CurMsg;
extern	StructMsgQuery		MsgQuery;

extern CreateTimeList	CreateTimelist;
extern ChangeTimeList	ChangeTimelist;
extern AccessTimeList	AccessTimelist;

int cmd_parse(void);
void InitTimeList(void);
int cmd_reply_a203_to_a201(u32 packnum, u32 type, u32 id, u32 result);
int cmd_reply_a203_to_a204(u32 packnum, u32 type, u32 id, u32 result);
int cmd_reply_a203_to_a205(u32 packnum, u32 type, u32 id, u32 result);
int cmd_reply_a203_to_a208(u32 packnum, u32 type, u32 id, u32 result);

int cmd_reply_a203_to_b201(u32 packnum, u32 type, u32 id, u32 result);
int cmd_reply_a203_to_d201(u32 packnum, u32 type, u32 id, u32 result);
int cmd_reply_a203_to_f201(u32 packnum, u32 type, u32 id, u32 result);

int cmd_reply_a206(StructMsg *pMsg, const BYTE* path);
int cmd_reply_a207(StructMsg *pMsg, const BYTE* path);
//int cmd_reply_a208(StructMsg *pMsg, BYTE* path);
int cmd_reply_a208(BYTE* path);
int run_reply_d207(StructMsg *pMsg);
int run_reply_d208(StructMsg *pMsg);
int cmd_reply_health_f201(void);

void cmd_type_id_parse(StructMsg *pMsg);

int run_cmd_a201(StructMsg *pMsg);
int run_cmd_a202(StructMsg *pMsg);
int run_cmd_a204(StructMsg *pMsg);
int run_cmd_a205(StructMsg *pMsg);
//int run_cmd_a208(StructMsg *pMsg);
int run_cmd_b201(StructMsg *pMsg);

int run_cmd_d201(StructMsg *pMsg);
int run_cmd_d202(StructMsg *pMsg);
int run_cmd_d203(StructMsg *pMsg);
int run_cmd_d204(StructMsg *pMsg);
int run_cmd_d205(StructMsg *pMsg);
int run_cmd_d206(StructMsg *pMsg);
int run_cmd_f201(StructMsg *pMsg);

//typedef struct
//{
//	u32		Head;
//	u32		SrcId;
//	u32		DestId;
//	u32		HandType;
//	u32		HandId;
//	u32		PackNum;
////	u32		DataPack[Data_Pack_Length];
//	u32		CheckCode;
//	u32		Tail;
//}__attribute__((__packed__))  StructCmd;


typedef struct
{
	u8		Name[1024];
	u8		Dir[1024];
	u64		Size;
	u8		CreateTime1[48];
	u8		CreateTime2[48];
	u8		ChangeTime1[48];
	u8		ChangeTime2[48];
	u8		AccessTime1[48];
	u8		AccessTime2[48];
	u32		RwCtrl;
	u32		DisplayCtrl;
}__attribute__((__packed__))  StructFlieAttri;


typedef struct
{
	u8		Name[1024];
	u8		Dir[1024];
	u64		Size;
	u32		SubFolderNum;
	u32		SubFileNum;
	u8		CreateTime1[48];
	u8		CreateTime2[48];
	u8		ChangeTime1[48];
	u8		ChangeTime2[48];
	u8		AccessTime1[48];
	u8		AccessTime2[48];
	u32		RWCtrl;
	u32		DisplayCtrl;
}__attribute__((__packed__))  StructFolderAttri;

typedef struct
{
	u32		Head;
	u32		SrcId;
	u32		DestId;
	u32		HandType;
	u32		HandId;
	u32		PackNum;
	u32		AckPackNum;
	u32		AckHandType;
	u32		AckHandId;
	u32		AckResult;
	u32		backups[4];       // 1.31按照客户协议增加
	u32		CheckCode;
	u32		Tail;
}__attribute__((__packed__))  StructA203Ack;

typedef struct
{
	u32		Head;
	u32		SrcId;
	u32		DestId;
	u32		HandType;
	u32		HandId;
	u32		PackNum;
//	u8		Name[1024];
//	u8		Dir[1024];
	u16		Name[512];
	u16		Dir[512];
	u64		Size;
//	u32		SubFolderNum;
//	u32		SubFileNum;
//	u8		CreateTime1[48];
//	u8		CreateTime2[48];
//	u8		ChangeTime1[48];
//	u8		ChangeTime2[48];
//	u8		AccessTime1[48];
//	u8		AccessTime2[48];
	u16		CreateTime1[24];
	u16		CreateTime2[24];
	u16		ChangeTime1[24];
	u16		ChangeTime2[24];
	u16		AccessTime1[24];
	u16		AccessTime2[24];
	u32		RWCtrl;
	u32		DisplayCtrl;
	u32		backups[12];       // 1.31按照客户协议增加
	u32		CheckCode;
	u32		Tail;
}__attribute__((__packed__))  StructA206Ack;

typedef struct
{
	u32		Head;
	u32		SrcId;
	u32		DestId;
	u32		HandType;
	u32		HandId;
	u32		PackNum;
//	u8		Name[1024];
//	u8		Dir[1024];
	u16		Name[512];
	u16		Dir[512];
	u64		Size;
	u32		SubFolderNum;
	u32		SubFileNum;
//	u8		CreateTime1[48];
//	u8		CreateTime2[48];
//	u8		ChangeTime1[48];
//	u8		ChangeTime2[48];
//	u8		AccessTime1[48];
//	u8		AccessTime2[48];
	u16		CreateTime1[24];
	u16		CreateTime2[24];
	u16		ChangeTime1[24];
	u16		ChangeTime2[24];
	u16		AccessTime1[24];
	u16		AccessTime2[24];
	u32		RWCtrl;
	u32		DisplayCtrl;
	u32		backups;       // 1.31按照客户协议增加
	u32		CheckCode;
	u32		Tail;
}__attribute__((__packed__))  StructA207Ack;


typedef struct
{
	u32		Head;
	u32		SrcId;
	u32		DestId;
	u32		HandType;
	u32		HandId;
	u32		PackNum;
	u32		AckHandType;
	u32		AckHandId;
	u32		FileNum;
	u32		DirNum;
	u32     SubpackNum;
	u32     LastPack;
	u32     SubpackFileNum;
	u32     SubpackDirNum;
	u32		CheckCode;
	u32		Tail;
	SingleFileOrDir *message;
}__attribute__((__packed__))  StructA208Ack;

//
typedef struct
{
	u32		Head;
	u32		SrcId;
	u32		DestId;
	u32		HandType;
	u32		HandId;
	u32		PackNum;
	u64		TotalCap;
	u64		UsedCap;
	u64		RemainCap;
	u32		FileNum;
	u32		WorkStatus;
	u32		WorkTemp;
	u32		Power;
	u32		PowerUpNum;
	u32		CheckCode;
	u32		Tail;
}__attribute__((__packed__))  StructHealthStatus;

//StructCmd RecvStructCmd;
//StructA203Ack SendStructA203Ack;
//StructHealthStatus SendStructHealthStatus;





#define CW16(a,b) ( (uint16_t)((((uint16_t)(a))<<8) | ((uint16_t)b)) )			//改之前
//#define CW16(a,b) ( (uint16_t)((((uint16_t)(a))) | ((uint16_t)b)<<8))			//改之后

//#define CW32(a,b,c,d) ((uint32_t)((((uint32_t)(a))<<24) | (((uint32_t)b)<<16)| (((uint32_t)c)<<8)| ((uint32_t)d)))     //改之前
#define CW32(a,b,c,d) ((uint32_t)((((uint32_t)(a))) | (((uint32_t)b)<<8)| (((uint32_t)c)<<16)| ((uint32_t)d)<<24))   //改之后

#define SW16(x) ((uint16_t)((((uint16_t)(x)&0x00ffU)<<8) | \
							(((uint16_t)(x)&0xff00U)>>8) ))

#define SW32(x) ((uint32_t)((x&0xff)<<24)|\
							(uint32_t)((x&0xff00)<<8)|\
							(uint32_t)((x&0xff0000)>>8)|\
							(uint32_t)((x&0xff000000)>>24))

#define SW64(x) ((uint64_t)((x&0xff)<<56)|\
							(uint64_t)((x&0xff00)<<40)|\
							(uint64_t)((x&0xff0000)<<24)|\
							(uint64_t)((x&0xff000000)<<8)|\
							(uint64_t)((x&0xff00000000)>>8)|\
							(uint64_t)((x&0xff0000000000)>>24)|\
							(uint64_t)((x&0xff000000000000)>>40)|\
							(uint64_t)((x&0xff00000000000000)>>56))   //1.31写


#endif
