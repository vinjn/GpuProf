/*------------------------------------------------------------------------------------------
  PERFORMANCE DATA HELPER 클래스

 PDH API의 성능 카운터를 이용하여 OS, 응용 프로그램, 서비스, 드라이버의 성능을 모니터링


- 사용법

	CPDH* pPdh = new CPDH();

	/// Add Counter ///
	int nIdx_CpuUsage = -1;
	int nIdx_MemAvail = -1;
	int nIdx_NpMem = -1;
	int nIdx_EthernetRecv = -1;
	int nIdx_EthernetSend = -1;
	if (pPdh->AddCounter(df_PDH_CPUUSAGE_TOTAL, nIdx_CpuUsage) != ERROR_SUCCESS)
		return;
	if (pPdh->AddCounter(df_PDH_MEMAVAIL_MB, nIdx_MemAvail) != ERROR_SUCCESS)
		return;
	if (pPdh->AddCounter(df_PDH_NONPAGEDMEM_BYTES, nIdx_NpMem) != ERROR_SUCCESS)
		return;
	if (pPdh->AddCounter(df_PDH_ETHERNETRECV_BYTES, nIdx_EthernetRecv) != ERROR_SUCCESS)
		return;
	if (pPdh->AddCounter(df_PDH_ETHERNETSEND_BYTES, nIdx_EthernetSend) != ERROR_SUCCESS)
		return;
	

	/// Performance Monitoring ///
	double dCpu, dMem, dNpmem, dERecv, dESend;
	double dMin, dMax, dMean;
	while (1)
	{
		/// Collect Data ///
		if (pPdh->CollectQueryData())
			continue;

		/// Update Counters ///
		if (!pPdh->GetCounterValue(nIdx_CpuUsage, &dCpu)) dCpu = 0;
		if (!pPdh->GetCounterValue(nIdx_MemAvail, &dMem)) dMem = 0;
		if (!pPdh->GetCounterValue(nIdx_NpMem, &dNpmem)) dNpmem = 0;
		if (!pPdh->GetCounterValue(nIdx_EthernetRecv, &dERecv)) dERecv = 0;
		if (!pPdh->GetCounterValue(nIdx_EthernetSend, &dESend)) dESend = 0;

		/// Get Statistics ///
		wprintf(" - HW CPU Usage 	: %.1f %%", dCpu);
		if (pPdh->GetStatistics(&dMin, &dMax, &dMean, nIdx_CpuUsage))
			wprintf(" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
		wprintf("\n");

		wprintf(" - HW Available Memory	: %.0f MB\n", dMem);

		wprintf(" - HW NonPaged Memory	: %.3f MB\n", dNpmem / 1024 / 1024);

		wprintf(" - Ethernet RecvBytes	: %.3f B/sec", dERecv);
		if (pPdh->GetStatistics(&dMin, &dMax, &dMean, nIdx_EthernetRecv))
			wprintf(" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
		wprintf("\n");

		wprintf(" - Ethernet SendBytes	: %.3f B/sec", dESend);
		if (pPdh->GetStatistics(&dMin, &dMax, &dMean, nIdx_EthernetSend))
			wprintf(" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
		wprintf("\n");


		Sleep(1000);
		system("cls");
	}

	pPdh->RemoveCounter(nIdx_CpuUsage);
	pPdh->RemoveCounter(nIdx_MemAvail);
	pPdh->RemoveCounter(nIdx_NpMem);
	pPdh->RemoveCounter(nIdx_EthernetRecv);
	pPdh->RemoveCounter(nIdx_EthernetSend);

	delete pPdh;

-------------------------------------------------------------------------------------------*/


#ifndef __PERFORMANCE_DATA_HELPER_H__
#define __PERFORMANCE_DATA_HELPER_H__
#include <Pdh.h>
#pragma comment(lib, "pdh.lib")
#include <vector>

/*********************************************************************************************
// PDH Browse Counters Define
//
//  PDH 쿼리문을 알아내기 위해 해당 소스 혹은 `성능 모니터` 이용
// https://docs.microsoft.com/en-us/windows/win32/perfctrs/browsing-performance-counters
**********************************************************************************************/
/// CPU ///
// * CPU 사용량은 OS에 따라 정상적으로 얻어지지 않는 경우도 있음 : 직접 사용시간을 구해서 사용하도록 함
#define df_PDH_CPUUSAGE_TOTAL "\\Processor(_Total)\\% Processor Time"	// CPU 전체 사용률(%)
#define df_PDH_CPUUSAGE_0 "\\Processor(0)\\% Processor Time"			// CPU 코어 사용률(%)
#define df_PDH_CPUUSAGE_1 "\\Processor(1)\\% Processor Time"
#define df_PDH_CPUUSAGE_2 "\\Processor(2)\\% Processor Time"
#define df_PDH_CPUUSAGE_3 "\\Processor(3)\\% Processor Time"
//#define df_PDH_CPUUSAGE_USER "\\Process(NAME)\\% User Time"			// 프로세스 CPU 유저 사용률(%)
//#define df_PDH_CPUUSAGE_USER "\\Process(NAME)\\% Processor Time"		// 프로세스 CPU 전체 사용률(%)

#define df_PDH_DISK_READ_TOTAL "\\PhysicalDisk(_Total)\\% Disk Read Time"
#define df_PDH_DISK_WRITE_TOTAL "\\PhysicalDisk(_Total)\\% Disk Write Time"

/// MEMORY ///
#define df_PDH_MEMINUSE_BYTES "\\Memory\\Committed Bytes" // 미리 할당된 메모리(Bytes)
#define df_PDH_MEMAVAIL_BYTES "\\Memory\\Available Bytes" // 사용가능 메모리(Bytes)
#define df_PDH_MEMAVAIL_KB "\\Memory\\Available KBytes" // 사용가능 메모리(Kilobytes)
#define df_PDH_MEMAVAIL_MB "\\Memory\\Available MBytes" // 사용가능 메모리(Megabytes)
#define df_PDH_MEMINUSE_PERCENT "\\Memory\\% Committed Bytes In Use" // 사용중 메모리(Committed Bytes/Commit Limit)(%)
#define df_PDH_MEMLIMIT_BYTES "\\Memory\\Commit Limit" // 미리 할당 한계 메모리(Bytes)
#define df_PDH_NONPAGEDMEM_BYTES "\\Memory\\Pool Nonpaged Bytes" // 논페이지 메모리(Bytes)
/// PROCESS ///
#define df_PDH_PROCESS_COUNT(PROCESS_NAME) "\\Process("#PROCESS_NAME")\\Thread Count" // 프로세스 스레드 수(n)
#define df_PDH_PROCESS_HANDLE_COUNT(PROCESS_NAME) "\\Process("#PROCESS_NAME")\\Handle Count" // 프로세스 핸들 수(n)
#define df_PDH_PROCESS_VRMEM_BYTES(PROCESS_NAME) "\\Process("#PROCESS_NAME")\\Virtual Bytes" // 프로세스 가상 메모리(Bytes)
// * 가상 메모리 = 예약된 메모리(Reserved) + 경계(boundary, 65KB). * 실제 사용 메모리(commit)과는 무관
#define df_PDH_PROCESS_USERMEM_BYTES(PROCESS_NAME) "\\Process("#PROCESS_NAME")\\Private Bytes" // 프로세스 유저할당 메모리(Bytes)
// * 프로세스 공용 메모리/커널 메모리 제외
#define df_PDH_PROCESS_WORKMEM(PROCESS_NAME) "\\Process("#PROCESS_NAME")\\Working Set" // 프로세스 작업 메모리(Bytes?)
// * 현재 물리 메모리에 사용되는 크기. 실제 할당 용량은 아닐 수 있음
#define df_PDH_PROCESS_NPMEM(PROCESS_NAME) "\\Process("#PROCESS_NAME")\\Pool Nonpaged Bytes" // 프로세스 논페이지 메모리(Bytes)
/// ETHERNET ///
#define df_PDH_ETHERNETRECV_BYTES "\\Network Interface(*)\\Bytes Received/sec" // 이더넷 수신량(Bytes/sec)
#define df_PDH_ETHERNETSEND_BYTES "\\Network Interface(*)\\Bytes Sent/sec" // 이더넷 송신량(Bytes/sec)


#define df_MAX_RAW 20

struct stPDHCOUNTER 
{
	stPDHCOUNTER()
	{
		dValue = nNextIdx = nOldestIdx = nRawCount = 0;
	}

	int nIdx;
	
	double dValue;

	HCOUNTER hCounter;	// 카운터 핸들

	/// 통계용 링버퍼 ///
	PDH_RAW_COUNTER RingBuffer[df_MAX_RAW];
	int nNextIdx;		// 다음 저장 원시값 인덱스
	int nOldestIdx;		// 가장 오래된 원시값 인덱스
	int nRawCount;		// 저장된 원시값 총 개수
};

class CPDH
{
public:
	CPDH();
	virtual ~CPDH();

	BOOL Init();
	void Clean();

	/// COUNTERS ///
	////////////////////////////////////////////////////////////
	// 카운터 추가
	//  쿼리 카운터 추가
	//
	// Parameters : (char*) 쿼리(CPdh.h 내 define값 참고)
	//				(int&) 성공시 카운터 인덱스(idx)값, 실패시 -1
	// Return :		(LONG) 성공시 0, 실패시 에러값
	////////////////////////////////////////////////////////////
	LONG AddCounter(const char* pszPdhDefine, int& iOutIdx);

	////////////////////////////////////////////////////////////
	// 카운터 제거
	//  특정 인덱스의 카운터 제거
	//
	// Parameters : (int) 삭제할 카운터의 인덱스 번호
	// Return :		(LONG) 성공시 0, 실패시 에러값이나 -1
	////////////////////////////////////////////////////////////
	LONG RemoveCounter(int nIdx);


	/// DATA ///
	////////////////////////////////////////////////////////////
	// 카운터 일괄 갱신
	//  해당 클래스의 쿼리 안에 있는 모든 카운터 갱신
	//
	// Parameters :
	// Return :		(LONG) 성공시 0, 실패시 에러값
	////////////////////////////////////////////////////////////
	LONG CollectQueryData();

	////////////////////////////////////////////////////////////
	// 카운터 통계
	//
	// Parameters : (double*) 최소
	//				(double*) 최대
	//				(double*) 평균
	//				(int) 계산할 카운터 인덱스값
	// Return :		(BOOL) 성공 여부
	////////////////////////////////////////////////////////////
	BOOL GetStatistics(double* nMin, double* nMax, double* nMean, int nIdx);

	////////////////////////////////////////////////////////////
	// 특정 카운터 값
	//
	// Parameters : (int) 값을 얻을 카운터 인덱스값
	//				(double*)
	// Return :		(BOOL) 성공 여부
	////////////////////////////////////////////////////////////
	BOOL GetCounterValue(int nIdx, double* dValue);

protected:
	/// COUNTERS ///
	////////////////////////////////////////////////////////////
	// 카운터 찾기
	//  쿼리 내 해당 인덱스를 가진 카운터 찾기
	//
	// Parameters : (int) 찾을 카운터의 인덱스
	// Return :		(stPDHCOUNTER*) 찾은 카운터의 포인터
	////////////////////////////////////////////////////////////
	stPDHCOUNTER* FindPdhCounter(int nIdx);


	/// COUNTER VALUES ///
	////////////////////////////////////////////////////////////
	// 특정 카운터 갱신(double)
	//  카운터값을 double형으로 갱신하고 출력
	//
	// Parameters : (stPDHCOUNTER*) 카운터값을 갱신할 구조체 포인터
	// Return :		(LONG) 성공시 0, 실패시 에러값
	////////////////////////////////////////////////////////////
	LONG UpdateValue(stPDHCOUNTER* pCounter);

	////////////////////////////////////////////////////////////
	// 특정 카운터 갱신 (원본값)
	//  카운터값을 갱신하고 출력
	//
	// Parameters : (stPDHCOUNTER*) 카운터값을 갱신할 구조체 포인터
	// Return :		(LONG) 성공시 0, 실패시 에러값
	////////////////////////////////////////////////////////////
	LONG UpdateRawValue(stPDHCOUNTER* pCounter);

private:
	int		_nIdx;
	HQUERY	_hQuery;
	std::vector<stPDHCOUNTER*> _vPerfData;
	
};

#endif // !__PERFORMANCE_DATA_HELPER_H__
