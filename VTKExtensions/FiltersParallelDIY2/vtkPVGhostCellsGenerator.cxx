// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkPVGhostCellsGenerator.h"

#include "vtkConvertToPartitionedDataSetCollection.h"
#include "vtkDataAssembly.h"
#include "vtkDataObjectTree.h"
#include "vtkDataObjectTreeIterator.h"
#include "vtkDataSet.h"
#include "vtkDemandDrivenPipeline.h"
#include "vtkHyperTreeGrid.h"
#include "vtkHyperTreeGridGhostCellsGenerator.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPartitionedDataSetCollection.h"

vtkStandardNewMacro(vtkPVGhostCellsGenerator);

//----------------------------------------------------------------------------
void vtkPVGhostCellsGenerator::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
int vtkPVGhostCellsGenerator::GhostCellsGeneratorUsingSuperclassInstance(
  vtkInformation*, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkNew<Superclass> instance;

  instance->SetBuildIfRequired(this->GetBuildIfRequired());
  instance->SetController(this->GetController());
  instance->SetNumberOfGhostLayers(this->GetNumberOfGhostLayers());

  vtkDataObject* inputDO = vtkDataObject::GetData(inputVector[0], 0);
  vtkDataObject* outputDO = vtkDataObject::GetData(outputVector, 0);

  instance->SetInputDataObject(inputDO);
  if (instance->GetExecutive()->Update())
  {
    outputDO->ShallowCopy(instance->GetOutput());
    return 1;
  }

  return 0;
}

int vtkPVGhostCellsGenerator::RequestDataObject(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  this->HasCompositeHTG = false;

  vtkDataObjectTree* inputComposite = vtkDataObjectTree::GetData(inInfo);
  if (inputComposite)
  {
    vtkSmartPointer<vtkDataObjectTreeIterator> iter;
    iter.TakeReference(inputComposite->NewTreeIterator());
    iter->VisitOnlyLeavesOn();
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      auto pds = vtkPartitionedDataSet::SafeDownCast(iter->GetCurrentDataObject());
      if (vtkHyperTreeGrid::SafeDownCast(iter->GetCurrentDataObject()) ||
        (pds && pds->GetNumberOfPartitions() > 0 &&
          vtkHyperTreeGrid::SafeDownCast(pds->GetPartitionAsDataObject(0))))
      {
        bool outputAlreadyCreated = vtkPartitionedDataSetCollection::GetData(outInfo);
        if (!outputAlreadyCreated)
        {
          vtkNew<vtkPartitionedDataSetCollection> outputPDC;
          this->GetExecutive()->SetOutputData(0, outputPDC);
          this->GetOutputPortInformation(0)->Set(
            vtkDataObject::DATA_EXTENT_TYPE(), outputPDC->GetExtentType());
          this->HasCompositeHTG = true;
        }
        return 1;
      }
    }
  }

  return Superclass::RequestDataObject(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkPVGhostCellsGenerator::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  vtkDataObject* input = vtkDataObject::GetData(inInfo);
  vtkDataObject* output = vtkDataObject::GetData(outInfo);

  if (!input)
  {
    return 1;
  }
  if (!output)
  {
    vtkErrorMacro(<< "Failed to get output data object.");
  }

  if (vtkHyperTreeGrid::SafeDownCast(input))
  {
    // Match behavior from vtkGhostCellsGenerator
    vtkNew<vtkHyperTreeGridGhostCellsGenerator> ghostCellsGenerator;
    ghostCellsGenerator->SetInputData(input);
    ghostCellsGenerator->Update();

    if (ghostCellsGenerator->GetExecutive()->Update())
    {
      output->ShallowCopy(ghostCellsGenerator->GetOutput(0));
      return 1;
    }

    return 0;
  }

  // Check if our composite input contains HTG.
  // If it does, dispatch the blocks to the right implementation of the filter.
  vtkDataObjectTree* inputComposite = vtkDataObjectTree::GetData(inInfo);
  if (inputComposite && this->HasCompositeHTG)
  {
    vtkPartitionedDataSetCollection* outputPDC = vtkPartitionedDataSetCollection::GetData(outInfo);
    if (!outputPDC)
    {
      vtkErrorMacro(<< "Unable to retrieve output as vtkPartitionedDataSetCollection.");
      return 0;
    }

    // Convert the input structure to PDC if necessary
    vtkNew<vtkConvertToPartitionedDataSetCollection> converter;
    converter->SetInputDataObject(inputComposite);
    converter->Update();
    vtkPartitionedDataSetCollection* inputPDC = converter->GetOutput();

    if (!inputPDC)
    {
      vtkErrorMacro(<< "Unable to convert composite input to Partitioned DataSet Collection");
      return 0;
    }

    outputPDC->Initialize();
    vtkDataAssembly* inputHierarchy = inputPDC->GetDataAssembly();
    if (inputHierarchy)
    {
      vtkNew<vtkDataAssembly> outputHierarchy;
      outputHierarchy->DeepCopy(inputHierarchy);
      outputPDC->SetDataAssembly(outputHierarchy);
    }

    outputPDC->SetNumberOfPartitionedDataSets(inputPDC->GetNumberOfPartitionedDataSets());

    // Dispatch individual partitioned datasets from the input PDC
    for (unsigned int pdsIdx = 0; pdsIdx < inputPDC->GetNumberOfPartitionedDataSets(); pdsIdx++)
    {
      auto currentPDS = inputPDC->GetPartitionedDataSet(pdsIdx);
      if (vtkHyperTreeGrid::SafeDownCast(currentPDS->GetPartitionAsDataObject(0)))
      {
        // HTG GCG cannot handle PDS input, so we apply the filter to each individual partition
        vtkNew<vtkPartitionedDataSet> outputPDS;
        outputPDS->SetNumberOfPartitions(currentPDS->GetNumberOfPartitions());
        for (unsigned int partId = 0; partId < currentPDS->GetNumberOfPartitions(); partId++)
        {
          vtkNew<vtkHyperTreeGridGhostCellsGenerator> ghostCellsGenerator;
          ghostCellsGenerator->SetInputData(currentPDS->GetPartitionAsDataObject(partId));
          ghostCellsGenerator->Update();

          outputPDS->SetPartition(partId, ghostCellsGenerator->GetOutput(0));
        }
        outputPDC->SetPartitionedDataSet(pdsIdx, outputPDS);
      }
      else
      {
        // Forward the whole PDS to non-HTG GCG.
        vtkNew<vtkGhostCellsGenerator> ghostCellsGenerator;
        ghostCellsGenerator->SetInputData(currentPDS);
        ghostCellsGenerator->Update();
        auto outputPDS = vtkPartitionedDataSet::SafeDownCast(ghostCellsGenerator->GetOutput(0));
        if (!outputPDS)
        {
          vtkErrorMacro(<< "Unable to retrieve output vtDataAssembly dataset " << pdsIdx
                        << " as vtkPartitionedDataSet.");
          return 0;
        }

        outputPDC->SetPartitionedDataSet(pdsIdx, outputPDS);
      }
    }
    return 1;
  }

  return this->GhostCellsGeneratorUsingSuperclassInstance(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkPVGhostCellsGenerator::FillInputPortInformation(int port, vtkInformation* info)
{
  this->Superclass::FillInputPortInformation(port, info);
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkHyperTreeGrid");
  return 1;
}
