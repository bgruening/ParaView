// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkMaterialInterfacePieceLoading.h"
#include "vtkMaterialInterfaceUtilities.h"
#include <algorithm>
#include <vector>
using std::cerr;
using std::endl;
using std::ostream;
using std::vector;

//
ostream& operator<<(ostream& sout, const vtkMaterialInterfacePieceLoading& fp)
{
  sout << "(" << fp.GetId() << "," << fp.GetLoading() << ")";

  return sout;
}
//
void PrintPieceLoadingHistogram(vector<vector<vtkIdType>>& pla)
{
  // cerr << "loading array:" <<endl;
  // cerr << pla << endl;
  size_t nProcs = pla.size();
  // get min and max loading
  vtkIdType minLoading = (vtkIdType)1 << ((sizeof(vtkIdType) * 8) - 2);
  vtkIdType maxLoading = 0;
  for (size_t procId = 0; procId < nProcs; ++procId)
  {
    size_t nPieces = pla[procId].size();
    for (size_t pieceId = 0; pieceId < nPieces; ++pieceId)
    {
      vtkIdType loading = pla[procId][pieceId];
      if (loading > 0 && minLoading > loading)
      {
        minLoading = loading;
      }
      maxLoading = std::max(maxLoading, loading);
    }
  }
  // generate histogram
  const vtkIdType nBins = 40;
  const vtkIdType binWidth = (maxLoading - minLoading) / nBins;
  const vtkIdType r = (maxLoading - minLoading) % nBins;
  vector<int> hist(nBins, 0);
  for (size_t procId = 0; procId < nProcs; ++procId)
  {
    size_t nPieces = pla[procId].size();
    for (size_t pieceId = 0; pieceId < nPieces; ++pieceId)
    {
      vtkIdType loading = pla[procId][pieceId];
      if (loading == 0)
      {
        continue;
      }
      for (int binId = 0; binId < nBins; ++binId)
      {
        vtkIdType binTop = minLoading + (binId + 1) * binWidth + binId * r;
        if (loading <= binTop)
        {
          ++hist[binId];
          break;
        }
      }
    }
  }
  // generate bin ids
  vector<vtkIdType> binIds(nBins);
  for (int binId = 0; binId < nBins; ++binId)
  {
    binIds[binId] = static_cast<int>(minLoading + (binId + 1) * binWidth);
  }
  // print
  cerr << "minLoading: " << minLoading << endl;
  cerr << "maxLoading: " << maxLoading << endl;
  cerr << "binWidth:   " << binWidth << endl;
  cerr << "nBins:      " << nBins << endl;
  PrintHistogram(hist, binIds);
}
