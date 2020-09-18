# PDH 클래스
## 📢 개요
 PDH(Performance Data Helper) 라이브러리는 윈도우 환경에서 성능 카운터 정보를 알아내서 메모리, CPU, 캐시, 다중 CPU 환경 등에서 발생할 수 있는 각종 병목 현상을 모니터링 할 수 있게 해준다. 윈도우의 프로그램 중 성능 모니터(Perfmon)가 이 라이브러리를 바탕으로 만들어졌다. 
 
 서버처럼 장시간 가동해야만 하는 프로그램의 경우 예측하기 어려운 원인으로 인해 중단되는 등 치명적인 문제가 발생할 수 있다. PDH 라이브러리를 통해 주기적으로 성능 정보를 기록해뒀다면 이러한 문제가 발생했을 때 원인을 추적할 수도 있다.

## 💻 Perfmon
  서버에서 계속 주시해야 할 주요 항목들을 1초 주기로 모니터링하는 예제

  ![capture](https://github.com/kbm0996/-Utility-CPDH/blob/master/PDH/jpg/test.JPG)
  
  **figure 1. Program*
 
 
## 🅿 사용법 및 예제 코드

```cpp
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
     wprintf(L" - HW CPU Usage 	: %.1f %%", dCpu);
     if (pPdh->GetStatistics(&dMin, &dMax, &dMean, nIdx_CpuUsage))
      wprintf(L" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
     wprintf(L"\n");
     wprintf(L" - HW Available Memory	: %.0f MB\n", dMem);
     wprintf(L" - HW NonPaged Memory	: %.3f MB\n", dNpmem / 1024 / 1024);
     wprintf(L" - Ethernet RecvBytes	: %.3f B/sec", dERecv);
     if (pPdh->GetStatistics(&dMin, &dMax, &dMean, nIdx_EthernetRecv))
      wprintf(L" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
     wprintf(L"\n");
     wprintf(L" - Ethernet SendBytes	: %.3f B/sec", dESend);
     if (pPdh->GetStatistics(&dMin, &dMax, &dMean, nIdx_EthernetSend))
      wprintf(L" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
     wprintf(L"\n");
     
     Sleep(1000);
     system("cls");
    }
    
    pPdh->RemoveCounter(nIdx_CpuUsage);
    pPdh->RemoveCounter(nIdx_MemAvail);
    pPdh->RemoveCounter(nIdx_NpMem);
    pPdh->RemoveCounter(nIdx_EthernetRecv);
    pPdh->RemoveCounter(nIdx_EthernetSend);
    delete pPdh;
