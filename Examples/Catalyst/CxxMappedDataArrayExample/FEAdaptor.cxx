// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "FEAdaptor.h"
#include "FEDataStructures.h"

#include <vtkAOSDataArrayTemplate.h>
#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonPipeline.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkLogger.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

#include <iostream>

namespace
{
vtkCPProcessor* Processor = nullptr;
vtkUnstructuredGrid* VTKGrid;

void BuildVTKGrid(Grid& grid)
{
  // create the points information
  vtkNew<vtkAOSDataArrayTemplate<double>> pointArray;
  pointArray->SetNumberOfComponents(3);
  pointArray->SetArray(
    grid.GetPointsArray(), 3 * static_cast<vtkIdType>(grid.GetNumberOfPoints()), /*save=*/1);
  vtkNew<vtkPoints> points;
  points->SetData(pointArray);
  VTKGrid->SetPoints(points);

  // create the cells
  size_t numCells = grid.GetNumberOfCells();
  VTKGrid->Allocate(static_cast<vtkIdType>(numCells * 9));
  for (size_t cell = 0; cell < numCells; cell++)
  {
    unsigned int* cellPoints = grid.GetCellPoints(cell);
    vtkIdType tmp[8] = { cellPoints[0], cellPoints[1], cellPoints[2], cellPoints[3], cellPoints[4],
      cellPoints[5], cellPoints[6], cellPoints[7] };
    VTKGrid->InsertNextCell(VTK_HEXAHEDRON, 8, tmp);
  }
}

void UpdateVTKAttributes(Grid& grid, Attributes& attributes, vtkCPInputDataDescription* idd)
{
  if (idd->IsFieldNeeded("velocity", vtkDataObject::POINT))
  {
    if (VTKGrid->GetPointData()->GetNumberOfArrays() == 0)
    {
      // velocity array
      vtkNew<vtkAOSDataArrayTemplate<double>> velocity;
      velocity->SetName("velocity");
      velocity->SetNumberOfComponents(3);
      VTKGrid->GetPointData()->AddArray(velocity);
    }
    auto velocity =
      vtkAOSDataArrayTemplate<double>::SafeDownCast(VTKGrid->GetPointData()->GetArray("velocity"));
    velocity->SetArray(attributes.GetVelocityArray(), 3 * VTKGrid->GetNumberOfPoints(), /*save=*/1);
  }
  if (idd->IsFieldNeeded("pressure", vtkDataObject::CELL))
  {
    if (VTKGrid->GetCellData()->GetNumberOfArrays() == 0)
    {
      // pressure array
      vtkNew<vtkAOSDataArrayTemplate<float>> pressure;
      pressure->SetName("pressure");
      pressure->SetNumberOfComponents(1);
      VTKGrid->GetCellData()->AddArray(pressure);
    }
    auto pressure =
      vtkAOSDataArrayTemplate<float>::SafeDownCast(VTKGrid->GetCellData()->GetArray("pressure"));
    // The pressure array is a scalar array so we can reuse
    // memory as long as we ordered the points properly.
    pressure->SetArray(attributes.GetPressureArray(), VTKGrid->GetNumberOfCells(), /*save=*/1);
  }
}

void BuildVTKDataStructures(Grid& grid, Attributes& attributes, vtkCPInputDataDescription* idd)
{
  if (VTKGrid == nullptr)
  {
    // The grid structure isn't changing so we only build it
    // the first time it's needed. If we needed the memory
    // we could delete it and rebuild as necessary.
    VTKGrid = vtkUnstructuredGrid::New();
    BuildVTKGrid(grid);
  }
  UpdateVTKAttributes(grid, attributes, idd);
}
}

namespace FEAdaptor
{

void Initialize(int numScripts, char* scripts[])
{
  if (Processor == nullptr)
  {
    Processor = vtkCPProcessor::New();
    Processor->Initialize();
  }
  else
  {
    Processor->RemoveAllPipelines();
  }
  for (int i = 0; i < numScripts; i++)
  {
    if (auto pipeline = vtkCPPythonPipeline::CreateAndInitializePipeline(scripts[i]))
    {
      Processor->AddPipeline(pipeline);
    }
    else
    {
      vtkLogF(ERROR, "failed to setup pipeline for '%s'", scripts[i]);
    }
  }
}

void Finalize()
{
  if (Processor)
  {
    Processor->Delete();
    Processor = nullptr;
  }
  if (VTKGrid)
  {
    VTKGrid->Delete();
    VTKGrid = nullptr;
  }
}

void CoProcess(
  Grid& grid, Attributes& attributes, double time, unsigned int timeStep, bool lastTimeStep)
{
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);
  if (lastTimeStep == true)
  {
    // assume that we want to all the pipelines to execute if it
    // is the last time step.
    dataDescription->ForceOutputOn();
  }
  if (Processor->RequestDataDescription(dataDescription) != 0)
  {
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructures(grid, attributes, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription);
  }
}
} // end of Catalyst namespace
