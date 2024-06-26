// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkSMCSVExporterProxy.h"

#include "vtkCSVExporter.h"
#include "vtkObjectFactory.h"
#include "vtkPVXYChartView.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMViewProxy.h"
#include "vtkSpreadSheetView.h"

#include <string>
#include <vtksys/SystemTools.hxx>

vtkStandardNewMacro(vtkSMCSVExporterProxy);
//----------------------------------------------------------------------------
vtkSMCSVExporterProxy::vtkSMCSVExporterProxy() = default;

//----------------------------------------------------------------------------
vtkSMCSVExporterProxy::~vtkSMCSVExporterProxy() = default;

//----------------------------------------------------------------------------
bool vtkSMCSVExporterProxy::CanExport(vtkSMProxy* proxy)
{
  if (proxy)
  {
    vtkObjectBase* obj = proxy->GetClientSideObject();
    if (vtkSpreadSheetView::SafeDownCast(obj) || vtkPVXYChartView::SafeDownCast(obj))
    {
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkSMCSVExporterProxy::Write()
{
  this->CreateVTKObjects();

  vtkCSVExporter* exporter = vtkCSVExporter::SafeDownCast(this->GetClientSideObject());
  if (!exporter)
  {
    vtkErrorMacro("No vtkCSVExporter.");
    return;
  }

  std::string fileName = vtkSMPropertyHelper(this, "FileName").GetAsString();
  if (fileName.empty())
  {
    return;
  }
  if (vtksys::SystemTools::GetFilenameLastExtension(fileName) == ".tsv")
  {
    exporter->SetFieldDelimiter("\t");
  }
  vtkObjectBase* obj = this->View->GetClientSideObject();
  if (vtkSpreadSheetView* sview = vtkSpreadSheetView::SafeDownCast(obj))
  {
    sview->Export(exporter);
  }
  else if (vtkPVContextView* cview = vtkPVContextView::SafeDownCast(obj))
  {
    /// Note, in CanExport() for now, we're only supporting exporting for
    /// XYChartViews.
    cview->Export(vtkAbstractChartExporter::SafeDownCast(exporter));
  }
}

//----------------------------------------------------------------------------
void vtkSMCSVExporterProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
