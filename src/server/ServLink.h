/***********************************************************************
* @ IOCPע������
* @ brief
	1��WSASend
		��һ�ν�Ͷ��һ��SendIO�����Ͷ���£���ĳ��DoneIOֻ�����˲������ݣ�tcp���ʹ�����
		�����⣬ͬʱͶ�ݶ��SendIO��ʵ�ʵ�IO�������ܲ��������
		��PostSend������������(��ͬ�߳�)Ҫ�������߼��߳�SendMsg��IO�̲߳����������̶߳���Ͷ��
		�����ͳ��ȹ�������socket������������tcp��������[TCP Zerowindow]
	2���Զ�ֻConnect�������ݣ�DoneIO���ᱻ�ص�
	3��DoneIO��dwNumberOfBytesTransferredΪ��
		��socket�رյĻص�����ֵΪ0
		��recvBuf̫С��tcp�������ڱ�����������[TCP Zerowindow]ʱҲ��Ϊ0
		��dwErrorCode��Чʱ��dwNumberOfBytesTransferred�Ƿ���Ϊ0������֤
	4��WSAENOBUFS����
		��ÿ�������ص��ύһ��send��receive������ʱ������ָ���ķ��ͻ���ջ������ͱ�������
		�����ڴ滺�����������󣬽����ܴ������ڴ���з�ҳ
		������ϵͳ��һ����������������ƣ�һ������������������ƣ��ͻ����WSAENOBUFS����
	5��WSARecv
		����Ŀ���뿴��WSARecvҲ��ÿ��ֻͶ��һ���ģ���ɻص���PostRecv��һ��
		�����ջ���̫С��������ʧ
		�������÷���Recvʱ������Recvһ������Ϊ0��buf����������ʱ��ص��㣬��ȥRecv�����ĳ���
		����Ϊ�����ύ����û�л�����ʱ����ôҲ��������ڴ汻������
		��ʹ�����ְ취�󣬵����receive�����¼���ɷ���ʱ����socket�ײ㻺���������ݻ�ԭ�ⲻ���Ļ������ж�û�б���ȡ��receive�����Ļ�������
		����ʱ�����������Լ򵥵ĵ��÷�����ʽ��recv��socket�������е�����ȫ����������һֱ��recv���� WSAEWOULDBLOCK Ϊֹ
		��������Ʒǳ��ʺ���Щ����������������������ȡ�޴󲢷��������ķ�����
		���á���������recv����socketʱ����Ԥ�Ʒ��������б��������������Կ���Ͷ��һ������receive��ȡ������������recv��
* @ author zhoumf
* @ date 2016-7-15
************************************************************************/
#pragma once
//////////////////////////////////////////////////////////////////////////
// ʹ��winsock2  ����ͬwinsock��ͻ
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
//////////////////////////////////////////////////////////////////////////
#include <Mswsock.h> // AcceptEx
#include "..\..\tool\buffer.h"
#include "..\..\tool\cLock.h"
#include "define.h"
#include <time.h>

//#define _Use_ArrayBuf

class ServLink;
class ServLinkMgr;
struct ServerConfig;

enum EnumIO{ IO_Write, IO_Read };

struct My_OVERLAPPED : public OVERLAPPED
{
	ServLink* client;
	EnumIO	  eType;

	void SetLink(ServLink* p)
	{
		memset((char *)this, 0, sizeof(OVERLAPPED));
		client = p;
	}
};

class ServLink{
	enum EStatus{ STATE_DEAD, STATE_ACCEPTING, STATE_CONNECTED };
public:
	ServLink(ServLinkMgr* p, DWORD sendbuffsize);
	~ServLink();

	static WORD s_nID;

	EStatus _eState;
	void OnConnect();
	bool CloseLink();
	bool CreateLinkAndAccept();
	void UpdateAcceptAddr();

	void DoneIOCallback(DWORD dwNumberOfBytesTransferred, EnumIO type);

	bool _bCanWrite;            // һ��::WSASend()��ϣ���������һ��
	void OnSend_DoneIO(DWORD dwNumberOfBytesTransferred);
	void ServerRun_SendIO();	// �ⲿ�̵߳���
	bool PostSend(char* buffer, DWORD size);		// Ͷ��һ������IO(д)����Add a packet to output buffer
	char* OnRead_DoneIO(DWORD size);		        // Retrieve a packet from input buffer
	bool PostRecv(char* buf);		                // Ͷ��һ������IO(��)����Do actual receive
	int RecvMsg(const char* buffer, DWORD size);

	void Maintain(time_t timenow);

	bool SendMsg(stMsg& msg, DWORD msgSize);

	LPCSTR GetIP(){ return _szIP; }
	int GetID(){ return _nLinkID; }

	bool IsSocket(){ return _sClient != INVALID_SOCKET; }
	bool IsConnected(){ return _eState == STATE_CONNECTED; }
	bool IsInvalid(){ return _bInvalid != 0; }
	void Invalid(InvalidMessageEnum eReason){
		_bInvalid = true;
		time(&_timeInvalid);
		if (_sClient != INVALID_SOCKET){
			shutdown(_sClient, SD_RECEIVE);
			printf("shutdown socket IP:%s - ID:%d - EnumReason:%d\n", _szIP, _nLinkID, eReason);
		}
	}

	InvalidMessageEnum _eLastError;
	void OnInvalidMessage(InvalidMessageEnum e, int nErrorCode, bool bToClient, int nParam = 0);
	void HandleClientMessage(stMsg* p, DWORD size){ printf("%s\n", (char*)p); /*������ѭ����Ϣ����*/ }
	void HandleNetMessage(stMsg* p, DWORD size){}

	void Err(LPCSTR sz){
		printf("%s:%d - ID:%d\n", sz, WSAGetLastError(), _nLinkID);
	}
	void Err(LPCSTR sz, DWORD err){
		printf("%s:%d - ID:%d\n", sz, err, _nLinkID);
	}


	DWORD _recvPacket;		//�õ��˶�����Ϣ
	time_t _recvPacketTime;	//���ĸ�ʱ��㿪ʼ�����
	time_t _recvIOTime;
	__forceinline void RecvIOTimeStart(time_t timenow)
	{
		_recvPacket = 0;
		_recvPacketTime = timenow;
		_recvIOTime = timenow;
	}
	__forceinline void LastRecvIOTime(time_t timenow){ _recvIOTime = timenow; }
	__forceinline int RecvIOElapsed(time_t timenow){ return (int)(timenow - _recvIOTime); }

	//������֤
	DWORD m_dwLastHeart;
	void OnHeartMsg(){ m_dwLastHeart = GetTickCount(); }
	void CheckHeart()
	{
		DWORD now = GetTickCount();
		if (now - m_dwLastHeart > 60000)
		{
			OnInvalidMessage(Net_HeartKick, 0, true);
		}
		m_dwLastHeart = now;
	}

private:
	int _nCharID; // ��ɫ�ڴ���е�ID��������(5000)������ID��Ч��ʾLink��������������
	int _nLinkID; // ServerLink�Լ���ID�����Ժܴ�

#ifdef _Use_ArrayBuf
	DWORD _nRecvSize;		// how much in buffer
	char* const _recvBuf;	// ���ջ��壬ָ�벻�ɸ�
#else
	net::Buffer _recvBuf;
#endif
	net::Buffer _sendBuf;

	CRITICAL_SECTION _csLock;

	My_OVERLAPPED _ovRecv;	// Used in WSARead() calls
	My_OVERLAPPED _ovSend;	// Used in WSASend() calls

	char _szIP[MAX_IP];
	sockaddr_in _local;
	sockaddr_in _peer;
	SOCKET _sClient;		// Socket used to communicate with client
	WSAEVENT _hEventClose;

	ServLinkMgr* const _pMgr;
	const ServerConfig& Config();

	LONG _bInvalid;
	time_t _timeInvalid;
};