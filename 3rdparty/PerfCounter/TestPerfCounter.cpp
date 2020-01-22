#include "stdafx.h"
#include "PerfCounter.h"

int _tmain() noexcept
{
  //Test out the methods of the class
  
  CPerformanceCounter ctr;
  ctr.Start();
  Sleep(1000);
  ctr.Stop();

  _tprintf(_T("Frequency of the counter is %I64d Hertz\n"), ctr.Frequency());
  const LONGLONG lDiff = ctr.Difference();
  _tprintf(_T("Time taken to Sleep(1000): %I64d nanoseconds\n"), ctr.AsNanoSeconds(lDiff));
  _tprintf(_T("Time taken to Sleep(1000): %I64d microseconds\n"), ctr.AsMicroSeconds(lDiff));
  _tprintf(_T("Time taken to Sleep(1000): %I64d milliseconds\n"), ctr.AsMilliSeconds(lDiff));
  _tprintf(_T("Time taken to Sleep(1000): %f seconds\n"), ctr.AsSeconds(lDiff));

  return 0;
}

