#include "cmd.h"
#include "string.h"
#include "simple_dma.h"
#include "fat/ff.h"		/* Declarations of FatFs API */
#include "xllfifo_drv.h"
#include "nhc_amba.h"
#include "mem_test.h"
#include "wchar.h"    // 2023.9.20
StructMsg			CurMsg;
StructMsgQuery		MsgQuery;
int Packet_body_length=0;
int CheckCode=0;
int result_a201=0x11;
int result_a204=0x11;
int result_a205=0x11;
int result_a208=0x11;
int result_b201=0x11;
int result_d201=0x11;
int result_f201=0x11;
extern FIL file;
DIR dir;
uint32_t nf=0,nd=0;
extern uint32_t  cmd_len;
extern uint32_t  len;
extern uint32_t  buff,buff_r,buff1;
extern uint32_t  packnum;
extern uint8_t   cancel;
///********************读盘存盘数据参数*******************/
//uint32_t  buff,buff_r;
//uint32_t  packnum;        // lyh 2023.8.28
//uint64_t  slba,slba_r;
//uint32_t  cmd_len,len;
//uint64_t  suba_r;
//uint32_t  recv_cmd_ack;
//uint64_t  write_slba = 0;
//int read_flag=0;
//uint32_t rw_count=0,r_count=0;
//uint32_t  cmd_write_cnt=0;
///***************************************************/

/**********************FIFO有关参数设置*************************/
u32 DestinationBuffer[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
u32 DestinationBuffer_1[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
/************************************************************/
void MsgQueryInit(void)
{
	memset( &MsgQuery, 0, sizeof(MsgQuery) );
	MsgQuery.Start = 0;
	MsgQuery.End = 0;
	CurMsg.HandType = 0x00;
	CurMsg.HandId = 0x00;
}

void GetMessage(StructMsg *pMsg)
{
	u32 i;
	StructMsg	*p;

	if((MsgQuery.Start>=MSG_QUERY)||(MsgQuery.End>=MSG_QUERY)) {
		pMsg->HandType = MSG_WARNING;
		pMsg->HandId = WARNING_MSG_OVERFLOW;
		//xil_printf( "GetMessage High OverFlow\r\n" );
	}
	else
	{
		if(MsgQuery.Start!=MsgQuery.End)
		{
			p = &(MsgQuery.MsgQuery[MsgQuery.End]);
			if(++MsgQuery.End >= MSG_QUERY)
				MsgQuery.End = 0;
		}
		else
		{
			pMsg->HandType = MSG_NULL;
			return;
		}
		pMsg->HandType  = p->HandType;
		pMsg->HandId = p->HandId;
		pMsg->DataLen = p->DataLen;
		pMsg->PackNum= p->PackNum;
		//xil_printf("%s %d   p->HandType:%u  p->HandId:%u  pMsg->DataLen:%u\n", __FUNCTION__, __LINE__,p->HandType,p->HandId,p->DataLen);
		for( i=0; i<pMsg->DataLen; i++ )
			pMsg->MsgData[i]  = p->MsgData[i];
	}
}

void SendMessage(StructMsg *pMsg)
{
		u32 i;
		StructMsg	*p;
//		xil_printf("%s %d   p->HandType:0x%x  p->HandId:0x%x  pMsg->DataLen:%u\r\n", __FUNCTION__, __LINE__,pMsg->HandType,pMsg->HandId,pMsg->DataLen);
		if((MsgQuery.Start>=MSG_QUERY)||(MsgQuery.End>=MSG_QUERY))
			return;
			if((MsgQuery.Start==(MsgQuery.End-1))||
				((MsgQuery.End==0)&&(MsgQuery.Start==(MSG_QUERY-1))))
			{
				p = &(MsgQuery.MsgQuery[MsgQuery.End]);
				pMsg->HandType = MSG_WARNING;
				pMsg->HandId = WARNING_MSG_OVERFLOW;
				//xil_printf( "SendMessage High OverFlow\r\n" );
			}
			else
			{
				p = &(MsgQuery.MsgQuery[MsgQuery.Start]);  // 消息入队
				if(++MsgQuery.Start >= MSG_QUERY)
					MsgQuery.Start = 0;
			}

			p->HandType  = pMsg->HandType;
			p->HandId = pMsg->HandId;
			p->DataLen = pMsg->DataLen;
			p->PackNum= pMsg->PackNum;
			for( i=0; i< pMsg->DataLen; i++ )
			{
				p->MsgData[i]  = pMsg->MsgData[i];
			}
//			xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
}

int cmd_parse(void)
{
		StructMsg TMsg;
		int i=0;
		uint32_t a;
		Xil_DCacheInvalidateRange((UINTPTR)CmdRxBufferPtr, CMD_PACK_LEN);
		if(0x55555555 != CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]))
			return -1;
		i+=4;
		if(SRC_ID != CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]))
			return -1;
		i+=4;
		if(DEST_ID != CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]))
			return -1;
		i+=4;
		TMsg.HandType = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
		i+=4;
		TMsg.HandId = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
		i+=4;
		TMsg.PackNum = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
//		i+=8;	//1.11改
		i+=4;

		while(1)
		{
			if(CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3])==0xAAAAAAAA)  break;
			i++;
			Packet_body_length++;
		}
		Packet_body_length-=4;
		TMsg.DataLen=Packet_body_length;
		Packet_body_length=0;
		i-=4;
		CheckCode=CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
//		if(CheckCode!=0x12345678)
//		{
//			xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
////			return -1;
//		}
		cmd_type_id_parse(&TMsg);
}

void cmd_type_id_parse(StructMsg *pMsg)
{
		int i=0;
		StructMsg TMsg={0};
		TMsg.HandType=pMsg->HandType;
		TMsg.HandId=pMsg->HandId;
		TMsg.PackNum=pMsg->PackNum;
		TMsg.DataLen=pMsg->DataLen;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		switch(pMsg->HandType)
		{
				case 0xa2:
					switch(pMsg->HandId)
					{
						case 0x1:
//							xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//								xil_printf("%x",TMsg.MsgData[i]);
							}
						break;
						case 0x2:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x4:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x5:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x8:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						default:
							return 0;
						break;
					}
				break;
				case 0xb2:
					for(i=0; i < TMsg.DataLen; i++)
					{
						TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
					}
				break;

				case 0xd2:
					switch(pMsg->HandId)
					{
						case 0x1:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
//						case 0x2:
//
//						break;
//						case 0x3:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//						break;
						case 0x4:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
//						case 0x5:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//						break;
//						case 0x6:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//						break;
						case 0x7:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x8:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x9:
							//写取消标志位置1
							cancel=1;
						break;
						case 0xA:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						default:
							return 0;
						break;
					}
				break;

				case 0xf2:
					for(i=0; i < TMsg.DataLen; i++)
					{
						TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
					}
				break;
				default:
					return 0;
				break;
		}
		//xil_printf("%s %d p->HandType:0x%x  p->HandId:0x%x\n", __FUNCTION__, __LINE__,TMsg.HandType,TMsg.HandId);

		SendMessage( &TMsg );
}

void convert(u16 *str1,u8 *str2)	//add by lyh on 1.22  单字节字符的ASCLL码后补零凑成16位,16位GB2312保持不变
{
		int x=0,h=0;
		for (x = 0; x < 256; x++)
		{
				u8 name=str2[x];
				if(name<=0x7E)
				{
					str1[h]=name;
					str1[h]<<=8;
				}
				else
				{
					str1[h]=name;
					str1[h]<<=8;
					name=str2[++x];
					str1[h]|=name;
				}
				h++;
				if(str2[x+1]=='\0')  break;
		}
}

void reverse_u16(u16 *str1,u16 *str2)	//数组中,双字节数据按字节倒置     例如0xB7D6->0xD6B7
{
		int i=0;
		u8  STR;
		for(i=0;;i++)
		{
			STR=str1[i];
			str2[i]=STR;
			str2[i]<<=8;
			STR=(str1[i]>>8);
			str2[i]|=STR;
			if(str1[i+1]=='\0') break;
		}
}

void ConvertReverse(u16 *str)   // 数组里类似0x3000的数,转换成0x0030
{
		int x=0;
		u8 STR=0;
		u8 STR1=0;
		for(x=0;;x++)
		{
			STR=str[x]>>8;
			STR1=str[x];
			if((STR<=0x7E)&&(STR1==0))
			{
				//按字节倒置
				str[x]>>=8;
			}
			if(str[x+1]=='\0')  break;
		}
}

void Reverse_u8(u8 *ARR,int n)
{
		u8 count=0;
		u16 a=0;
		int i=n;
		int temp=n;
		while(1)
		{
			a=(ARR[i++]|ARR[i++]<<8);
			if(a=='\0')  break;
			count++;
		}
		u8 *arr=(u8 *)wjq_malloc_t(sizeof(u8)*count*2);

		for(int j=0;j<count*2;j++)
		{
			arr[j]=ARR[temp+j];
		}

		for(int j=0;j<count;j++)
		{
			ARR[temp+2*j]=arr[2*(count-j-1)];
			ARR[temp+2*j+1]=arr[2*(count-j)-1];
		}
}

void Convert_GB2312_to_UTF16LE(u16 *name)
{
		u8 dat[1024]={0};
		u8 flag=0;
		int k=0;
		int h=0;
		u16 data=0;
		while(1)
		{
			dat[k]=name[k/2]&0xff;
			dat[k+1]=((name[k/2]&0xff00)>>8);
			if( dat[k]==0 && dat[k+1]==0 )   break;
			k+=2;
		}
		k=0;
		while(1)
		{
			if(dat[k]>=0x80 && dat[k+1]>=0x80)
			{
				name[h++]=CW16(dat[k],dat[k+1]);
			}
			else if(dat[k] < 0x80 && dat[k + 1] < 0x80)
			{
				name[h++]=(u16)(dat[k]<<8);
				name[h++]=(u16)(dat[k+1]<<8);
			}
			else
			{
				name[h++]=(u16)(dat[k] << 8);
				flag = 1;
			}
			if(dat[k]==0 && dat[k+1]==0)   break;
			if (flag == 1)
			{
				k+=1;
				flag = 0;
			}
			else
			{
				k+=2;
			}
		}
		k=0;
		h=0;
		ConvertReverse(name);
		while(1)
		{
			data=name[k];
			name[k]=ff_oem2uni(data,FF_CODE_PAGE);
			if(name[k]==0)   break;
			name[k]=SW16(name[k]);
			k++;
		}
		k=0;
		h=0;
}


void InitTimeList(void)
{
	 CreateTimelist= (CreateTimeNode *)wjq_malloc_t(sizeof(CreateTimeNode));    // 分配一个头结点
	 ChangeTimelist= (ChangeTimeNode *)wjq_malloc_t(sizeof(ChangeTimeNode));    // 分配一个头结点
	 AccessTimelist= (AccessTimeNode *)wjq_malloc_t(sizeof(AccessTimeNode));    // 分配一个头结点
	 if (CreateTimelist == NULL|| ChangeTimelist==NULL || AccessTimelist==NULL)					// 内存不足分配失败
	 {
		 return -1;
	 }
	 CreateTimelist->next  = NULL;
	 ChangeTimelist->next  = NULL;
	 AccessTimelist->next  = NULL;
}
//CreateTime节点入队，采用尾插法  12.25 by lyh
void CreateTimeNode_TailInsert(CreateTimeList CreateTimelist,CreateTimeNode node)
{
	 CreateTimeNode *L;
//	 L = (Node *)malloc(sizeof(Node));
	 L = (CreateTimeNode *)wjq_malloc_t(sizeof(CreateTimeNode));
	 L->data= node.data;
	 CreateTimeNode *r=CreateTimelist,*b=CreateTimelist;
	 while(b->next!=NULL)	// 防止相同文件名重复入队
	 {
		 b=b->next;
		 if(!strcmp(b->data.name,node.data.name))
			return 0;
	 }

	 while(r->next!=NULL)
	 {
		 r=r->next;
	 }
	 r->next=L;
	 r=L;
	 r->next=NULL;
}

//删除CreateTime节点
void CreateTimeNode_Delete(CreateTimeList CreateTimelist,BYTE *str)
{
	 CreateTimeNode N;
	 strcpy(N.data.name,str);
	 CreateTimeNode *r=CreateTimelist,*b=CreateTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))	//找到要删除的节点
		 {
			 r->next=b->next;
//			 wjq_free_t(b);
			 break;
		 }
		 r=r->next;
	 }
}

//修改CreateTime节点名称，str1为旧名称，str2为新名称或时间，mode模式：0->修改名称，为1->修改时间
void CreateTimeNode_Modify(CreateTimeList CreateTimelist,BYTE *str1,BYTE *str2,u8 mode)
{
	 CreateTimeNode N;
	 strcpy(N.data.name,str1);
	 CreateTimeNode *r=CreateTimelist,*b=CreateTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))			//	找到要修改的节点
		 {
			 if(!mode)
				 strcpy(b->data.name,str2);
			 else
				 strcpy(b->data.time,str2);
			 break;
		 }
		 r=r->next;
	 }
}

// 根据名称，获取CreateTime链表节点
CreateTimeNode GetCreateTimeNode(BYTE *str)
{
	 CreateTimeNode N;
	 strcpy(N.data.name,str);
	 CreateTimeNode *r=CreateTimelist,*b=CreateTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))			//	找到要修改的节点
		 {
			 N.data=b->data;
			 return N;
		 }
		 r=r->next;
	 }
}

//	ChangeTime节点入队，采用尾插法  12.25 by lyh
void ChangeTimeNode_TailInsert(ChangeTimeList ChangeTimelist,ChangeTimeNode node)
{
	 ChangeTimeNode *L;
//	 L = (Node *)malloc(sizeof(Node));
	 L = (ChangeTimeNode *)wjq_malloc_t(sizeof(ChangeTimeNode));
	 L->data= node.data;
	 ChangeTimeNode *r=ChangeTimelist,*b=ChangeTimelist;

	 while(b->next!=NULL)	// 防止相同文件名重复入队
	 {
		 b=b->next;
		 if(!strcmp(b->data.name,node.data.name))
			return 0;
	 }

	 while(r->next!=NULL)
	 {
		 r=r->next;
	 }
	 r->next=L;
	 r=L;
	 r->next=NULL;
}

//	删除ChangeTime节点
void ChangeTimeNode_Delete(ChangeTimeList ChangeTimelist,BYTE *str)
{
	 ChangeTimeNode N;
	 strcpy(N.data.name,str);
	 ChangeTimeNode *r=ChangeTimelist,*b=ChangeTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))	//找到要删除的节点
		 {
			 r->next=b->next;
	//			 wjq_free_t(b);
			 break;
		 }
		 r=r->next;
	 }
}

//修改ChangeTime节点名称，str1为旧名称，str2为新名称或时间，mode模式：0->修改名称，为1->修改时间
void ChangeTimeNode_Modify(ChangeTimeList ChangeTimelist,BYTE *str1,BYTE *str2,u8 mode)
{
	 ChangeTimeNode N;
	 strcpy(N.data.name,str1);
	 ChangeTimeNode *r=ChangeTimelist,*b=ChangeTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))			//	找到要修改的节点
		 {
			 if(!mode)
				 strcpy(b->data.name,str2);
			 else
				 strcpy(b->data.time,str2);
			 break;
		 }
		 r=r->next;
	 }
}

// 根据名称，获取ChangeTime链表节点
ChangeTimeNode GetChangeTimeNode(BYTE *str)
{
	 ChangeTimeNode N;
	 strcpy(N.data.name,str);
	 ChangeTimeNode *r=ChangeTimelist,*b=ChangeTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))			//	找到要修改的节点
		 {
			 N.data=b->data;
			 return N;
		 }
		 r=r->next;
	 }
}

//	AccessTime节点入队，采用尾插法  12.25 by lyh
void AccessTimeNode_TailInsert(AccessTimeList AccessTimelist,AccessTimeNode node)
{
	 AccessTimeNode *L;
//	 L = (Node *)malloc(sizeof(Node));
	 L = (AccessTimeNode *)wjq_malloc_t(sizeof(AccessTimeNode));
	 L->data= node.data;
	 AccessTimeNode *r=AccessTimelist,*b=AccessTimelist;

	 while(b->next!=NULL)	// 防止相同名称重复入队
	 {
		 b=b->next;
		 if(!strcmp(b->data.name,node.data.name))
			return 0;
	 }

	 while(r->next!=NULL)
	 {
		 r=r->next;
	 }
	 r->next=L;
	 r=L;
	 r->next=NULL;
}

//	AccessTime节点删除
void AccessTimeNode_Delete(AccessTimeList AccessTimelist,BYTE *str)
{
	 AccessTimeNode N;
	 strcpy(N.data.name,str);
	 AccessTimeNode *r=AccessTimelist,*b=AccessTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))	//找到要删除的节点
		 {
			 r->next=b->next;
	//			 wjq_free_t(b);
			 break;
		 }
		 r=r->next;
	 }
}

//修改AccessTime节点名称，str1为旧名称，str2为新名称或时间，mode模式：0->修改名称，为1->修改时间
void AccessTimeNode_Modify(AccessTimeList AccessTimelist,BYTE *str1,BYTE *str2,u8 mode)
{
	 AccessTimeNode N;
	 strcpy(N.data.name,str1);
	 AccessTimeNode *r=AccessTimelist,*b=AccessTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))			//	找到要修改的节点
		 {
			 if(!mode)
				 strcpy(b->data.name,str2);
			 else
				 strcpy(b->data.time,str2);
			 break;
		 }
		 r=r->next;
	 }
}

// 根据名称，获取AccessTime链表节点
AccessTimeNode GetAccessTimeNode(BYTE *str)
{
	 AccessTimeNode N;
	 strcpy(N.data.name,str);
	 AccessTimeNode *r=AccessTimelist,*b=AccessTimelist;

	 while(r->next!=NULL)
	 {
		 b=r->next;
		 if(!strcmp(b->data.name,N.data.name))			//	找到要修改的节点
		 {
			 N.data=b->data;
			 return N;
		 }
		 r=r->next;
	 }
}

// 递归修改文件夹下，所有文件和文件夹的各时间链表，str1原文件夹名称，str2为新文件夹名称
FRESULT Dir_TimeNodeModify(BYTE *str1,BYTE *str2)
{
		FRESULT res;
		DIR dir;
		UINT i=0,j=0;
		static FILINFO fno;
		BYTE STR1[100]={0};
		BYTE STR2[100]={0};

		res = f_opendir(&dir, str2);                       /* Open the directory */
		if (res == FR_OK) {
			for (;;) {
				res = f_readdir(&dir, &fno);                   /* Read a directory item */
				if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
				if (fno.fattrib & AM_DIR) {                    /* It is a directory */
					CreateTimeNode_Modify(CreateTimelist,str1,str2,0);
					ChangeTimeNode_Modify(ChangeTimelist,str1,str2,0);
					AccessTimeNode_Modify(AccessTimelist,str1,str2,0);
					i = strlen(str1);
					j = strlen(str2);

					sprintf(&str2[j], "/%s", fno.fname);
					sprintf(&str1[i], "/%s", fno.fname);
					res = Dir_TimeNodeModify(str1,str2);                    /* Enter the directory */
					CreateTimeNode_Modify(CreateTimelist,str1,str2,0);
					ChangeTimeNode_Modify(ChangeTimelist,str1,str2,0);
					AccessTimeNode_Modify(AccessTimelist,str1,str2,0);
					if (res != FR_OK) break;
					printf("directory name is:%s\n", str2);
					str1[i] = 0;
					str2[j] = 0;
				}
				else {                                       /* It is a file. */
//					printf("file name is:%s/%s\n", str2, fno.fname);
					sprintf(STR1,"%s/%s",str1,fno.fname);
					sprintf(STR2,"%s/%s",str2,fno.fname);
					CreateTimeNode_Modify(CreateTimelist,STR1,STR2,0);
					ChangeTimeNode_Modify(ChangeTimelist,STR1,STR2,0);
					AccessTimeNode_Modify(AccessTimelist,STR1,STR2,0);
				}
			}
			f_closedir(&dir);
		}
		return res;
}

//递归删除文件夹,同时修改时间链表
FRESULT delete_dir (BYTE* path)
{
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;
    BYTE FilePath[100]={0};
    int flag=0;
    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK)
    {
        for (;;)
        {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) /* Break on error or end of dir */
			{
                if(res==FR_OK && flag==0) 			// empty directory
                {
                	flag=1;
                	res = f_unlink (path); 			// delete the empty directory
					if (res != FR_OK) {
						xil_printf("Delete directory  Failed! ret=%d\r\n", res);
						return -1;
					}
					printf("succeed to delete directory: %s ! \r\n",path);
					CreateTimeNode_Delete(CreateTimelist,path);
					ChangeTimeNode_Delete(ChangeTimelist,path);
					AccessTimeNode_Delete(AccessTimelist,path);
                }
            	break;
			}
            if (fno.fattrib & AM_DIR)       /* It is a directory */
			{
            		CreateTimeNode_Delete(CreateTimelist,path);
					ChangeTimeNode_Delete(ChangeTimelist,path);
					AccessTimeNode_Delete(AccessTimelist,path);
					i = strlen(path);
					sprintf(&path[i], "/%s", fno.fname);
					res = delete_dir(path);                    /* Enter the directory */
            		CreateTimeNode_Delete(CreateTimelist,path);
					ChangeTimeNode_Delete(ChangeTimelist,path);
					AccessTimeNode_Delete(AccessTimelist,path);
					if (res != FR_OK) break;
						path[i] = 0;
            }
            else 							/* It is a file. */
            {
//					printf("%s/%s \r\n", path, fno.fname);
					sprintf(FilePath,"%s/%s",path, fno.fname);
					if(f_unlink(FilePath) == FR_OK)
					{
						printf("succeed to delete file: %s ! \r\n",FilePath);
					}
					CreateTimeNode_Delete(CreateTimelist,FilePath);
					ChangeTimeNode_Delete(ChangeTimelist,FilePath);
					AccessTimeNode_Delete(AccessTimelist,FilePath);
            }
        }
        f_closedir(&dir);
    }
    else
    {
    	xil_printf("Failed to open! res=%d\r\n", res);
    }
    return res;
}

/*递归插入文件夹下，所有文件和文件夹的各时间链表，str1原文件夹名称，str2为新文件夹名称*/
FRESULT Dir_TimeNodeInsert(BYTE *str1,BYTE *str2)
{
		FRESULT res;
		DIR dir;
		UINT i=0,j=0;
		static FILINFO fno;
		CreateTimeNode CreateTimenode;
		ChangeTimeNode ChangeTimenode;
		AccessTimeNode AccessTimenode;
		BYTE STR1[100]={0};
		BYTE STR2[100]={0};

		res = f_opendir(&dir, str2);                       /* Open the directory */
		if (res == FR_OK) {
			for (;;) {
				res = f_readdir(&dir, &fno);                   /* Read a directory item */
				if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
				if (fno.fattrib & AM_DIR) {                    /* It is a directory */
					CreateTimenode=GetCreateTimeNode(str1);
					strcpy(CreateTimenode.data.name,str2);
					ChangeTimenode=GetChangeTimeNode(str1);
					strcpy(ChangeTimenode.data.name,str2);
					AccessTimenode=GetAccessTimeNode(str1);
					strcpy(AccessTimenode.data.name,str2);
					CreateTimeNode_TailInsert(CreateTimelist,CreateTimenode);//插入创建时间链表
					ChangeTimeNode_TailInsert(ChangeTimelist,ChangeTimenode);//插入修改时间链表
					AccessTimeNode_TailInsert(AccessTimelist,AccessTimenode);//插入访问时间链表
					i = strlen(str1);
					j = strlen(str2);

					sprintf(&str1[i], "/%s", fno.fname);
					sprintf(&str2[j], "/%s", fno.fname);

					res = Dir_TimeNodeInsert(str1,str2);                    /* Enter the directory */
					if (res != FR_OK) break;
					CreateTimenode=GetCreateTimeNode(str1);
					strcpy(CreateTimenode.data.name,str2);
					ChangeTimenode=GetChangeTimeNode(str1);
					strcpy(ChangeTimenode.data.name,str2);
					AccessTimenode=GetAccessTimeNode(str1);
					strcpy(AccessTimenode.data.name,str2);
					CreateTimeNode_TailInsert(CreateTimelist,CreateTimenode);//插入创建时间链表
					ChangeTimeNode_TailInsert(ChangeTimelist,ChangeTimenode);//插入修改时间链表
					AccessTimeNode_TailInsert(AccessTimelist,AccessTimenode);//插入访问时间链表
//					printf("directory name is:%s\n", str2);
					str1[i] = 0;
					str2[j] = 0;
				}
				else {                                       /* It is a file. */
//					printf("file name is:%s/%s\n", str2, fno.fname);
					sprintf(STR1,"%s/%s",str1,fno.fname);
					sprintf(STR2,"%s/%s",str2,fno.fname);
					CreateTimenode=GetCreateTimeNode(STR1);
					strcpy(CreateTimenode.data.name,STR2);
					ChangeTimenode=GetChangeTimeNode(STR1);
					strcpy(ChangeTimenode.data.name,STR2);
					AccessTimenode=GetAccessTimeNode(STR1);
					strcpy(AccessTimenode.data.name,STR2);
					CreateTimeNode_TailInsert(CreateTimelist,CreateTimenode);//插入创建时间链表
					ChangeTimeNode_TailInsert(ChangeTimelist,ChangeTimenode);//插入修改时间链表
					AccessTimeNode_TailInsert(AccessTimelist,AccessTimenode);//插入访问时间链表
				}
			}
			f_closedir(&dir);
		}
		return res;
}

int run_cmd_a201(StructMsg *pMsg)
{
	int ret=0,i=0,x=0,temp=0,h=0;
	u16 unicode_u16=0;
	u32 file_cmd=0;
//	u8 count=0;
//	u16 a=0;
	WCHAR cmd_str_1[1024]={0},cmd_str_2[1024]={0};
	BYTE  cmd_str_11[100]={0},cmd_str_21[100]={0};
	FRESULT fr1;
	FILINFO fno1;
	CreateTimeNode CreateTimenode;
	ChangeTimeNode ChangeTimenode;
	AccessTimeNode AccessTimenode;//003A0030 8FBE96F7
	xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	i=i+4;
	temp=i;   // 9.7 LYH
	xil_printf("%s %d  file_cmd:0x%x\r\n", __FUNCTION__, __LINE__,file_cmd);

//	while(1)
//	{
//		a=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//		if(a=='\0')  break;
//		count++;
//	}
//	u8 *arr=(u8 *)wjq_malloc_t(sizeof(u8)*count*2);
//
//	for(int j=0;j<count*2;j++)
//	{
//		arr[j]=pMsg->MsgData[temp+j];
//	}
//
//	for(int j=0;j<count;j++)
//	{
//		pMsg->MsgData[temp+2*j]=arr[2*(count-j-1)];
//		pMsg->MsgData[temp+2*j+1]=arr[2*(count-j)-1];
//	}
	i+=2;   // 1.31改   客户在文件名后加了两个字节的标志位
	for (x = 0; x < 1024; x++)
	{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if ((cmd_str_1[x] == '\0'))  break;
	}
	xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);

    switch(file_cmd)
    {
		case NEW_FILE:       //新建文件
			ret = f_open (&file, cmd_str_11, FA_CREATE_ALWAYS | FA_WRITE);
//			ret = f_open (&file, "abc", FA_CREATE_ALWAYS | FA_WRITE);
			if (ret != FR_OK) {
				xil_printf("f_open Failed! ret=%d\r\n", ret);
			  //cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
				return -1;
			}
			xil_printf("NEW_FILE Command Success! file name:%s\r\n",cmd_str_11);
			f_close(&file);
			if (ret != FR_OK) {
				xil_printf("close file Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			CreateTimenode.data.type=0;
			strcpy(CreateTimenode.data.name,cmd_str_11);
			strcpy(CreateTimenode.data.time,"default_file");

			ChangeTimenode.data.type=0;
			strcpy(ChangeTimenode.data.name,cmd_str_11);
			strcpy(ChangeTimenode.data.time,"default_file");

			AccessTimenode.data.type=0;
			strcpy(AccessTimenode.data.name,cmd_str_11);
			strcpy(AccessTimenode.data.time,"default_file");

			CreateTimeNode_TailInsert(CreateTimelist,CreateTimenode);//插入创建时间链表
			ChangeTimeNode_TailInsert(ChangeTimelist,ChangeTimenode);//插入修改时间链表
			AccessTimeNode_TailInsert(AccessTimelist,AccessTimenode);//插入访问时间链表
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;

/***************************************************************************************/
/***************************************************************************************/
		case NEW_FOLDER:    //新建文件夹
			ret =f_mkdir(cmd_str_11);
//			ret =f_mkdir("bcd");
			if (ret != FR_OK) {
				xil_printf("f_mkdir  Failed! ret=%d\r", ret);
				if(ret==8) xil_printf("Failed reason:FR_EXIST\n");
				return -1;
			}
			xil_printf("NEW_FOLDER Command Success! folder name:%s\r\n",cmd_str_11);
//			CreateTimenode.data.type=1;
//			strcpy(CreateTimenode.data.name,cmd_str_11);
//			strcpy(CreateTimenode.data.time,"default_dir");
//
//			ChangeTimenode.data.type=1;
//			strcpy(ChangeTimenode.data.name,cmd_str_11);
//			strcpy(ChangeTimenode.data.time,"default_dir");
//
//			AccessTimenode.data.type=1;
//			strcpy(AccessTimenode.data.name,cmd_str_11);
//			strcpy(AccessTimenode.data.time,"default_dir");
//
//			CreateTimeNode_TailInsert(CreateTimelist,CreateTimenode);//插入创建时间链表
//			ChangeTimeNode_TailInsert(ChangeTimelist,ChangeTimenode);//插入修改时间链表
//			AccessTimeNode_TailInsert(AccessTimelist,AccessTimenode);//插入访问时间链表
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;

/***************************************************************************************/
/***************************************************************************************/
		case DEL_FILE:       //删除文件时，除了判断文件是否存在外，还需判断文件是否是只读属性
			fr1 = f_stat(cmd_str_11, &fno1);    //lyh 9.4改
		    switch(fr1)
		    {
			    case FR_OK:
                     if(fno1.fattrib & AM_RDO){
                    	 xil_printf("FR_DENIED, the file's attribute is read-only\r\n");
                         return 0;
		        }
			    ret = f_unlink (cmd_str_11); // LYH 2023.9.4
				if (ret != FR_OK) {
					xil_printf("Delete file  Failed! ret=%d\r\n", ret);
					return -1;
				}
				xil_printf("DEL_FILE Command Success! file name:%s\r\n",cmd_str_11);
				/*在创建时间链表、修改时间链表、访问时间链表删除该名称*/
				CreateTimeNode_Delete(CreateTimelist,cmd_str_11);
				ChangeTimeNode_Delete(ChangeTimelist,cmd_str_11);
				AccessTimeNode_Delete(AccessTimelist,cmd_str_11);
//				cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
				cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			    break;
			    case FR_NO_FILE:
			    	xil_printf("%s is not exist.\r\n", cmd_str_11);// LYH 2023.9.4
			        return -1;
			    break;
			    default:
			    	xil_printf("An error occured. (%d)\r\n", fr1);
				break;
			}
		break;
/***************************************************************************************/
/***************************************************************************************/

		case DEL_FOLDER:  // 删除文件夹
			ret = delete_dir(cmd_str_11);//  Writed on 2023.8.15 by LYH
			if (ret != FR_OK) {
				xil_printf("delete directory Failed! ret=%d\r\n", ret);
			   	return -1;
			}

			xil_printf("DEL_FOLDER Command Success! folder name:%s\r\n",cmd_str_11);
			/*同时也递归删除名称目录下所有文件在时间链表里的记录*/

//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case RENAME_FILE:    //重命名文件
//			i=temp+(strlen(cmd_str_11)+1)*2;        // 1.5 改 by lyh
			i=temp+2048;
			h=0;
		    for (x = 0; x < 1024; x++) {
		    	unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
		    	cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
		    	if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
		        if (cmd_str_2[x] == '\0') break;
		    }
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);

			ret = f_rename (cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("rename file Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			xil_printf("Success to rename file:%s to file:%s! \r\n",cmd_str_11,cmd_str_21);
//			/*修改原来名称在创建时间、修改时间、访问时间链表里的信息， 并且以新名称继承原来名称在时间链表里的信息*/
			CreateTimeNode_Modify(CreateTimelist,cmd_str_11,cmd_str_21,0);
			ChangeTimeNode_Modify(ChangeTimelist,cmd_str_11,cmd_str_21,0);
			AccessTimeNode_Modify(AccessTimelist,cmd_str_11,cmd_str_21,0);
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case RENAME_FOLDER:   //重命名文件夹
//			i=temp+(strlen(cmd_str_11)+1)*2;		// 1.5 改 by lyh
			i=temp+2048;
			h=0;
		    for (x = 0; x < 1024; x++) {
		    	unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
		    	cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
		    	if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
		        if (cmd_str_2[x] == '\0') break;
		    }
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = f_rename (cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("rename directory Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			xil_printf("Success to copy directory:%s to directory:%s ! \r\n",cmd_str_11,cmd_str_21);

			/*同时也需要递归修改原来文件夹下所有文件和文件夹的在时间链表里的信息*/
			ret=Dir_TimeNodeModify(cmd_str_11,cmd_str_21);
			if (ret != FR_OK)
			{
				xil_printf("Modify TimeNode Failed! ret=%d\r\n", ret);
				return -1;
			}

//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case MOVE_FOLDER:   	//移动文件夹
//			i=temp+(strlen(cmd_str_11)+1)*2;		// 1.5 改 by lyh
			i=temp+2048;
			h=0;
		    for (x = 0; x < 1024; x++) {
		    	unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
		    	cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
		    	if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
		        if (cmd_str_2[x] == '\0') break;
		    }

			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = f_rename(cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("move directory Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			xil_printf("Success to move directory:%s to directory:%s ! \r\n",cmd_str_11,cmd_str_21);

			/*同时也需要递归修改原来文件夹下所有文件和文件夹的在时间链表里的信息*/
			ret=Dir_TimeNodeModify(cmd_str_11,cmd_str_21);
			if (ret != FR_OK)
			{
				xil_printf("Modify TimeNode Failed! ret=%d\r\n", ret);
				return -1;
			}
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case MOVE_FILE:      //移动文件
//			i=temp+(strlen(cmd_str_11)+1)*2;		// 1.5 改 by lyh
			i=temp+2048;
			h=0;
		    for (x = 0; x < 1024; x++) {
		    	unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
		    	cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
		    	if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
		        if (cmd_str_2[x] == '\0') break;
		    }

			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = f_rename(cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("move file  Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			xil_printf("Success to move file:%s to file:%s ! \r\n",cmd_str_11, cmd_str_21);
			/*修改原来名称在创建时间、修改时间、访问时间链表里的信息， 并且以新名称继承原来名称在时间链表里的信息*/
			CreateTimeNode_Modify(CreateTimelist,cmd_str_11,cmd_str_21,0);
			ChangeTimeNode_Modify(ChangeTimelist,cmd_str_11,cmd_str_21,0);
			AccessTimeNode_Modify(AccessTimelist,cmd_str_11,cmd_str_21,0);
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case OPEN_FILE:    //打开文件
			ret = f_open (&file, cmd_str_11, FA_OPEN_EXISTING| FA_WRITE|FA_READ);
			if (ret != FR_OK) {
				xil_printf("f_open  Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			xil_printf("OPEN_FILE Command Success! file name:%s\r\n",cmd_str_11);
			/*更新该名称在访问时间链表里的信息*/
			AccessTimeNode_Modify(AccessTimelist,cmd_str_11,"new_time",1);
////			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case CLOSE_FILE:    //关闭文件
			ret = f_close(&file);
			if (ret != FR_OK) {
				xil_printf("f_close  Failed! ret=%d\r\n", ret);
			   	return -1;
			}
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case COPY_FILE:   //拷贝文件 最后的参数是0表示不覆盖存在的文件，1表示覆盖存在的文件
//			i=temp+(strlen(cmd_str_11)+1)*2;		// 1.5 改 by lyh
			i=temp+2048;
			h=0;
			for (x = 0; x < 1024; x++) {
				  unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			      cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			      if(cmd_str_2[x]<=0x7E)
				  {
						cmd_str_21[h++]=cmd_str_2[x];
				  }
				  else
				  {
						cmd_str_21[h++]=(cmd_str_2[x]>>8);
						cmd_str_21[h++]=cmd_str_2[x];
				  }
			      if (cmd_str_2[x] == '\0') break;
		    }

			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret =my_fcopy (cmd_str_11,cmd_str_21,0);
			if (ret != FR_OK) {
				xil_printf("Copy File Failed! ret=%d\r\n", ret);
				return -1;
			}

			CreateTimenode=GetCreateTimeNode(cmd_str_11);
			strcpy(CreateTimenode.data.name,cmd_str_21);
			ChangeTimenode=GetChangeTimeNode(cmd_str_11);
			strcpy(ChangeTimenode.data.name,cmd_str_21);
			AccessTimenode=GetAccessTimeNode(cmd_str_11);
			strcpy(AccessTimenode.data.name,cmd_str_21);
			CreateTimeNode_TailInsert(CreateTimelist,CreateTimenode);//插入创建时间链表
			ChangeTimeNode_TailInsert(ChangeTimelist,ChangeTimenode);//插入修改时间链表
			AccessTimeNode_TailInsert(AccessTimelist,AccessTimenode);//插入访问时间链表
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case COPY_FOLDER:  //拷贝文件夹 最后的参数是0表示不覆盖存在的文件，1表示覆盖存在的文件
//			i=temp+(strlen(cmd_str_11)+1)*2;		// 1.5 改 by lyh
			i=temp+2048;
			h=0;
			temp=strlen(cmd_str_11);
			for (x = 0; x < 1024; x++) {
				  unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				  cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				  if(cmd_str_2[x]<=0x7E)
				  {
						cmd_str_21[h++]=cmd_str_2[x];
				  }
				  else
				  {
						cmd_str_21[h++]=(cmd_str_2[x]>>8);
						cmd_str_21[h++]=cmd_str_2[x];
				  }
		          if (cmd_str_2[x] == '\0') break;
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = my_dcopy (cmd_str_11,cmd_str_21,0);
			if (ret != FR_OK) {
				xil_printf("Copy Directory Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			cmd_str_11[temp]='\0';
			ret = Dir_TimeNodeInsert(cmd_str_11,cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("Insert Time Node Failed! ret=%d\r\n", ret);
				return -1;
			}
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case GET_DIR:   // 返回目录中的文件和子目录列表
			ret = cmd_reply_a208(cmd_str_11);
//			ret = cmd_reply_a208("0:");
			if (ret != FR_OK)
			{
				xil_printf("Returns Directory List Failed! ret=%d\r\n", ret);
				return -1;
			}
//			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
			cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
#if 0  	// 1.31注释掉，因为客户想用该命令执行cmd_reply_a208这个指令
			ret = scan_files(cmd_str_11);
			if (ret != FR_OK)
			{
				xil_printf("Get Directory List Failed! ret=%d\r\n", ret);
				return -1;
			}
			nf=0,nd=0;
			ret =Num_of_Dir_and_File(cmd_str_11,&nf,&nd,0);
			if (ret != FR_OK) {
				xil_printf("Get Directory List Failed! ret=%d\r\n", ret);
			   	return -1;
			}
			xil_printf("file num=%d, directory num=%d\r\n",nf,nd);
			//更新扫描的目录的访问时间
			AccessTimeNode_Modify(AccessTimelist,cmd_str_11,"new_time",1);
			cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
#endif
		break;

		default:
			break;
    }
    return 0;
}

int run_cmd_a202(StructMsg *pMsg)
{
        //解析要问讯哪种（包括A2-01、A2-04、A2-05、A2-08、D2-01、D2-01）
	    int ret=0,i=0;
        u32 ack_HandType,ack_HandID,ack_PackNum;
        ack_HandType=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
        i+=4;
        ack_HandID=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
        i+=4;
        ack_PackNum=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);

        switch(ack_HandType)
        {
           case 0xA2:
                 switch(ack_HandID)
                {
                     case  0x01:
                    	 cmd_reply_a203(ack_PackNum,ack_HandType,ack_HandID,result_a201);
//                    	 cmd_reply_a203_to_a201(ack_PackNum, ack_HandType, ack_HandID, result_a201);
                    	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a201);
                     break;

                     case  0x04:
                    	 cmd_reply_a203(ack_PackNum,ack_HandType,ack_HandID,result_a204);
//                    	 cmd_reply_a203_to_a204(ack_PackNum, ack_HandType, ack_HandID, result_a204);
                    	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a204);
                     break;

                     case  0x05:
                    	 cmd_reply_a203(ack_PackNum,ack_HandType,ack_HandID,result_a205);
//                       cmd_reply_a203_to_a205(ack_PackNum, ack_HandType, ack_HandID, result_a205);
                      	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a205);
                     break;

//                     case  0x08:
//                    	 cmd_reply_a203_to_a208(ack_PackNum, ack_HandType, ack_HandID, result_a208);
//                    	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a208);
//                     break;

                     default:
                         return 0;
                     break;
                }
           break;
           case 0xD2:
        	   switch(ack_HandID)
			  {
				   case  0x01:
//					    cmd_reply_a203_to_d201(ack_PackNum, ack_HandType, ack_HandID, result_d201);
					    cmd_reply_a203(ack_PackNum, ack_HandType, ack_HandID, result_d201);
					    xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_d201);

				   break;
				   default:
						return 0;
				   break;
			  }
           break;
           case 0xB2:
        	   switch(ack_HandID)
			  {
				   case  0x01:
					    cmd_reply_a203_to_b201(ack_PackNum, ack_HandType, ack_HandID, result_b201);
					    xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_b201);
				   break;
				   default:
						return 0;
				   break;
			  }
           break;

           default:
        	   return 0;
           break;
        }
      return 0;
}

int run_cmd_a204(StructMsg *pMsg)   //增加了协议内容
{
	    int ret=0,i=0,x=0,temp=0,h=0;
		u16 unicode_u16=0;
		int flag=0;
		u32 file_cmd=0,file_cmd2=0;
		WCHAR cmd_str_1[1024]={0},cmd_str_2[512]={0};
		BYTE cmd_str_11[100]={0},cmd_str_21[100]={0};

		WCHAR cmd_str_3[1024]={0},cmd_str_4[512]={0};
		BYTE cmd_str_31[100]={0},cmd_str_41[100]={0};
		int x0=0,x1=0,x2=0,x3=0,x4=0,x5=0,x6=0,x7=0;
		DIR dir;
		FIL file;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		//属性修改使能指令file_cmd
		file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i=i+4;
		temp=i;
		xil_printf("%s %d  file_cmd:0x%x\r\n", __FUNCTION__, __LINE__,file_cmd);

		// 解析操作指令
		x0=((file_cmd>>0)&0x1==1)?1:0;   //x0=1修改文件属性,x0=0不修改文件属性
		x1=((file_cmd>>4)&0x1==1)?1:0;   //x1=1修改文件读写控制,x1=0不修改文件读写控制
		x2=((file_cmd>>8)&0x1==1)?1:0;   //x2=1修改文件显示控制,x2=0不修改文件显示控制
		x3=((file_cmd>>12)&0x1==1)?1:0;  //x3=1对文件进行操作,x3=0不对文件进行操作

		x4=((file_cmd>>16)&0x1==1)?1:0;  //x4=1修改文件夹属性,x4=0不修改文件夹属性
		x5=((file_cmd>>20)&0x1==1)?1:0;  //x5=1修改文件夹读写控制,x5=0不修改文件夹读写控制
		x6=((file_cmd>>24)&0x1==1)?1:0;  //x6=1修改文件夹显示控制,x6=0不修改文件夹显示控制
		x7=((file_cmd>>28)&0x1==1)?1:0;  //x7=1对文件夹进行操作,x7=0不对文件夹进行操作

/////////////////////////////////////////////////////////////////////////
		if(x3==1)
		{
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_1[x]<=0x7E)
				{
					cmd_str_11[h++]=cmd_str_1[x];
				}
				else
				{
					cmd_str_11[h++]=(cmd_str_1[x]>>8);
					cmd_str_11[h++]=cmd_str_1[x];
				}
				if (cmd_str_1[x] == '\0')	break;
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
			// *****************   判断是否更改文件属性        *******************//
			if(x0==1)
			{
				flag=1;
//				i=temp+strlen(cmd_char_str1)*2+2;
//				i=temp+(strlen(cmd_str_11)+1)*2;	//1.15 改 by lyh
				i=temp+2048;
				temp=i;
				h=0;
				for (x = 0; x < 512; x++)
				{
				   unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				   cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				   if(cmd_str_2[x]<=0x7E)
			       {
						cmd_str_21[h++]=cmd_str_2[x];
				   }
				   else
				   {
						cmd_str_21[h++]=(cmd_str_2[x]>>8);
						cmd_str_21[h++]=cmd_str_2[x];
				   }
				   if (cmd_str_2[x] == '\0')	break;
				}

				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21); // 新文件名称

				ret=f_rename(cmd_str_11,cmd_str_21);
				if (ret != FR_OK) {
					xil_printf("Rename File Failed! ret=%d\r\n", ret);
	//				return -1;
				}
			}
			// *****************                   *******************//
			if(flag==0)
//				i=temp+(strlen(cmd_str_11)+1)*2;
				i=temp+2048+1024;
			else if(flag==1)
//				i=temp+(strlen(cmd_str_21)+1)*2;
				i=temp+1024;
			flag=0;
			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]); //文件新读写控制值
			if(x1==1)
			{
				if(file_cmd2==1)
				{
						if(x0==1)    //  用新文件名
						{
							ret=f_chmod(cmd_str_21,AM_RDO,AM_RDO );        //  文件设成只读
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
			//					return -1;
							}
						}
						else         //  用旧文件名
						{
							ret=f_chmod(cmd_str_11,AM_RDO,AM_RDO );         //  文件设成只读
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
	//							return -1;
							}
						}
				}
				else if(file_cmd2==0)
				{
						if(x0==1)    //  用新文件名
						{
							ret=f_chmod(cmd_str_21,0, AM_RDO); 		 //  文件取消只读
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
			//					return -1;
							}
						}
						else         //  用旧文件名
						{
							ret=f_chmod(cmd_str_11,0,AM_RDO);         //  文件取消只读
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
								return -1;
							}
						}
				}
			}
			i+=4;
			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件新显示控制值
			if(x2==1)
			{
				if(file_cmd2==1)
				{
						if(x0==1)    //  用新文件名
						{
							ret=f_chmod(cmd_str_21,AM_HID,AM_HID );
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
			//					return -1;
							}
						}
						else         //  用旧文件名
						{
							ret=f_chmod(cmd_str_11,AM_HID,AM_HID);         //  文件设成隐藏
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
								return -1;
							}
						}
				}
				else if(file_cmd2==0)
				{
						if(x0==1)    //  用新文件名
						{
							ret=f_chmod(cmd_str_21,0,AM_HID);
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
			//					return -1;
							}
						}
						else         //  用旧文件名
						{
							ret=f_chmod(cmd_str_11,0,AM_HID);         //  文件取消隐藏
							if (ret != FR_OK) {
								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
								return -1;
							}
						}
				}
			}
		i+=4;
		temp=i;
		}//if(x3==1)

        /*********************************************/
		if(x7==1)
		{
			if(x3==0)  i=temp+3080;			// 2048+1024+4+4
			h=0;
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_3[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_3[x]<=0x7E)
			    {
					cmd_str_31[h++]=cmd_str_3[x];
			    }
			    else
			    {
					cmd_str_31[h++]=(cmd_str_3[x]>>8);
					cmd_str_31[h++]=cmd_str_3[x];
			    }
				if ((cmd_str_3[x] == '\0')||(cmd_str_3[x] == ' '))
				{
					if(cmd_str_3[x] == ' ')
					{
						cmd_str_31[x]='\0';
					}
					break;
				}
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_31);//  原文件夹名称

			// *****************   判断是否更改文件夹属性        *******************//
			if(x4==1)
			{
				flag=1;
				i=temp+(strlen(cmd_str_31)+1)*2;
				temp=i;
				for (x = 0; x < 512; x++)
				{
					unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
					cmd_str_4[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
					if(cmd_str_4[x]<=0x7E)
					{
						cmd_str_41[h++]=cmd_str_4[x];
					}
					else
					{
						cmd_str_41[h++]=(cmd_str_4[x]>>8);
						cmd_str_41[h++]=cmd_str_4[x];
					}
					if ((cmd_str_4[x] == '\0')||(cmd_str_4[x] == ' '))
					{
						if(cmd_str_4[x] == ' ')
						{
							cmd_str_41[x]='\0';
						}
						break;
					}
				}
				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_41); //  新文件夹名称
				ret=f_rename(cmd_str_31,cmd_str_41);
				if (ret != FR_OK) {
					xil_printf("Rename File Failed! ret=%d\r\n", ret);
	//				return -1;
				}
			}
			// *****************                    *******************//
			if(flag==0)
//				i=temp+strlen(cmd_str_31)*2;
				i=temp+2048+1024;
			else if(flag==1)
//				i=temp+strlen(cmd_str_41)*2;
				i=temp+1024;
			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新读写控制值
			if(x5==1)
			{
				if(file_cmd2==1)
				{
						if(x4==1)           //  用新文件夹名
						{
							ret=f_chmod(cmd_str_41,AM_RDO,AM_RDO );
							if (ret != FR_OK) {
								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
			//					  return -1;
							}
						}
						else                //  用旧文件夹名
						{
							ret=f_chmod(cmd_str_31,AM_RDO,AM_RDO );	    //  文件夹设成只读
							if (ret != FR_OK) {
								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
								  return -1;
							}
						}
				}
				else if(file_cmd2==0)
				{
						if(x4==1)           //  用新文件夹名
						{
							ret=f_chmod(cmd_str_41,0, AM_RDO);
							if (ret != FR_OK) {
								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
			//					  return -1;
							}
						}
						else                //  用旧文件夹名
						{
							ret=f_chmod(cmd_str_31,0, AM_RDO);	    //  文件夹取消只读
							if (ret != FR_OK) {
								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
								  return -1;
							}
						}
				}
			}
			i+=4;
			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新显示控制值
			if(x6==1)
			{
				if(file_cmd2==1)
				{
							if(x4==1)           //  用新文件夹名
							{
								  ret=f_chmod(cmd_str_41,AM_HID,AM_HID );
								  if (ret != FR_OK) {
									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
				//					  return -1;
								  }
							}
							else                //  用旧文件夹名
							{
								  ret=f_chmod(cmd_str_31,AM_HID,AM_HID );     //  文件夹设成隐藏
								  if (ret != FR_OK) {
									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
									  return -1;
								  }
							}
				}
				else if(file_cmd2==0)
				{
							if(x4==1)           //  用新文件夹名
							{
								  ret=f_chmod(cmd_str_41,0, AM_HID);
								  if (ret != FR_OK) {
									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
				//					  return -1;
								  }
							}
							else                //  用旧文件夹名
							{
								  ret=f_chmod(cmd_str_31,0, AM_HID);     //  文件夹取消隐藏
								  if (ret != FR_OK) {
									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
									  return -1;
								  }
							}
				}

			}
		}//if(x7==1)
		cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
//        cmd_reply_a203_to_a204(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
}

//int run_cmd_a204(StructMsg *pMsg)   //按甲方协议写的
//{
//	    int ret=0,i=0,x=0,temp=0;
//		u16 unicode_u16=0;
//		int flag=0;
//		u32 file_cmd=0,file_cmd2=0;
//		WCHAR cmd_str_1[1024]={0},cmd_str_2[512]={0};
//		WCHAR cmd_str_3[1024]={0},cmd_str_4[512]={0};
//		TCHAR *cmd_char_str1,*cmd_char_str2;  // 放单字节数据，由双字节数据转化
//		TCHAR *cmd_char_str3,*cmd_char_str4;
//		int x0=0,x1=0,x2=0,x3=0,x4=0,x5=0,x6=0,x7=0;
//		DIR dir;
//		FIL file;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
//		//属性修改使能指令file_cmd
//		file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
//		i=i+4;
//		temp=i;
//		xil_printf("%s %d  file_cmd:0x%x\r\n", __FUNCTION__, __LINE__,file_cmd);
//
//		// 解析操作指令
//		x0=((file_cmd>>0)&0x1==1)?1:0;   //x0=1修改文件属性,x0=0不修改文件属性
//		x1=((file_cmd>>4)&0x1==1)?1:0;   //x1=1修改文件读写控制,x1=0不修改文件读写控制
//		x2=((file_cmd>>8)&0x1==1)?1:0;   //x2=1修改文件显示控制,x2=0不修改文件显示控制
//		x4=((file_cmd>>16)&0x1==1)?1:0;  //x4=1修改文件夹属性,x4=0不修改文件夹属性
//		x5=((file_cmd>>20)&0x1==1)?1:0;  //x5=1修改文件夹读写控制,x5=0不修改文件夹读写控制
//		x6=((file_cmd>>24)&0x1==1)?1:0;  //x6=1修改文件夹显示控制,x6=0不修改文件夹显示控制
//
///////////////////////////////////////////////////////////////////////////
//	    for (x = 0; x < 1024; x++)
//	    {
//	    	unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//	    	cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//	        if ((cmd_str_1[x] == '\0')||(cmd_str_1[x] == ' ')) break;
//	    }
//	    cmd_char_str1=(TCHAR *)malloc(sizeof(TCHAR)*(x)); // 旧文件名称
//
//		for(int j=0;j<x;j++)
//		{
//			sprintf(cmd_char_str1+j,"%s",cmd_str_1+j);
//		}
//		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_char_str1);
////      i=temp+2048;
////		i=temp+10;
//
//        // *****************   判断是否更改文件属性        *******************//
//        if(x0==1)
//        {
//        	flag=1;
//        	i=temp+strlen(cmd_char_str1)*2+2;
//        	temp=i;
//        	for (x = 0; x < 512; x++)
//        	{
//        	   unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//        	   cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
////        	   if (cmd_str_2[x] == '\0'||i==(temp+10)) break;
//        	   if ((cmd_str_2[x] == '\0')||(cmd_str_2[x] == ' ')) break;
//        	}
//
//        	cmd_char_str2=(TCHAR *)malloc(sizeof(TCHAR)*(x));
//        	for(int j=0;j<x;j++)
//			{
//				sprintf(cmd_char_str2+j,"%s",cmd_str_2+j);
//			}
//			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_char_str2); // 新文件名称
////        	ret=f_rename(cmd_str_1,cmd_str_2);
//			ret=f_rename(cmd_char_str1,cmd_char_str2);
//        	if (ret != FR_OK) {
//				xil_printf("Rename File Failed! ret=%d\r\n", ret);
////				return -1;
//			}
//        }
//        // *****************                   *******************//
//        if(flag==0)
//            i=temp+strlen(cmd_char_str1)*2+2;
//        else if(flag==1)
//        	i=temp+strlen(cmd_char_str2)*2+2;
//        flag=0;
//        file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]); //文件新读写控制值
//        if(x1==1)
//        {
//        	if(file_cmd2==1)
//			{
//        			if(x0==1)    //  用新文件名
//					{
//						ret=f_chmod(cmd_char_str2,AM_RDO,AM_RDO | AM_ARC);        //  文件设成只读
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//		//					return -1;
//						}
//					}
//					else         //  用旧文件名
//					{
//						ret=f_chmod(cmd_char_str1,AM_RDO,AM_RDO | AM_ARC);         //  文件设成只读
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
////							return -1;
//						}
//					}
//        	}
//        	else if(file_cmd2==0)
//        	{
//					if(x0==1)    //  用新文件名
//					{
//						ret=f_chmod(cmd_char_str2,0, AM_ARC); 		 //  文件取消只读
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//		//					return -1;
//						}
//					}
//					else         //  用旧文件名
//					{
//						ret=f_chmod(cmd_char_str1,0,AM_ARC);         //  文件取消只读
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//							return -1;
//						}
//					}
//        	}
//        }
//        i+=4;
//        file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件新显示控制值
//        if(x2==1)
//        {
//			if(file_cmd2==1)
//			{
//					if(x0==1)    //  用新文件名
//					{
//						ret=f_chmod(cmd_char_str2,AM_HID,AM_HID | AM_ARC);
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//		//					return -1;
//						}
//					}
//					else         //  用旧文件名
//					{
//						ret=f_chmod(cmd_char_str1,AM_HID,AM_HID | AM_ARC);         //  文件设成隐藏
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//							return -1;
//						}
//					}
//			}
//			else if(file_cmd2==0)
//			{
//					if(x0==1)    //  用新文件名
//					{
//						ret=f_chmod(cmd_char_str2,0,AM_ARC);
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//		//					return -1;
//						}
//					}
//					else         //  用旧文件名
//					{
//						ret=f_chmod(cmd_char_str1,0,AM_ARC);         //  文件取消隐藏
//						if (ret != FR_OK) {
//							xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//							return -1;
//						}
//					}
//			}
//        }
//        /*********************************************/
//        i+=4;
//        temp=i;
//        for (x = 0; x < 1024; x++)
//        {
//            unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//            cmd_str_3[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//            if ((cmd_str_3[x] == '\0')||(cmd_str_3[x] == ' ')) break;
//        }
//        cmd_char_str3=(TCHAR *)malloc(sizeof(TCHAR)*(x));
//
//		for(int j=0;j<x;j++)
//		{
//			sprintf(cmd_char_str3+j,"%s",cmd_str_3+j);
//		}
//		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_char_str3);//  原文件夹名称
//
////      i=temp+2048;
////		i=temp+10;
//
//        // *****************   判断是否更改文件夹属性        *******************//
//        if(x4==1)
//        {
//        	flag=1;
//        	i=temp+strlen(cmd_char_str3)*2+2;
//        	temp=i;
//            for (x = 0; x < 512; x++)
//            {
//                unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//                cmd_str_4[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//                if ((cmd_str_4[x] == '\0')||(cmd_str_4[x] == ' ')) break;
//            }
//            cmd_char_str4=(TCHAR *)malloc(sizeof(TCHAR)*(x));
//
//			for(int j=0;j<x;j++)
//			{
//				sprintf(cmd_char_str4+j,"%s",cmd_str_4+j);
//			}
//			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_char_str4); //  新文件夹名称
//			ret=f_rename(cmd_char_str3,cmd_char_str4);
//            if (ret != FR_OK) {
//				xil_printf("Rename File Failed! ret=%d\r\n", ret);
////				return -1;
//			}
//        }
//        // *****************                    *******************//
////      i=temp+1024;
////        i=temp+10;
//        if(flag==0)
//        	i=temp+strlen(cmd_char_str3)*2+2;
//        else if(flag==1)
//        	i=temp+strlen(cmd_char_str4)*2+2;
//        file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新读写控制值
//        if(x5==1)
//        {
//        	if(file_cmd2==1)
//        	{
//					if(x4==1)           //  用新文件夹名
//					{
//						ret=f_chmod(cmd_char_str4,AM_RDO,AM_RDO | AM_ARC);
//						if (ret != FR_OK) {
//							  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//		//					  return -1;
//						}
//					}
//					else                //  用旧文件夹名
//					{
//						ret=f_chmod(cmd_char_str3,AM_RDO,AM_RDO | AM_ARC);	    //  文件夹设成只读
//						if (ret != FR_OK) {
//							  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//							  return -1;
//						}
//					}
//        	}
//        	else if(file_cmd2==0)
//        	{
//						if(x4==1)           //  用新文件夹名
//						{
//							ret=f_chmod(cmd_char_str4,0, AM_ARC);
//							if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//			//					  return -1;
//							}
//						}
//						else                //  用旧文件夹名
//						{
//							ret=f_chmod(cmd_char_str3,0, AM_ARC);	    //  文件夹设成只读
//							if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							}
//						}
//        	}
//        }
//        i+=4;
//        file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新显示控制值
//        if(x6==1)
//        {
//        	if(file_cmd2==1)
//        	{
//						if(x4==1)           //  用新文件夹名
//						{
//							  ret=f_chmod(cmd_char_str4,AM_HID,AM_HID | AM_ARC);
//							  if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//			//					  return -1;
//							  }
//						}
//						else                //  用旧文件夹名
//						{
//							  ret=f_chmod(cmd_char_str3,AM_HID,AM_HID | AM_ARC);     //  文件夹设成隐藏
//							  if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							  }
//						}
//        	}
//        	else if(file_cmd2==0)
//        	{
//						if(x4==1)           //  用新文件夹名
//						{
//							  ret=f_chmod(cmd_char_str4,0, AM_ARC);
//							  if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//			//					  return -1;
//							  }
//						}
//						else                //  用旧文件夹名
//						{
//							  ret=f_chmod(cmd_char_str3,0, AM_ARC);     //  文件夹设成隐藏
//							  if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							  }
//						}
//        	}
//
//        }
//        cmd_reply_a203_to_a204(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
//}

//文件及文件夹属性获取   主控组件->存储组件
int run_cmd_a205(StructMsg *pMsg)
{
		 int file_cmd=0,ret=0,i=0,x = 0,temp=0,h=0;
		 u16 unicode_u16=0;
		 int res;
		 WCHAR cmd_str_1[1024]={0},cmd_str_2[1024]={0};
		 BYTE cmd_str_11[100]={0},cmd_str_21[100]={0};
//		 DIR dir;
//		 FIL file;
		 file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		 i=i+4;
		 temp=i;
		 switch(file_cmd)
		 {
			case FILE_ATTRIBUTE:
				for (x = 0; x < 1024; x++)
				{
					 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
					 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
					 if(cmd_str_1[x]<=0x7E)
					 {
						 cmd_str_11[h++]=cmd_str_1[x];
					 }
					 else
					 {
						 cmd_str_11[h++]=(cmd_str_1[x]>>8);
						 cmd_str_11[h++]=cmd_str_1[x];
					 }
					 if (cmd_str_1[x] == '\0') break;
				}
				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
				res=cmd_reply_a206(pMsg, cmd_str_11);
//				res=cmd_reply_a206(pMsg, "abc");
				if(res!=0)
				{
					xil_printf("Failed to get file attributes! res=%d\r\n",res);
					cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
					return -1;
				}
				break;

			case FOLDER_ATTRIBUTE:
				i=temp+2048;
				for (x = 0; x < 1024; x++)
				{
					unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
					cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
					if(cmd_str_2[x]<=0x7E)
				    {
					    cmd_str_21[h++]=cmd_str_2[x];
				    }
				    else
				    {
					    cmd_str_21[h++]=(cmd_str_2[x]>>8);
					    cmd_str_21[h++]=cmd_str_2[x];
				    }
					if (cmd_str_2[x] == '\0') break;
				}
				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
//				res=cmd_reply_a207(pMsg, cmd_str_21);
				res=cmd_reply_a207(pMsg, "bcd");
				if(res!=0)
				{
					xil_printf("Failed to get folder properties! res=%d\r\n",res);
					cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
					return -1;
				}
				break;
			default:       // 操作失败通过a203进行回复
				xil_printf("command failed!\r\n");
//				cmd_reply_a203_to_a205(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
				cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
				return -1;
				break;
		 }
		 cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
    return 0;
}

// 文件属性获取应答  存储组件—>主控组件
int cmd_reply_a206(StructMsg *pMsg, const BYTE* path)
{
	   int Status;
	   FILINFO fno;
	   FRESULT fr;
	   u8 CreateTime1[48]={0};
	   u8 CreateTime2[48]={0};
	   u8 ChangeTime1[48]={0};
	   u8 ChangeTime2[48]={0};
	   u8 AccessTime1[48]={0};
	   u8 AccessTime2[48]={0};
	   StructA206Ack ReplyStructA206Ack={0};
	   fr = f_stat(path, &fno);
	   switch(fr)
	   {
	   	   case FR_OK:
		   break;
		   case FR_NO_FILE:
				  xil_printf("\"%s\" is not exist.\r\n", path);
				  return -1;
		   break;
           default:
				  xil_printf("An error occured. (%d)\r\n", fr);
				  return -1;
	    }
		ReplyStructA206Ack.Head = 0x55555555;
		ReplyStructA206Ack.SrcId = SRC_ID;
		ReplyStructA206Ack.DestId = DEST_ID;
#if  0
		ReplyStructA206Ack.HandType = 0xA2;
		ReplyStructA206Ack.HandId = 0x6;
		ReplyStructA206Ack.PackNum=0x0;
#endif

//		sprintf(ReplyStructA206Ack.Name,"%s",(char *)fno.fname);
		strcpy(ReplyStructA206Ack.Name,(char *)fno.fname);  //noted by lyh on the 1.22
//		convert(ReplyStructA206Ack.Name,fno.fname);				  //add   by lyh on the 1.22
		Convert_GB2312_to_UTF16LE(ReplyStructA206Ack.Name);       //add   by lyh on the 2.02

		get_path_dname(path,ReplyStructA206Ack.Dir);
		Convert_GB2312_to_UTF16LE(ReplyStructA206Ack.Dir);        //add   by lyh on the 2.02
		ReplyStructA206Ack.Size = fno.fsize;
#if  1   //改变字节顺序
		ReplyStructA206Ack.HandType = SW32(0xA2);
		ReplyStructA206Ack.HandId = SW32(0x6);
		ReplyStructA206Ack.PackNum=SW32(0x0);
		ReplyStructA206Ack.Size = SW64(ReplyStructA206Ack.Size);
#endif
//		sprintf(ReplyStructA206Ack.CreateTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间   // 11.21 tested by lyh: have some problems
//
//		sprintf(ReplyStructA206Ack.ChangeTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
//
//		sprintf(ReplyStructA206Ack.AccessTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems

		sprintf(CreateTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间
		convert(ReplyStructA206Ack.CreateTime1,CreateTime1);

		sprintf(CreateTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间
		convert(ReplyStructA206Ack.CreateTime2,CreateTime2);

		sprintf(ChangeTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.ChangeTime1,ChangeTime1);

		sprintf(ChangeTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.ChangeTime2,ChangeTime2);

		sprintf(AccessTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.AccessTime1,AccessTime1);

		sprintf(AccessTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.AccessTime2,AccessTime2);

		if((fno.fattrib & AM_RDO) == 0)
		{
				ReplyStructA206Ack.RWCtrl = 0x0;
		}
		else
		{
				ReplyStructA206Ack.RWCtrl = 0x1;
		}

		if((fno.fattrib & AM_HID) == 0)
		{
				ReplyStructA206Ack.DisplayCtrl = 0x0;
		}
		else
		{
				ReplyStructA206Ack.DisplayCtrl = 0x1;
		}
		/****** CRC *****/
	//	ReplyStructA206Ack.CheckCode = ReplyStructA206Ack.AckPackNum + \
	//			ReplyStructA206Ack.AckHandType +ReplyStructA206Ack.AckHandId + \
	//			ReplyStructA206Ack.AckResult;
		ReplyStructA206Ack.Tail = 0xAAAAAAAA;
		ReplyStructA206Ack.RWCtrl=SW32(ReplyStructA206Ack.RWCtrl);
		ReplyStructA206Ack.DisplayCtrl=SW32(ReplyStructA206Ack.DisplayCtrl);
		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)(&ReplyStructA206Ack),
					sizeof(StructA206Ack), XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
//			free(path);
//			path=NULL;
			xil_printf("SimpleTransfer failed!\r\n");
			return XST_FAILURE;
		}
		xil_printf("%s %d  sizeof(StructA206Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA206Ack));
		return 0;
}

// 文件夹属性获取应答  存储组件——>主控组件
int cmd_reply_a207(StructMsg *pMsg, const BYTE* path)
{
		int Status;
		FRESULT fr;
		FILINFO fno;
		int temp1, temp2;
		int nfile=0,ndir=0;
		u8 CreateTime1[48]={0};
		u8 CreateTime2[48]={0};
		u8 ChangeTime1[48]={0};
		u8 ChangeTime2[48]={0};
		u8 AccessTime1[48]={0};
		u8 AccessTime2[48]={0};
		StructA207Ack ReplyStructA207Ack={0};
		fr = f_stat(path, &fno);
		Num_of_Dir_and_File (path,&nfile,&ndir,0);
		switch (fr)
		{
			case FR_OK:
				break;

			case FR_NO_PATH:
				xil_printf("\"%s\" is not exist.\r\n", path);
				return -1;

			default:
				xil_printf("An error occured. (%d)\r\n", fr);
				return -1;
		}
		ReplyStructA207Ack.Head = 0x55555555;
		ReplyStructA207Ack.SrcId = SRC_ID;
		ReplyStructA207Ack.DestId = DEST_ID;
#if  0
		ReplyStructA207Ack.HandType = 0xA2;
		ReplyStructA207Ack.HandId = 0x7;     // lyh 203.8.11改
#endif
#if  1   //改变字节顺序
		ReplyStructA207Ack.HandType = SW32(0xA2);
		ReplyStructA207Ack.HandId = SW32(0x7);
		ReplyStructA207Ack.PackNum=SW32(0x0);
#endif

		strcpy((char *)ReplyStructA207Ack.Name, (char *)fno.fname);


		get_path_dname(path,ReplyStructA207Ack.Dir);
//		ReplyStructA207Ack.Size =fno.fsize;
		get_Dir_size(ReplyStructA207Ack.Name,&ReplyStructA207Ack.Size);

//		convert(ReplyStructA207Ack.Name,fno.fname);     //add   by lyh on the 1.22
//		reverse_u16(ReplyStructA207Ack.Name,a);
//		ConvertReverse(ReplyStructA207Ack.Name);
		Convert_GB2312_to_UTF16LE(ReplyStructA207Ack.Name);
		Convert_GB2312_to_UTF16LE(ReplyStructA207Ack.Dir);
		ReplyStructA207Ack.SubFolderNum =ndir;
		ReplyStructA207Ack.SubFileNum =nfile;
#if 1
		ReplyStructA207Ack.Size=SW64(ReplyStructA207Ack.Size);
		ReplyStructA207Ack.SubFolderNum =SW32(ReplyStructA207Ack.SubFolderNum);
		ReplyStructA207Ack.SubFileNum=SW32(ReplyStructA207Ack.SubFileNum);
#endif
//		sprintf(ReplyStructA207Ack.CreateTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间     //
		sprintf(CreateTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间     //
		convert(ReplyStructA207Ack.CreateTime1,CreateTime1);
//		ConvertReverse(ReplyStructA207Ack.CreateTime1);

		sprintf(CreateTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间     //
		convert(ReplyStructA207Ack.CreateTime2,CreateTime2);

		sprintf(ChangeTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.ChangeTime1,ChangeTime1);

		sprintf(ChangeTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.ChangeTime2,ChangeTime2);

		sprintf(AccessTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.AccessTime1,AccessTime1);

		sprintf(AccessTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.AccessTime2,AccessTime2);

		if ((fno.fattrib & AM_RDO) == 0)
		{
			ReplyStructA207Ack.RWCtrl = 0x0;
		}
		else
		{
			ReplyStructA207Ack.RWCtrl = 0x1;
		}

		if ((fno.fattrib & AM_HID) == 0)
		{
			ReplyStructA207Ack.DisplayCtrl = 0x0;
		}
		else
		{
			ReplyStructA207Ack.DisplayCtrl = 0x1;
		}
		/****** CRC *****/
	//	ReplyStructA207Ack.CheckCode = ReplyStructA206Ack.AckPackNum + \
	//			ReplyStructA206Ack.AckHandType +ReplyStructA206Ack.AckHandId + \
	//			ReplyStructA206Ack.AckResult;
		ReplyStructA207Ack.Tail = 0xAAAAAAAA;
		ReplyStructA207Ack.RWCtrl=SW32(ReplyStructA207Ack.RWCtrl);
		ReplyStructA207Ack.DisplayCtrl=SW32(ReplyStructA207Ack.DisplayCtrl);

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA207Ack,
					sizeof(StructA207Ack), XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
//			free(path);
//			path=NULL;
			xil_printf("SimpleTransfer failed!\r\n");
			return XST_FAILURE;
		}
		xil_printf("%s %d  sizeof(StructA207Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA207Ack));
		return 0;
}

//int run_cmd_a208(StructMsg *pMsg)  // 1.31注释掉
//{
//	    int file_cmd=0,ret=0,i=0,x = 0,temp=0,h=0;
//		u16 unicode_u16=0;
//		WCHAR cmd_str_1[1024]={0};
//		BYTE cmd_str_11[100]={0};
////		DIR dir;
////		FIL file;
////		file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
////		i=i+4;
////		temp=i;
//		for (x = 0; x < 1024; x++)
//		{
//			 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//			 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//			 if(cmd_str_1[x]<=0x7E)
//			 {
//				 cmd_str_11[h++]=cmd_str_1[x];
//			 }
//			 else
//			 {
//				 cmd_str_11[h++]=(cmd_str_1[x]>>8);
//				 cmd_str_11[h++]=cmd_str_1[x];
//			 }
//			 if (cmd_str_1[x] == '\0') break;
//		}
//		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
//		cmd_reply_a208(pMsg, cmd_str_11);
////		cmd_reply_a208(pMsg, "0:");
//	    return 0;
//}

//初始化链表
static LinkedList InitList(void)
{
//	 LinkedList List= (Node * ) malloc (sizeof(Node));          // 分配一个头结点
//	 LinkedList List= (Node * ) wjq_malloc_t (sizeof(Node));    // 分配一个头结点
	 LinkedList List= (Node * ) wjq_malloc_m (sizeof(Node));    // 分配一个头结点
	 if (List == NULL)
	 {											  			    // 内存不足分配失败
		 return -1;
	 }
	 List->next  = NULL;
	 
	 return List;
}

// 返回目录中的文件和子目录列表 	存储组件->主控组件
//int cmd_reply_a208(StructMsg *pMsg,BYTE* path)   //1.31注释
int cmd_reply_a208(BYTE* path)
{
		int Status;
		uint32_t TotalFileNum=0,TotaldirNum=0;
		LinkedList LinkList=NULL;     //12.21写
		uint32_t sum=0;
		LinkList=InitList();
		if(LinkList==NULL)
		{
			xil_printf("list allocated failed！\r\n");
			return -1;
		}
		StructA208Ack ReplyStructA208Ack;
		ReplyStructA208Ack.Head=0x55555555;
		ReplyStructA208Ack.SrcId=SRC_ID;
		ReplyStructA208Ack.DestId=DEST_ID;
#if 0
		ReplyStructA208Ack.HandType=0xA2;
		ReplyStructA208Ack.HandId=0x08;
		ReplyStructA208Ack.PackNum=0;
//		ReplyStructA208Ack.AckHandType=pMsg->HandType;
//		ReplyStructA208Ack.AckHandId=pMsg->HandId;
		ReplyStructA208Ack.AckHandType=0xA2;
		ReplyStructA208Ack.AckHandId=0x01;
#endif

		Status = Num_of_Dir_and_File(path,&TotalFileNum,&TotaldirNum,1);
		if (Status != FR_OK) {
			xil_printf("Count Failed! ret=%d\r\n",Status);
			return -1;
		}
		ReplyStructA208Ack.FileNum=TotalFileNum; //文件总个数
		ReplyStructA208Ack.DirNum=TotaldirNum;   //文件夹总个数
		sum=TotalFileNum+TotaldirNum;

#if 1    // 改变字节序
		ReplyStructA208Ack.HandType=SW32(0xA2);
		ReplyStructA208Ack.HandId=SW32(0x08);
		ReplyStructA208Ack.PackNum=SW32(0);
		ReplyStructA208Ack.AckHandType=SW32(0xA2);
		ReplyStructA208Ack.AckHandId=SW32(0x01);
		ReplyStructA208Ack.FileNum=SW32(ReplyStructA208Ack.FileNum);
		ReplyStructA208Ack.DirNum=SW32(ReplyStructA208Ack.DirNum);
#endif

#if 0
		ReplyStructA208Ack.SubpackNum;		//子包序号
		ReplyStructA208Ack.LastPack;		//最后一包标识
		ReplyStructA208Ack.SubpackFileNum;	//子包文件个数（N个）
		ReplyStructA208Ack.SubpackDirNum;	//子包文件夹个数（N个）
#endif
		ReplyStructA208Ack.CheckCode=0x0;
		ReplyStructA208Ack.Tail=0xAAAAAAAA;
		// 循环N个单个文件或文件夹目录信息
		// 1.19号之前的代码，客户不想展示每一层文件夹下的内容，只想显示一层，因此1.19号之后用另一套代码
		record_struct_of_Dir_and_File(path,LinkList);

//		ReplyStructA208Ack.message=(SingleFileOrDir  *)wjq_malloc_t(sizeof(SingleFileOrDir)*(sum+1));// 定义存储单个文件或文件夹目录信息结构体数组
		ReplyStructA208Ack.message=(SingleFileOrDir  *)wjq_malloc_m(sizeof(SingleFileOrDir)*(sum+1));// 定义存储单个文件或文件夹目录信息结构体数组
		LinkedList r=LinkList;
		for(int i=0;;i++)
        {
        	ReplyStructA208Ack.message[i]=r->data;
        	r=r->next;
        	if(r==NULL)
        		break;
        }
//  *************************************************  //
		// 打印一下每个文件和目录的结构体信息
//		for(int i=1;i<sum+1;i++)
//		{
//			printf("Type:%llu,  Size:%ll8u Byte,  Name:%16s,  Changtime:default.\r\n",ReplyStructA208Ack.message[i].type,ReplyStructA208Ack.message[i].size,ReplyStructA208Ack.message[i].name);
//		}

//  *************************************************  //

		for(int i=1;i<sum+1;i++)
		{
			Convert_GB2312_to_UTF16LE(ReplyStructA208Ack.message[i].name);
		}

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack,
        					56, XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		AxiDma.TxBdRing.HasDRE=1;
		for(int i=1;i<sum+1;i++)
		{
				Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack.message[i],
						sizeof(SingleFileOrDir), XAXIDMA_DMA_TO_DEVICE);
				if (Status != XST_SUCCESS) {
					return XST_FAILURE;
				}
				xil_printf("%s %d  sizeof(SingleFileOrDir)=%d\r\n", __FUNCTION__, __LINE__,sizeof(SingleFileOrDir));
		}

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack.CheckCode,
							4, XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack.Tail,
							4, XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}


		//销毁链表
		DestroyList(LinkList);
//		wjq_free_t(ReplyStructA208Ack.message); 	// 释放内存
		wjq_free_m(ReplyStructA208Ack.message); 	// 释放内存
		LinkList=NULL;
		return 0;
}


int run_cmd_b201(StructMsg *pMsg)
{
	int file_cmd=0,ret=0,i=0;
	BYTE Buff[4096];
	file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	switch(file_cmd)
	{
		case DISK_FORMAT:
				ret = f_mkfs(
						" ",	/* Logical drive number */
						0,			/* Format option  FM_EXFAT*/
						Buff,			/* Pointer to working buffer (null: use heap memory) */
						sizeof Buff			/* Size of working buffer [byte] */
						);
				if (ret != FR_OK)
				{
						xil_printf("f_mkfs  Failed! ret=%d\r\n", ret);
					//	cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
						return -1;
				}
//				cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		break;

		case DISK_REMOUNT:
				ret = f_mount(&fs,"",1);
				if (ret != FR_OK)
				{
						xil_printf("f_remount  Failed! ret=%d\r\n", ret);
					//	cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
						return -1;
				}
//				cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		break;

		case DISK_UNMOUNT:
				ret = f_mount(0,"",1);
				if (ret != FR_OK)
				{
						xil_printf("f_unmount  Failed! ret=%d\r\n", ret);
					//	cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
						return -1;
				}
//				cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		break;

		default:
			break;
	}
	cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
	return 0;
}

int run_cmd_d201(StructMsg *pMsg)
{
		int file_cmd=0,ret=0,i=0,x=0,h=0;
		int temp=0;
		u16 unicode_u16=0;
		WCHAR cmd_str_1[1024]={0};
		BYTE cmd_str_11[100]={0};
		int32_t file_offset=0,file_seek=0;
		FIL file1;
		BYTE *cmd_char_str1;
		for (x = 0; x < 1024; x++)
		{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if (cmd_str_1[x] == '\0') break;
		}              // 文件路径
		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
//		i=temp+strlen(cmd_str_11)*2;
		i=temp+2048;   // 1.15 改 by lyh
		file_offset = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		  // 偏移量
		i=i+4;
		file_seek = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		  // 起始位置
		ret =f_open(&file1, cmd_str_11,  FA_OPEN_EXISTING |FA_READ);
		if (ret != FR_OK)
		{
			xil_printf("f_open  Failed! ret=%d\r\n", ret);
			return -1;
		}         // 应该先打开文件
		 switch(file_seek)
		{
			   case   SEEK_SET:
				   // offset
				   ret =f_lseek(&file1,file_offset);
				   if (ret != FR_OK)
				   {
					   xil_printf("f_lseek  Failed! ret=%d\r\n", ret);
					   return -1;
				   }
				   f_close(&file1);
//				   cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
			   break;

			   case   SEEK_CUR:
				   // offset
				   ret =f_lseek(&file1,f_tell(&file1)+file_offset);
				   if (ret != FR_OK)
				   {
					   xil_printf("f_lseek  Failed! ret=%d\r\n", ret);
					   return -1;
				   }
				   f_close(&file1);
//				   cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
			   break;

			   case  SEEK_END:
				   ret =f_lseek(&file1,f_size(&file1)+file_offset);
				   if (ret != FR_OK)
				   {
					   xil_printf("f_lseek  Failed! ret=%d\r\n", ret);
					   return -1;
				   }
				   f_close(&file1);
//				   cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
			   break;
			   default:
			   //cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
			   break;
		}
		 cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		 return 0;
}

// 流写入指令
int run_cmd_d20A(StructMsg *pMsg)
{
		uint32_t ret=0,i=0,x=0,h=0;
		u16 unicode_u16=0;
		uint32_t Status=0,bw=0;
		u32 file_cmd=0;
		uint8_t sts;
		WCHAR cmd_str_1[1024]={0};
		BYTE cmd_str_11[100]={0};
		uint32_t cmd_write_cnt=0;
//		uint8_t wbuff[4096]={0};
		for (x = 0; x < 1024; x++)
		{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if (cmd_str_1[x] == '\0') break;
		}
		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);

          // 获取并解析从DMA0传过来的文件路径
//		ret = f_open(&file,cmd_str_11, FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
		ret = f_open(&file,"B", FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
//		ret = f_open(&file,"B", FA_OPEN_EXISTING | FA_WRITE |FA_READ);
		if (ret != FR_OK)
		{
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			//cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
			return ret;
		}
		xil_printf(" Open ok!\r\n");
		xil_printf("Waiting FPGA Vio Ctrl Read Write Start\r\n");
//		for(int i=0;i<4096;i++)
//		{
//			wbuff[i]=i;
//		}

		ret = f_write1(
						&file,			/* Open file to be written */
						wbuff,			/* Data to be written */
						4096*4,			/* Number of bytes to write */
						&bw				/* Number of bytes written */
		);
		if (ret != FR_OK)
		{
			 xil_printf(" f_write Failed! %d\r\n",ret);
			 f_close(&file);
			 return ret;
		}
		f_close(&file);
#if  0
		while(1)
		{
			int count=3;
			for(int i=0;i<3;i++)
			{
				sleep(1);
				xil_printf("Loading......! %d \r\n",count);
				count--;
			}

			if(cancel==1)
			{
				xil_printf("cancel write!\r\n");
				cancel=0;
				return 0;
			}
			else
				break;
		}
		while (1)
		{
//			xil_printf("Start Write!\r\n");
			if (RxReceive(DestinationBuffer,&cmd_len) == XST_SUCCESS)
			{

				buff =DestinationBuffer[0];  // 保存写入数据的DDR地址
				len  =DestinationBuffer[1];  // 写入数据的长度

				ret = f_write1(
					&file,			/* Open file to be written */
					buff,			/* Data to be written */
					len,			/* Number of bytes to write */
					&bw				/* Number of bytes written */
				);
				if (ret != FR_OK)
				{
					 xil_printf(" f_write Failed! %d\r\n",ret);
					 f_close(&file);
					 return ret;
				}
				cmd_write_cnt += 1;
//				xil_printf("buff:0x%lx \r\n",buff);
			}
			else
			{
				for(i=0;i<NHC_NUM;i++)
				{
					do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
				}
			}

			for(i=0;i<NHC_NUM;i++)
			{
				while (nhc_queue_ept(i) == 0)
				{
					do {
						sts = nhc_cmd_sts(i);
					}while(sts == 0x01);
				}
			}
//			if(cmd_write_cnt>15)    // 写256M
//			if(cmd_write_cnt>31)   	// 写512M
//			if(cmd_write_cnt>127)  	// 写2G
//			if(cmd_write_cnt>159)  	// 写2.56G
//			if(cmd_write_cnt>191)  	// 写3G
//			if(cmd_write_cnt>223) 	// 写3.58G
//			if(cmd_write_cnt>255) 	// 写4G
//			if(cmd_write_cnt>319) 	// 写5G

//写盘256M
//			if(cmd_write_cnt>15)  // 写2G
			if(cmd_write_cnt>23)  // 写3G
//			if(cmd_write_cnt>39)  // 写5G
			{
				xil_printf("I/O Write Finish!\r\n");
				xil_printf("w_count = %u\r\n",cmd_write_cnt);
				for(i=0;i<NHC_NUM;i++)
				{
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
				}
//				return 0;       // 2023.8.21
				break;
			}
		 }   // while
//		 fclose(&file);
//		 cleanup_platform();
//		 run_cmd_d205(0);
		 return 0;
#endif
}

//////******读文件命令*******//////
int run_cmd_d204(StructMsg *pMsg)
{
     int temp=0,ret=0,i=0,x=0,h=0;
	 int Read_mode,Read_speed;
	 u16 unicode_u16=0;
	 WCHAR cmd_str_1[1024]={0};
	 BYTE cmd_str_11[100]={0};
	 FIL file;

     for (x = 0; x < 1024; x++)
     {
      	 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
    	 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
    	 if(cmd_str_1[x]<=0x7E)
		 {
			 cmd_str_11[h++]=cmd_str_1[x];
		 }
		 else
		 {
			 cmd_str_11[h++]=(cmd_str_1[x]>>8);
			 cmd_str_11[h++]=cmd_str_1[x];
		 }
         if (cmd_str_1[x] == '\0') break;
    }           // 文件路径
    xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
//  i=temp+strlen(cmd_str_11)*2;
    i=temp+2048;
	Read_mode=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	i+=4;
	Read_speed=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	switch(Read_mode)
	{
        case 0x0:    // 分包读取
        	switch(Read_mode)
        	{

//        	        case 0x0:    // 0.5Gbps
//
//        			break;
//
//        	        case 0x1:    // 1Gbps
//
//        			break;
//
//        			case 0x2:    // 2Gbps
//
//        			break;
//
//        	        case 0x3:    // 3Gbps
//
//        			break;
//
//        			case 0x4:    // 4Gbps
//
//        			break;
//
//        	        case 0x5:    // 5Gbps
//
//        			break;
//        	        default:
//
//        	        break;
        	}
//          run_cmd_d205(StructMsg *pMsg);
		break;
//
//        case 0x1:    // 流式读取
//        	switch(Read_mode)
//           {
//        	        case 0x0:    // 0.5Gbps
//
//        	        break;
//
//        	        case 0x1:    // 1Gbps
//
//        	        break;
//
//        	        case 0x2:    // 2Gbps
//
//        	        break;
//
//        	        case 0x3:    // 3Gbps
//
//        	        break;
//
//        	        case 0x4:    // 4Gbps
//
//        	        break;
//
//        	        case 0x5:    // 5Gbps
//
//        	        break;
//        	        default:
//        	        break;
//           }
//         run_cmd_d206(StructMsg *pMsg);
//		break;
	}
	return 0;
}

/*****************分包读取文件命令*******************/
int run_cmd_d205(StructMsg *pMsg)
{
	 int i=0,x=0,Status,ret,h=0;
	 int value=0;
	 int Checknum=0,Sign=0;
	 u16 unicode_u16=0;
	 uint8_t sts;
	 WCHAR cmd_str_1[1024]={0};
	 BYTE cmd_str_11[100]={0};
	 u32 file_cmd=0;
	 int br;
	 uint32_t r_count=0;
	 file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	 i=i+4;
     for (x = 0; x < 1024; x++)
     {
			 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			 if(cmd_str_1[x]<=0x7E)
			 {
				 cmd_str_11[h++]=cmd_str_1[x];
			 }
			 else
			 {
				 cmd_str_11[h++]=(cmd_str_1[x]>>8);
				 cmd_str_11[h++]=cmd_str_1[x];
			 }
			 if (cmd_str_1[x] == '\0') break;
	 }       // 文件路径
	 xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
	 sleep(1);
//	 f_close(&file);
//	 ret = f_open(&file,cmd_str_11, FA_OPEN_EXISTING |FA_READ);
	 ret = f_open(&file,"B", FA_OPEN_EXISTING |FA_READ|FA_WRITE);
	 if (ret != FR_OK)
	 {
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			//cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
			return ret;
	  }

	 XAxiDma1_tx(XPAR_AXIDMA_1_DEVICE_ID);
     //  分包数据传输
	 while(1)
	 {
//		 	 xil_printf("Start Read!\r\n");
			 len= OFFSET_SIZE;
			 ret = f_read1(
							&file,
							buff_r,
							len,
							&br
			 );
			 if (ret != FR_OK)
			 {
					xil_printf("f_read Failed! ret=%d\r\n", ret);
					return ret;
			 }
//			 r_count++;
//			 xil_printf("r_count=%d\r\n",r_count);
//			 if (buff_r < (DDR3_END_ADDR-OFFSET_SIZE))
//				  buff_r += OFFSET_SIZE;
//			 else
//				  buff_r = MEM_DDR3_BASE+(512*3*1024*1024);
#if   0   // 回读存放的地址，为固定0xE0000000开始的32M区域
			uint32_t last=(void *)(0xE2000000);
			int32_t check_block_size=0x100000;
			if(CheckOut_HeadTail((uint32_t *)buff_r,(uint32_t *)last,check_block_size)!=0)
			{
				xil_printf("check error!\r\n");
//					return 1;
			}
			Checknum++;
			xil_printf("                                                                          Checknum:%d\r\n",Checknum);
			r_count++;
//						if(r_count==105)
//						{
//							xil_printf("here!\r\n");
//						}
			DestinationBuffer_1[0]=buff_r;
			DestinationBuffer_1[1]=len;
			ret = TxSend_1(DestinationBuffer_1,8);
			if (ret != XST_SUCCESS)
			{
				 xil_printf("TxSend Failed! ret=%d\r\n", ret);
				 return ret;
			}
			XLLFIFO_SysInit();
#endif

#if   1   // 回读存放的地址为:0xE0000000~0xFFFFFFFF
//			len=OFFSET_SIZE;
			uint32_t last=(void *)(0xE2000000+Sign*len);
			if (buff_r == MEM_DDR3_BASE+(512*3*1024*1024))
			{
				last = 0xE2000000;
				Sign=0;
			}
			else if(buff_r == 0xFE000000)
			{
				last = 0xFFFFFFFF;
			}
			int32_t check_block_size=0x100000;
			if(CheckOut_HeadTail((uint32_t *)buff_r,(uint32_t *)last,check_block_size)!=0)
			{
				xil_printf("check error!\r\n");
//					return 1;
			}
			Checknum++;
			xil_printf("                                                                          Checknum:%d\r\n",Checknum);
			r_count++;
//			for(int k=0;k<4096;k++)
//			{
//				Xil_Out32(buff_r+k*4,value++);
//			}
			DestinationBuffer_1[0]=buff_r;
			DestinationBuffer_1[1]=len;
			XLLFIFO_SysInit();
			ret = TxSend_1(DestinationBuffer_1,8);
			if (ret != XST_SUCCESS)
			{
				 xil_printf("TxSend Failed! ret=%d\r\n", ret);
				 return ret;
			}

			if (buff_r <(DDR3_END_ADDR-OFFSET_SIZE))
			{
				buff_r += OFFSET_SIZE;
			}
			else
			{
				buff_r = MEM_DDR3_BASE+(512*3*1024*1024);
			}
			Sign++;
#endif
			 for(i=0;i<NHC_NUM;i++)
			 {
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
			 }

//			 if(r_count>15)   		// 写512M
//	 		 if(r_count>63)  		// 写2G
//	 		 if(r_count>79)  		// 写2.56G
	 		 if(r_count>95)  		// 写3G
//	 		 if(r_count>111) 		// 写3.58G
//			 if(r_count>127) 		// 写4G
//	 		 if(r_count>159)		// 写5G
			 {
					xil_printf("I/O Read or Write Test Finish!\r\n");
					xil_printf("r_count=%u\r\n",r_count);
					for(i=0;i<NHC_NUM;i++)
					{
						while (nhc_queue_ept(i) == 0)
						{
							do {
								sts = nhc_cmd_sts(i);
							}while(sts == 0x01);
						}
					}
					break;
			}

	 }  //while
	 f_close(&file);
//	 cleanup_platform();
	 return 0;
}

////////*****流式读取文件命令*******/////////
//int run_cmd_d206(StructMsg *pMsg)
//{
////	 int i=0,x=0;
////	 u16 unicode_u16=0;
////	 WCHAR cmd_str_1[1024];
////	 FIL file;
////
////     for (x = 0; x < 1024; x++) {
////      	 unicode_u16=(pMsg->MsgData[i++]<<8|pMsg->MsgData[i++]);
////    	 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
////         if (cmd_str_1[x] == '\0') break;
////    }         // 文件路径
////           //  流式写入
//	return 0;
//}

//*********文件写入校验错误反馈报文结构*********//
int run_reply_d207(StructMsg *pMsg)
{
	    int i=0,x=0;
		u16 unicode_u16=0;
		WCHAR cmd_str_1[1024];
		FIL file;
		int32_t reply_Handpocket_num;          //包序列号
		int32_t reply_HandType;
		int32_t reply_HandId;
		int32_t reply_Content;
		reply_Handpocket_num=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i+=4;
		reply_HandType=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i+=4;
		reply_HandId=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i+=4;
		reply_Content=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	    switch(reply_Content)
	    {
	        case 0x01:

	    	break;

	    }
	return 0;
}


//*********文件读取校验错误反馈报文结构*********//
int run_reply_d208(StructMsg *pMsg)
{
		int i=0,x=0;
		u16 unicode_u16=0;
		WCHAR cmd_str_1[1024];
		FIL file;
		int32_t reply_Handpocket_num;          //包序列号
		int32_t reply_HandType;
		int32_t reply_HandId;
		int32_t reply_Content;
		reply_Handpocket_num=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i+=4;
		reply_HandType=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i+=4;
		reply_HandId=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i+=4;
		reply_Content=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		switch(reply_Content)
		{
			case 0x01:


			break;
		}


		   return 0;
}


int cmd_reply_a203(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
#if   0
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;
#endif
		switch(type)
		{
			case  0xA2:
				switch(id)
				{
					case  0x01:
						if(result_a201==result)
						{
							ReplyStructA203Ack.AckResult = result_a201;
						}
						else
						{
							result_a201=result;
							ReplyStructA203Ack.AckResult = result_a201;
						}
					break;

					case  0x04:
						if(result_a204==result)
						{
							ReplyStructA203Ack.AckResult = result_a204;
						}
						else
						{
							result_a204=result;
							ReplyStructA203Ack.AckResult = result_a204;
						}
					break;

					case  0x05:
						if(result_a205==result)
						{
							ReplyStructA203Ack.AckResult = result_a205;
						}
						else
						{
							result_a205=result;
							ReplyStructA203Ack.AckResult = result_a205;
						}
					break;
					default:
					break;
				}
			break;

			case  0xB2:
				if(result_b201==result)
				{
					ReplyStructA203Ack.AckResult = result_b201;
				}
				else
				{
					result_b201=result;
					ReplyStructA203Ack.AckResult = result_b201;
				}
			break;

			case  0xD2:
				if(result_d201==result)
				{
					ReplyStructA203Ack.AckResult = result_d201;
				}
				else
				{
					result_d201=result;
					ReplyStructA203Ack.AckResult = result_d201;
				}
			break;

			case  0xF2:
				if(result_f201==result)
				{
					ReplyStructA203Ack.AckResult = result_f201;
				}
				else
				{
					result_f201=result;
					ReplyStructA203Ack.AckResult = result_f201;
				}
			break;

			default:
			break;
		}
		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;

		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;

#if 1    // 改变字节序
		ReplyStructA203Ack.HandType = SW32(0xA2);
		ReplyStructA203Ack.HandId =  SW32(0x03);
		ReplyStructA203Ack.PackNum = SW32(0x0);   //need change
		ReplyStructA203Ack.AckPackNum = SW32(ReplyStructA203Ack.AckPackNum);
		ReplyStructA203Ack.AckHandType = SW32(ReplyStructA203Ack.AckHandType);
		ReplyStructA203Ack.AckHandId = SW32(ReplyStructA203Ack.AckHandId);
		ReplyStructA203Ack.CheckCode = SW32(ReplyStructA203Ack.CheckCode);
		ReplyStructA203Ack.AckResult = SW32(ReplyStructA203Ack.AckResult);
#endif
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

		if (Status != XST_SUCCESS)
		{
			return XST_FAILURE;
		}       //  a202 会卡住
		xil_printf("%s %d result:0x%x\r\n", __FUNCTION__, __LINE__,result_a201);

		return 0;
}

int cmd_reply_a203_to_a201(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
#if   0
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;   //need change
#endif
		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;

	/***************lyh 2023.8.15 改******************************/
		if(result_a201==result)
			ReplyStructA203Ack.AckResult = result_a201;
		else
			result_a201=result;
			ReplyStructA203Ack.AckResult = result_a201;
	/************************************************************/
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);

		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;

#if 1    // 改变字节序
		ReplyStructA203Ack.HandType = SW32(0xA2);
		ReplyStructA203Ack.HandId =  SW32(0x3);
		ReplyStructA203Ack.PackNum = SW32(0x0);   //need change
		ReplyStructA203Ack.AckPackNum = SW32(ReplyStructA203Ack.AckPackNum);
		ReplyStructA203Ack.AckHandType = SW32(ReplyStructA203Ack.AckHandType);
		ReplyStructA203Ack.AckHandId = SW32(ReplyStructA203Ack.AckHandId);
		ReplyStructA203Ack.CheckCode = SW32(ReplyStructA203Ack.CheckCode);
		ReplyStructA203Ack.AckResult = SW32(ReplyStructA203Ack.AckResult);
#endif
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	//	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
	//				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
//		CmdTxBufferPtr = (u8 *)&ReplyStructA203Ack;   //????wfeng

		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));


//		*CmdTxBufferPtr='h';
		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}       //  a202 会卡住
		xil_printf("%s %d result:0x%x\r\n", __FUNCTION__, __LINE__,result_a201);

		return 0;
}

int cmd_reply_a203_to_a204(u32 packnum, u32 type, u32 id, u32 result)
{
	    int Status;
		StructA203Ack ReplyStructA203Ack;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;   //need change

		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;
	/***************lyh 2023.8.15 改******************************/
		if(result_a204==result)
		    ReplyStructA203Ack.AckResult = result_a204;
		else
		    result_a204=result;
		    ReplyStructA203Ack.AckResult = result_a204;
	/************************************************************/
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	//	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
	//				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
		CmdTxBufferPtr = (u8 *)&ReplyStructA203Ack;   //????wfeng
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));
		Status = XAxiDma_SimpleTransfer(&AxiDma,CmdTxBufferPtr,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);


		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		return 0;
}

int cmd_reply_a203_to_a205(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;   //need change

		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;
	/***************lyh 2023.8.15 改******************************/
		if(result_a205==result)
			ReplyStructA203Ack.AckResult = result_a205;
		else
			result_a205=result;
			ReplyStructA203Ack.AckResult = result_a205;
	/************************************************************/
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	//	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
	//				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
		CmdTxBufferPtr = (u8 *)&ReplyStructA203Ack;   //????wfeng
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));
		Status = XAxiDma_SimpleTransfer(&AxiDma,CmdTxBufferPtr,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		return 0;
}

int cmd_reply_a203_to_a208(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;   //need change

		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;
	/***************lyh 2023.8.15 改******************************/
		if(result_a208==result)
			ReplyStructA203Ack.AckResult = result_a208;
		else
			result_a208=result;
			ReplyStructA203Ack.AckResult = result_a208;
	/************************************************************/
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	//	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
	//				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
		CmdTxBufferPtr = (u8 *)&ReplyStructA203Ack;   //????wfeng
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));
		Status = XAxiDma_SimpleTransfer(&AxiDma,CmdTxBufferPtr,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		return 0;
}

int cmd_reply_a203_to_b201(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;   //need change

		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;
	/***************lyh 2023.8.15 改******************************/
		if(result_b201==result)
			ReplyStructA203Ack.AckResult = result_b201;
		else
			result_b201=result;
			ReplyStructA203Ack.AckResult = result_b201;
	/************************************************************/
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	//	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
	//				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
		CmdTxBufferPtr = (u8 *)&ReplyStructA203Ack;   //????wfeng
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));
		Status = XAxiDma_SimpleTransfer(&AxiDma,CmdTxBufferPtr,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		return 0;
}

int cmd_reply_a203_to_d201(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;   //need change

		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;
	/***************lyh 2023.8.15 改******************************/
		if(result_d201==result)
			ReplyStructA203Ack.AckResult = result_d201;
		else
			result_d201=result;
			ReplyStructA203Ack.AckResult = result_d201;
	/************************************************************/
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	//	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
	//				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);
		CmdTxBufferPtr = (u8 *)&ReplyStructA203Ack;   //????wfeng
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));
		Status = XAxiDma_SimpleTransfer(&AxiDma,CmdTxBufferPtr,
				sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		return 0;
}

int cmd_reply_a203_to_f201(u32 packnum, u32 type, u32 id, u32 result)
{
		return 0;
}



int run_cmd_f201(StructMsg *pMsg)
{
		cmd_reply_health_f201();
}

int cmd_reply_health_f201(void)
{
		int Status;
		StructHealthStatus SendStructHealthStatus;
		SendStructHealthStatus.Head = 0x55555555;
		SendStructHealthStatus.SrcId = SRC_ID;
		SendStructHealthStatus.DestId = DEST_ID;
		SendStructHealthStatus.HandType = 0xF2;
		SendStructHealthStatus.HandId = 0x1;
	//	SendStructHealthStatus.PackNum = ;

		Storage_state1(&SendStructHealthStatus.TotalCap,&SendStructHealthStatus.UsedCap,
				&SendStructHealthStatus.RemainCap,&SendStructHealthStatus.FileNum);

		Storage_state2(&SendStructHealthStatus.WorkStatus,&SendStructHealthStatus.WorkTemp,
				&SendStructHealthStatus.Power,&SendStructHealthStatus.PowerUpNum);

		SendStructHealthStatus.CheckCode = SendStructHealthStatus.TotalCap + \
				SendStructHealthStatus.UsedCap + SendStructHealthStatus.RemainCap + \
				SendStructHealthStatus.FileNum + SendStructHealthStatus.WorkStatus + \
				SendStructHealthStatus.WorkTemp + SendStructHealthStatus.Power + \
				SendStructHealthStatus.PowerUpNum;
		SendStructHealthStatus.Tail = 0xAAAAAAAA;

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &SendStructHealthStatus,
					sizeof(SendStructHealthStatus), XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			xil_printf("SimpleTransfer failed!\r\n");
			return XST_FAILURE;
		}
		return 0;
}
