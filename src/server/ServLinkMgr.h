/***********************************************************************
* @ IOCP网络库
* @ brief
	1、Client主动断开，能保证收到socket close否？
		·客户端主动close、关进程，有DoneIO回调，且“dwNumberOfBytesTransferred = 0”
	2、Client强杀进程，心跳检查
		·手动杀进程：操作系统TCP模块会触发关闭，对端可收到socket close
		·直接关机：对端什么都不知道了，须靠心跳包
	3、包有效性验证(大小、格式、加密)
	4、控制Client发包频率
	5、某链接消息堆积，主动踢出
	6、网络波动，整体延时，发送缓冲堆积，内存震荡
	7、糊涂窗口综合征，多会设置TCP_NODELAY禁用Nagle算法，网络库自己管理数据发送
		·消息积累超过定长后才调底层API
		·另辟线程(？)定期(50ms)发送所有buffer
	8、SYN攻击，防止大量链接被无效占用
	9、中间网络硬件引起的socket close，例如路由器炸了、网线被老鼠啃了
		·属于非业务逻辑异常，一段时间后会触发TCP的RST重置，对应的socket也不能用了，会有DoneIO回调
		·是否同“心跳包”功能重合？心跳检查的断开时限长些(60s)，网络异常检查间隔短很多(5s)
	*、手游网络闪断？
* @ author zhoumf
* @ date 2016-7-15
************************************************************************/
#pragma once
//////////////////////////////////////////////////////////////////////////
// 使用winsock2  避免同winsock冲突
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
//////////////////////////////////////////////////////////////////////////

const int c_nMaxLink = 2/*50000*/;

class ServLink;
class Thread;
struct ServerConfig;
struct stMsg;

class ServLinkMgr{
public:
	ServLinkMgr(const ServerConfig& info);
	~ServLinkMgr();

	const ServerConfig& _config;

	time_t _timeNow;

	static bool InitWinsock();
	static bool CleanWinsock();
	static bool IsValidIP(LPCSTR szIP){ return true; }

	// 创建监听socket，并先投递几个AcceptEx
	bool CreateServer();
	bool Close();

	bool ThreadStart();	//create a thread to run !
	bool RunThread();
	void BroadcastMsg(stMsg& msg, DWORD msgSize);

	// 检查sClient(ServerLink)，若Accept数量不够(创建监听socket时预先投递了几个Accept)，继续增加
	void Maintain(time_t timenow);

	ServLink* GetLink(DWORD id){ return id < (DWORD)_nMaxLink ? _arrLink[id - 1] : NULL; }
	SOCKET GetListener(){ return _sListener; }

	// 原子性操作：变更connect、accept、invalid数，两个原子操作中间有竞态的，会不会出Bug？
	void LinkOnCreate(int id){
		InterlockedIncrement(&_nAccept);
		InterlockedDecrement(&_nInvalid);
	}
	void LinkOnConnect(int id){
		InterlockedIncrement(&_nConnect);
		InterlockedDecrement(&_nAccept);
	}
	void LinkOnAcceptClose(int id){
		InterlockedIncrement(&_nInvalid);
		InterlockedDecrement(&_nAccept);
	}
	void LinkOnClose(int id){
		InterlockedIncrement(&_nInvalid);
		InterlockedDecrement(&_nConnect);
	}
private:
	int	_nMaxLink;
	LONG _nInvalid;
	LONG _nAccept;     // 正在等待连接的socket数量
	LONG _nConnect;    // 已连接的socket数量

	Thread* _pThread;

	SOCKET	_sListener;
	ServLink* _arrLink[c_nMaxLink];
};

/************************************************************************/
// 示例
#ifdef _MY_Test
#include "define.h"
void RunServerIOCP()
{
	cout << "———————————— RunServerIOCP ————————————" << endl;
	ServerConfig config;
	config.strIP = "127.0.0.1";
	config.dwPort = 4567;

	ServLinkMgr mgr(config);
	ServLinkMgr::InitWinsock();
	mgr.CreateServer();

	mgr.ThreadStart();
	Sleep(1000 * 600);
}
#endif