#include "CPdh.h"

CPDH::CPDH()
{
	_nIdx = 0;
	Init();
}

CPDH::~CPDH()
{
	Clean();
}

BOOL CPDH::Init()
{
	if (PdhOpenQuery(NULL, 1, &_hQuery) != ERROR_SUCCESS)
		return false;

	return true;
}

void CPDH::Clean()
{
	PdhCloseQuery(&_hQuery);
	for (auto iter = _vPerfData.begin(); iter != _vPerfData.end();)
	{	
		delete *iter;
		iter = _vPerfData.erase(iter);
	}
}

LONG CPDH::AddCounter(const char *pszPdhDefine, int& iOutIdx)
{
	stPDHCOUNTER* pCounter = new stPDHCOUNTER();
	LONG lRetval = PdhAddCounter(_hQuery, pszPdhDefine, (DWORD_PTR)pCounter, &(pCounter->hCounter));
	if(lRetval != ERROR_SUCCESS)
	{
		delete pCounter;
		return lRetval;
	}

	pCounter->nIdx = _nIdx++;
	_vPerfData.push_back(pCounter);

	iOutIdx = pCounter->nIdx;

	return lRetval;
}

LONG CPDH::RemoveCounter(int nIdx)
{
	stPDHCOUNTER* pCounter = FindPdhCounter(nIdx);
	if (pCounter == nullptr) 
		return -1;
	
	return PdhRemoveCounter(pCounter->hCounter);
}

LONG CPDH::CollectQueryData()
{
	return PdhCollectQueryData(_hQuery);
}

BOOL CPDH::GetStatistics(double * nMin, double * nMax, double * nMean, int nIdx)
{
	PDH_STATISTICS pdhStats;
	stPDHCOUNTER* pCounter = FindPdhCounter(nIdx);
	if (pCounter == nullptr)
		return false;

	//PDH_FUNCTION PdhComputeCounterStatistics(
	//	PDH_HCOUNTER     hCounter,			// 통계할 카운터 핸들
	//	DWORD            dwFormat,			// 데이터 타입
	//	DWORD            dwFirstEntry,		// 원시 카운터 버퍼의 첫번째 인덱스 
	//	DWORD            dwNumEntries,		// 원시 카운터 값의 총 개수
	//	PPDH_RAW_COUNTER lpRawValueArray,	// 원시 카운터 버퍼
	//	PPDH_STATISTICS  data				// 통계를 수신할 구조체
	//);
	if (PdhComputeCounterStatistics(pCounter->hCounter, PDH_FMT_DOUBLE, pCounter->nOldestIdx, pCounter->nRawCount, pCounter->RingBuffer, &pdhStats) != ERROR_SUCCESS)
		return false;

	if (pdhStats.min.CStatus == ERROR_SUCCESS)
		*nMin = pdhStats.min.doubleValue;

	if (pdhStats.max.CStatus == ERROR_SUCCESS)
		*nMax = pdhStats.max.doubleValue;

	if (pdhStats.mean.CStatus == ERROR_SUCCESS)
		*nMean = pdhStats.mean.doubleValue;

	return true;
}

BOOL CPDH::GetCounterValue(int nIdx, double * dValue)
{
	stPDHCOUNTER* pCounter = FindPdhCounter(nIdx);
	if (pCounter == nullptr)
		return false;

	if (UpdateValue(pCounter) != ERROR_SUCCESS)
		return false;

	if (UpdateRawValue(pCounter) != ERROR_SUCCESS)
		return false;

	*dValue = pCounter->dValue;
	return true;
}

stPDHCOUNTER* CPDH::FindPdhCounter(int nIdx)
{
	for (auto iter = _vPerfData.begin(); iter != _vPerfData.end(); ++iter)
	{
		if ((*iter)->nIdx == nIdx)
			return *iter;
	}

	return nullptr;
}

LONG CPDH::UpdateValue(stPDHCOUNTER * pCounter)
{
	PDH_FMT_COUNTERVALUE pdhFormattedValue;
	LONG lErr = PdhGetFormattedCounterValue(pCounter->hCounter, PDH_FMT_DOUBLE, NULL, &pdhFormattedValue);
	if (lErr != ERROR_SUCCESS)
		return lErr;

	pCounter->dValue = pdhFormattedValue.doubleValue;

	return lErr;
}

LONG CPDH::UpdateRawValue(stPDHCOUNTER* pCounter)
{
	PPDH_RAW_COUNTER pPdhRawCounter = &pCounter->RingBuffer[pCounter->nNextIdx];
	LONG lErr = PdhGetRawCounterValue(pCounter->hCounter, NULL, pPdhRawCounter);
	if (lErr != ERROR_SUCCESS)
		return lErr;

	// * 로직 시간 단축을 위해 간단한 링버퍼 사용
	// 최소, 최대 및 평균을 계산할 카운터값을 최대 df_MAX_RAW개 저장 
	pCounter->nRawCount = min(pCounter->nRawCount + 1, df_MAX_RAW);
	pCounter->nNextIdx = (pCounter->nNextIdx + 1) % df_MAX_RAW;
	if (pCounter->nRawCount >= df_MAX_RAW)
		pCounter->nOldestIdx = pCounter->nNextIdx;

	return lErr;
}