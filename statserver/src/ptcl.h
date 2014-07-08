/*
 * ptcl.h
 *	
 *  ���ƣ�Э��
 *  ���ܣ�����Э���
 *  
 *  Created on: 2011-10-13
 *      Author: fengjianhua
 *        Mail: jianhuafeng@sohu-inc.com 56683216@qq.com
 *
 *  �޸ļ�¼��
 *  ��1��LOGIN3�����������ţ���ʵ��
 */

#ifndef STATS_PTCL_H_
#define STATS_PTCL_H_

typedef enum _CMD {
	STATS_HEART_REQ_CMD = 0x91,
	STATS_HEART_RSP_CMD = 0x92,
	STATS_SPEED_REQ_CMD = 0x93,
	STATS_SPEED_RSP_CMD = 0x94,/*������1.2.0.0��ǰ�汾��Э��*/
	CMD_STATS_LOGIN = 0x9001,
	CMD_STATS_HEART_BEAT = 0x9002,
	CMD_STATS_SPEED = 0x9003,
	CMD_STATS_P2PINFO = 0x9008,
	CMD_STATS_ERRINFO = 0x9009,
    CMD_ADMIN_USER_CODE = 13041701,
    CMD_CHECK_USER_CODE = 13041702,
    CMD_CHECK_UPGRADE = 13041703,
    CMD_REPORT_UPGRADE = 13041704
} CMD;

/*�ⲿ�ӿ�Э��*/
#pragma pack(1)

//1.2.0.0��ǰ������������Э��
typedef struct _OLD_HEAD {
	uint16 pkglen;
	uint32 stx;
	uint16 akg_id;
	uint32 randNum;
	uint16 protocalVersion;
} OLD_HEAD;

typedef struct _OLD_HEART {
	uint32 cliver;
	uint32 uid;
	uint32 lip;
} OLD_HEART;

//1.2.0.0Э��, �������µľ���Э��
typedef struct _LOGIN {
	uint32 cliver;
	uint32 updatever;
	uint32 lip;
	uint32 uid;
	uint32 osver;
	uint32 osver2;
} LOGIN;

typedef struct _SPEED {
	uint16 speed;
} SPEED;

typedef struct _OLI_DOWN {
	uint32 cliver;
	uint32 lip;
	uint32 code;
} OLI_DOWN;

typedef struct _OLI_INSTALL {
	uint32 cliver;
	uint32 lip;
	uint32 code;
} OLI_INSTALL;

//1.3.0.0
typedef struct _LOGIN2 {
	uint32 cliver;
	uint32 updatever;
	uint32 lip;
	uint32 uid;
	uint32 osver;
	uint32 osver2;
	ubyte starttype;
	ubyte nat;
} LOGIN2;

//����������chid
typedef struct _LOGIN3 {
	uint32 cliver;
	uint32 updatever;
	uint32 lip;
	uint32 uid;
	uint32 osver;
	uint32 osver2;
	ubyte starttype;
	ubyte nat;
	uint16 chid;
} LOGIN3;

typedef struct _NATCONN {
	ubyte nat;
	uint16 success; //���ӳɹ���
	uint16 failed; //����ʧ����
} NATCONN;

typedef struct _P2PINFO {
	uint16 cdnspeed; //kBps
	uint16 p2pspeed; //kBps
	uint32 p2pbyte;
	uint32 cdnbyte;
	ubyte itemnum;
	NATCONN natconn[0];
} P2PINFO;

typedef struct _ERRINFO {
	uint16 len;
	char info[0];
} ERRINFO;

#pragma pack()

#endif /* STATS_PTCL_H_ */