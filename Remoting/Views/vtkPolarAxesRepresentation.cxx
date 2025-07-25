// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkPolarAxesRepresentation.h"

#include "vtkAlgorithmOutput.h"
#include "vtkAxisActor.h"
#include "vtkBoundingBox.h"
#include "vtkCommand.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataSet.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkFloatArray.h"
#include "vtkHyperTreeGrid.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineSource.h"
#include "vtkPVChangeOfBasisHelper.h"
#include "vtkPVRenderView.h"
#include "vtkPolarAxesActor.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkStringArray.h"
#include "vtkTextProperty.h"
#include "vtkTransform.h"

vtkStandardNewMacro(vtkPolarAxesRepresentation);
//----------------------------------------------------------------------------
vtkPolarAxesRepresentation::vtkPolarAxesRepresentation()
{
  this->PolarAxesActor->PickableOff();
  vtkMath::UninitializeBounds(this->DataBounds);
}

//----------------------------------------------------------------------------
vtkPolarAxesRepresentation::~vtkPolarAxesRepresentation() = default;

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetVisibility(bool val)
{
  if (this->GetVisibility() != val)
  {
    this->Superclass::SetVisibility(val);
    this->PolarAxesActor->SetVisibility((this->ParentVisibility && val) ? 1 : 0);
    this->Modified();
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetParentVisibility(bool val)
{
  if (this->ParentVisibility != val)
  {
    this->ParentVisibility = val;
    this->PolarAxesActor->SetVisibility((val && this->GetVisibility()) ? 1 : 0);
    this->Modified();
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetEnableOverallColor(bool enable)
{
  if (this->EnableOverallColor != enable)
  {
    this->EnableOverallColor = enable;
    this->Modified();

    if (this->EnableOverallColor)
    {
      this->PolarAxesActor->GetLastRadialAxisProperty()->SetColor(
        this->OverallColor[0], this->OverallColor[1], this->OverallColor[2]);
      this->PolarAxesActor->GetSecondaryRadialAxesProperty()->SetColor(
        this->OverallColor[0], this->OverallColor[1], this->OverallColor[2]);
      this->PolarAxesActor->GetPolarArcsProperty()->SetColor(
        this->OverallColor[0], this->OverallColor[1], this->OverallColor[2]);
      this->PolarAxesActor->GetSecondaryPolarArcsProperty()->SetColor(
        this->OverallColor[0], this->OverallColor[1], this->OverallColor[2]);
      this->PolarAxesActor->GetPolarAxisProperty()->SetColor(
        this->OverallColor[0], this->OverallColor[1], this->OverallColor[2]);
    }
    else
    {
      this->PolarAxesActor->GetLastRadialAxisProperty()->SetColor(
        this->LastRadialAxisColor[0], this->LastRadialAxisColor[1], this->LastRadialAxisColor[2]);
      this->PolarAxesActor->GetSecondaryRadialAxesProperty()->SetColor(
        this->SecondaryRadialAxesColor[0], this->SecondaryRadialAxesColor[1],
        this->SecondaryRadialAxesColor[2]);
      this->PolarAxesActor->GetPolarArcsProperty()->SetColor(
        this->PolarArcsColor[0], this->PolarArcsColor[1], this->PolarArcsColor[2]);
      this->PolarAxesActor->GetSecondaryPolarArcsProperty()->SetColor(
        this->SecondaryPolarArcsColor[0], this->SecondaryPolarArcsColor[1],
        this->SecondaryPolarArcsColor[2]);
      this->PolarAxesActor->GetPolarAxisProperty()->SetColor(
        this->PolarAxisColor[0], this->PolarAxisColor[1], this->PolarAxisColor[2]);
    }
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetOverallColor(double r, double g, double b)
{
  if (this->EnableOverallColor &&
    (this->OverallColor[0] != r || this->OverallColor[1] != g || this->OverallColor[2] != b))
  {
    this->OverallColor[0] = r;
    this->OverallColor[1] = g;
    this->OverallColor[2] = b;
    this->Modified();

    this->PolarAxesActor->GetLastRadialAxisProperty()->SetColor(r, g, b);
    this->PolarAxesActor->GetSecondaryRadialAxesProperty()->SetColor(r, g, b);
    this->PolarAxesActor->GetPolarArcsProperty()->SetColor(r, g, b);
    this->PolarAxesActor->GetSecondaryPolarArcsProperty()->SetColor(r, g, b);
    this->PolarAxesActor->GetPolarAxisProperty()->SetColor(r, g, b);
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLastRadialAxisColor(double r, double g, double b)
{
  if (!this->EnableOverallColor &&
    (this->LastRadialAxisColor[0] != r || this->LastRadialAxisColor[1] != g ||
      this->LastRadialAxisColor[2] != b))
  {
    this->LastRadialAxisColor[0] = r;
    this->LastRadialAxisColor[1] = g;
    this->LastRadialAxisColor[2] = b;
    this->Modified();

    this->PolarAxesActor->GetLastRadialAxisProperty()->SetColor(r, g, b);
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetSecondaryRadialAxesColor(double r, double g, double b)
{
  if (!this->EnableOverallColor &&
    (this->SecondaryRadialAxesColor[0] != r || this->SecondaryRadialAxesColor[1] != g ||
      this->SecondaryRadialAxesColor[2] != b))
  {
    this->SecondaryRadialAxesColor[0] = r;
    this->SecondaryRadialAxesColor[1] = g;
    this->SecondaryRadialAxesColor[2] = b;
    this->Modified();

    this->PolarAxesActor->GetSecondaryRadialAxesProperty()->SetColor(r, g, b);
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarArcsColor(double r, double g, double b)
{
  if (!this->EnableOverallColor &&
    (this->PolarArcsColor[0] != r || this->PolarArcsColor[1] != g || this->PolarArcsColor[2] != b))
  {
    this->PolarArcsColor[0] = r;
    this->PolarArcsColor[1] = g;
    this->PolarArcsColor[2] = b;
    this->Modified();

    this->PolarAxesActor->GetPolarArcsProperty()->SetColor(r, g, b);
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetSecondaryPolarArcsColor(double r, double g, double b)
{
  if (!this->EnableOverallColor &&
    (this->SecondaryPolarArcsColor[0] != r || this->SecondaryPolarArcsColor[1] != g ||
      this->SecondaryPolarArcsColor[2] != b))
  {
    this->SecondaryPolarArcsColor[0] = r;
    this->SecondaryPolarArcsColor[1] = g;
    this->SecondaryPolarArcsColor[2] = b;
    this->Modified();

    this->PolarAxesActor->GetSecondaryPolarArcsProperty()->SetColor(r, g, b);
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisColor(double r, double g, double b)
{
  if (!this->EnableOverallColor &&
    (this->PolarAxisColor[0] != r || this->PolarAxisColor[1] != g || this->PolarAxisColor[2] != b))
  {
    this->PolarAxisColor[0] = r;
    this->PolarAxisColor[1] = g;
    this->PolarAxisColor[2] = b;
    this->Modified();

    this->PolarAxesActor->GetPolarAxisProperty()->SetColor(r, g, b);
  }
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisTitleTextProperty(vtkTextProperty* prop)
{
  this->PolarAxesActor->SetPolarAxisTitleTextProperty(prop);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisLabelTextProperty(vtkTextProperty* prop)
{
  this->PolarAxesActor->SetPolarAxisLabelTextProperty(prop);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLastRadialAxisTextProperty(vtkTextProperty* prop)
{
  this->PolarAxesActor->SetLastRadialAxisTextProperty(prop);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetSecondaryRadialAxesTextProperty(vtkTextProperty* prop)
{
  this->PolarAxesActor->SetSecondaryRadialAxesTextProperty(prop);
}

//----------------------------------------------------------------------------
bool vtkPolarAxesRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* pvview = vtkPVRenderView::SafeDownCast(view);
  if (pvview)
  {
    if (vtkRenderer* renderer = pvview->GetRenderer(this->RendererType))
    {
      renderer->AddActor(this->PolarAxesActor);
      this->PolarAxesActor->SetCamera(renderer->GetActiveCamera());
      this->RenderView = pvview;
      return this->Superclass::AddToView(view);
    }
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkPolarAxesRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* pvview = vtkPVRenderView::SafeDownCast(view);
  if (pvview)
  {
    if (vtkRenderer* renderer = pvview->GetRenderer(this->RendererType))
    {
      renderer->RemoveActor(this->PolarAxesActor);
      this->PolarAxesActor->SetCamera(nullptr);
      this->RenderView = nullptr;
      return this->Superclass::RemoveFromView(view);
    }
  }
  this->RenderView = nullptr;
  return false;
}

//----------------------------------------------------------------------------
int vtkPolarAxesRepresentation::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkHyperTreeGrid");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkPolarAxesRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
  {
    return 0;
  }

  if (request_type == vtkPVView::REQUEST_UPDATE())
  {
    vtkPVRenderView::SetPiece(inInfo, this, this->OutlineGeometry.Get());
    vtkPVRenderView::SetDeliverToAllProcesses(inInfo, this, true);
  }
  else if (request_type == vtkPVView::REQUEST_RENDER())
  {
    vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
    if (producerPort)
    {
      vtkAlgorithm* producer = producerPort->GetProducer();
      vtkDataObject* data =
        vtkDataObject::SafeDownCast(producer->GetOutputDataObject(producerPort->GetIndex()));
      if ((data && data->GetMTime() > this->DataBoundsTime.GetMTime()) ||
        this->GetMTime() > this->DataBoundsTime.GetMTime())
      {
        this->InitializeDataBoundsFromData(data);
        this->UpdateBounds();
      }
    }
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkPolarAxesRepresentation::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkMath::UninitializeBounds(this->DataBounds);
  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
  {
    vtkDataObject* input = vtkDataObject::GetData(inputVector[0], 0);
    this->InitializeDataBoundsFromData(input);
    this->UpdateBounds();
  }

  if (vtkMath::AreBoundsInitialized(this->DataBounds))
  {
    vtkNew<vtkOutlineSource> outlineSource;
    outlineSource->SetBoxTypeToAxisAligned();
    outlineSource->SetBounds(this->DataBounds);
    outlineSource->Update();
    this->OutlineGeometry->ShallowCopy(outlineSource->GetOutput());
  }
  else
  {
    this->OutlineGeometry->Initialize();
  }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::InitializeDataBoundsFromData(vtkDataObject* data)
{
  vtkMath::UninitializeBounds(this->DataBounds);
  vtkDataSet* ds = vtkDataSet::SafeDownCast(data);
  vtkCompositeDataSet* cd = vtkCompositeDataSet::SafeDownCast(data);
  vtkHyperTreeGrid* htg = vtkHyperTreeGrid::SafeDownCast(data);
  if (ds)
  {
    ds->GetBounds(this->DataBounds);
  }
  else if (cd)
  {
    vtkCompositeDataIterator* iter = cd->NewIterator();
    vtkBoundingBox bbox;
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      ds = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
      htg = vtkHyperTreeGrid::SafeDownCast(iter->GetCurrentDataObject());

      double bds[6];
      if (ds)
      {
        ds->GetBounds(bds);
      }
      else if (htg)
      {
        htg->GetBounds(bds);
      }

      if (vtkMath::AreBoundsInitialized(bds))
      {
        bbox.AddBounds(bds);
      }
    }
    iter->Delete();
    bbox.GetBounds(this->DataBounds);
  }
  else if (htg)
  {
    htg->GetBounds(this->DataBounds);
  }
  this->DataBoundsTime.Modified();
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::UpdateBounds()
{
  double bds[6];
  this->GetDataBounds(bds);

  // overload bounds with the active custom bounds
  for (int i = 0; i < 3; ++i)
  {
    int pos = i * 2;
    if (this->EnableCustomBounds[i])
    {
      bds[pos] = this->CustomBounds[pos];
      bds[pos + 1] = this->CustomBounds[pos + 1];
    }
  }

  vtkBoundingBox bbox(bds);

  vtkNew<vtkTransform> transform;
  transform->Translate(this->Translation);
  transform->RotateX(this->Orientation[0]);
  transform->RotateY(this->Orientation[1]);
  transform->RotateZ(this->Orientation[2]);
  transform->Scale(this->Scale);

  vtkBoundingBox transformedBox;
  for (int corner = 0; corner < 8; corner++)
  {
    double pt[3];
    bbox.GetCorner(corner, pt);
    transform->TransformPoint(pt, pt);
    transformedBox.AddPoint(pt);
  }

  transformedBox.GetBounds(bds);

  double pole[3] = { 0.0, 0.0, 0.0 };
  double center[3];
  transformedBox.GetCenter(center);
  double maxRadius = this->EnableCustomMaxRadius ? this->MaxRadius : 0.0;
  double minRadius = this->EnableCustomMinRadius ? this->MinRadius : 0.0;
  double minAngle = EnableCustomAngle ? this->MinAngle : 0.0;
  double maxAngle = EnableCustomAngle ? this->MaxAngle : 360.0;

  if (this->EnableAutoPole)
  {
    pole[0] = center[0];
    pole[1] = center[1];
    pole[2] = center[2];

    if (!this->EnableCustomMaxRadius)
    {
      maxRadius = sqrt(pow(bds[1] - center[0], 2) + pow(bds[3] - center[1], 2));
    }
  }
  else
  {
    for (std::size_t ind{ 0 }; ind < 3; ++ind)
    {
      pole[ind] = this->Position[ind];
    }

    if (!this->EnableCustomMaxRadius)
    {
      // Compute the max length between pole and bounds for maximum radius
      // Check bottom-left, top-left, bottom-right, top-right
      if (pole[0] < center[0])
      {
        if (pole[1] < center[1])
        {
          maxRadius = sqrt(pow(bds[1] - pole[0], 2) + pow(bds[3] - pole[1], 2));
        }
        else
        {
          maxRadius = sqrt(pow(bds[1] - pole[0], 2) + pow(bds[2] - pole[1], 2));
        }
      }
      else
      {
        if (pole[1] < center[1])
        {
          maxRadius = sqrt(pow(bds[0] - pole[0], 2) + pow(bds[3] - pole[1], 2));
        }
        else
        {
          maxRadius = sqrt(pow(bds[0] - pole[0], 2) + pow(bds[2] - pole[1], 2));
        }
      }
    }
    // Compute the min length between pole and bounds if pole is outside box for minimum radius and
    // min/max angle
    // Check bottom-left, top-left, left, bottom-right, top-right, right, bottom, top
    // If inside, keep default values
    if (pole[0] <= bds[0])
    {
      if (!this->EnableCustomMinRadius)
      {
        if (pole[1] < bds[2])
        {
          minRadius = sqrt(pow(bds[0] - pole[0], 2) + pow(bds[2] - pole[1], 2));
        }
        else if (pole[1] > bds[3])
        {
          minRadius = sqrt(pow(bds[0] - pole[0], 2) + pow(pole[1] - bds[3], 2));
        }
        else
        {
          minRadius = bds[0] - pole[0];
        }
      }

      if (!this->EnableCustomAngle)
      {
        maxAngle = ((pole[1] <= bds[3]) ? atan((bds[3] - pole[1]) / (bds[0] - pole[0]))
                                        : atan((bds[3] - pole[1]) / (bds[1] - pole[0]))) *
          180.0 / vtkMath::Pi();
        minAngle = ((pole[1] <= bds[2]) ? atan((bds[2] - pole[1]) / (bds[1] - pole[0]))
                                        : atan((bds[2] - pole[1]) / (bds[0] - pole[0]))) *
          180.0 / vtkMath::Pi();
      }
    }
    else if (pole[0] >= bds[1])
    {
      if (!this->EnableCustomMinRadius)
      {
        if (pole[1] < bds[2])
        {
          minRadius = sqrt(pow(pole[0] - bds[1], 2) + pow(bds[2] - pole[1], 2));
        }
        else if (pole[1] > bds[3])
        {
          minRadius = sqrt(pow(pole[0] - bds[1], 2) + pow(pole[1] - bds[3], 2));
        }
        else
        {
          minRadius = pole[0] - bds[1];
        }
      }

      if (!this->EnableCustomAngle)
      {
        maxAngle = 180 +
          ((pole[1] <= bds[2]) ? atan((bds[2] - pole[1]) / (bds[0] - pole[0]))
                               : atan((bds[2] - pole[1]) / (bds[1] - pole[0]))) *
            180 / vtkMath::Pi();
        minAngle = 180 +
          ((pole[1] <= bds[3]) ? atan((bds[3] - pole[1]) / (bds[1] - pole[0]))
                               : atan((bds[3] - pole[1]) / (bds[0] - pole[0]))) *
            180 / vtkMath::Pi();
      }
    }
    else if (pole[1] <= bds[2])
    {
      if (!this->EnableCustomMinRadius)
      {
        minRadius = bds[2] - pole[1];
      }

      if (!this->EnableCustomAngle)
      {
        maxAngle = 180 + atan((bds[2] - pole[1]) / (bds[0] - pole[0])) * 180 / vtkMath::Pi();
        minAngle = atan((bds[2] - pole[1]) / (bds[1] - pole[0])) * 180 / vtkMath::Pi();
      }
    }
    else if (pole[1] >= bds[3])
    {
      if (!this->EnableCustomMinRadius)
      {
        minRadius = pole[1] - bds[3];
      }

      if (!this->EnableCustomAngle)
      {
        maxAngle = atan((bds[3] - pole[1]) / (bds[1] - pole[0])) * 180 / vtkMath::Pi();
        minAngle = 180 + atan((bds[3] - pole[1]) / (bds[0] - pole[0])) * 180 / vtkMath::Pi();
      }
    }
  }

  this->PolarAxesActor->SetMinimumRadius(minRadius);
  this->PolarAxesActor->SetMaximumRadius(maxRadius);
  this->PolarAxesActor->SetMinimumAngle(minAngle);
  this->PolarAxesActor->SetMaximumAngle(maxAngle);

  if (this->EnableCustomRange)
  {
    this->PolarAxesActor->SetRange(this->CustomRange);
  }
  else
  {
    this->PolarAxesActor->SetRange(minRadius, maxRadius);
  }

  // SetPole triggers a bounds computation based on Min/Max Radii and Angles.
  // So call it after everything is setup.
  this->PolarAxesActor->SetPole(pole);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->PolarAxesActor->PrintSelf(os, indent.GetNextIndent());
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLog(bool active)
{
  this->PolarAxesActor->SetLog(active);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetNumberOfRadialAxes(vtkIdType val)
{
  this->PolarAxesActor->SetRequestedNumberOfRadialAxes(val);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetNumberOfPolarAxes(vtkIdType val)
{
  this->PolarAxesActor->SetRequestedNumberOfPolarAxes(val);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDeltaAngleRadialAxes(double angle)
{
  this->PolarAxesActor->SetRequestedDeltaAngleRadialAxes(angle);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDeltaRangePolarAxes(double range)
{
  this->PolarAxesActor->SetRequestedDeltaRangePolarAxes(range);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetSmallestVisiblePolarAngle(double angle)
{
  this->PolarAxesActor->SetSmallestVisiblePolarAngle(angle);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetTickLocation(int location)
{
  this->PolarAxesActor->SetTickLocation(location);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialUnits(bool use)
{
  this->PolarAxesActor->SetRadialUnits(use);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetScreenSize(double size)
{
  this->PolarAxesActor->SetScreenSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisTitle(const char* title)
{
  this->PolarAxesActor->SetPolarAxisTitle(title);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarLabelFormat(const char* format)
{
  this->PolarAxesActor->SetPolarLabelFormat(format);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetExponentLocation(int location)
{
  this->PolarAxesActor->SetExponentLocation(location);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialAngleFormat(const char* format)
{
  this->PolarAxesActor->SetRadialAngleFormat(format);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetEnableDistanceLOD(int enable)
{
  this->PolarAxesActor->SetEnableDistanceLOD(enable);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDistanceLODThreshold(double val)
{
  this->PolarAxesActor->SetDistanceLODThreshold(val);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetEnableViewAngleLOD(int enable)
{
  this->PolarAxesActor->SetEnableViewAngleLOD(enable);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetViewAngleLODThreshold(double val)
{
  this->PolarAxesActor->SetViewAngleLODThreshold(val);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisVisibility(int visible)
{
  this->PolarAxesActor->SetPolarAxisVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDrawRadialGridlines(int draw)
{
  this->PolarAxesActor->SetDrawRadialGridlines(draw);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDrawPolarArcsGridlines(int draw)
{
  this->PolarAxesActor->SetDrawPolarArcsGridlines(draw);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarTitleVisibility(int visible)
{
  this->PolarAxesActor->SetPolarTitleVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialAxisTitleLocation(int location)
{
  this->PolarAxesActor->SetRadialAxisTitleLocation(location);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisTitleLocation(int location)
{
  this->PolarAxesActor->SetPolarAxisTitleLocation(location);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialTitleOffset(double offsetX, double offsetY)
{
  this->PolarAxesActor->SetRadialTitleOffset(offsetX, offsetY);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarTitleOffset(double offsetX, double offsetY)
{
  this->PolarAxesActor->SetPolarTitleOffset(offsetX, offsetY);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarLabelOffset(double offsetY)
{
  this->PolarAxesActor->SetPolarLabelOffset(offsetY);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarExponentOffset(double offsetY)
{
  this->PolarAxesActor->SetPolarExponentOffset(offsetY);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarLabelVisibility(int visible)
{
  this->PolarAxesActor->SetPolarLabelVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcTicksOriginToPolarAxis(int use)
{
  this->PolarAxesActor->SetArcTicksOriginToPolarAxis(use);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialAxesOriginToPolarAxis(int use)
{
  this->PolarAxesActor->SetRadialAxesOriginToPolarAxis(use);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarTickVisibility(int visible)
{
  this->PolarAxesActor->SetPolarTickVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetAxisTickVisibility(int visible)
{
  this->PolarAxesActor->SetAxisTickVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetAxisMinorTickVisibility(int visible)
{
  this->PolarAxesActor->SetAxisMinorTickVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetAxisTickMatchesPolarAxes(int enable)
{
  this->PolarAxesActor->SetAxisTickMatchesPolarAxes(enable);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcTickVisibility(int visible)
{
  this->PolarAxesActor->SetArcTickVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcMinorTickVisibility(int visible)
{
  this->PolarAxesActor->SetArcMinorTickVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcTickMatchesRadialAxes(int enable)
{
  this->PolarAxesActor->SetArcTickMatchesRadialAxes(enable);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetTickRatioRadiusSize(double ratio)
{
  this->PolarAxesActor->SetTickRatioRadiusSize(ratio);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcMajorTickSize(double size)
{
  this->PolarAxesActor->SetArcMajorTickSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisMajorTickSize(double size)
{
  this->PolarAxesActor->SetPolarAxisMajorTickSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLastRadialAxisMajorTickSize(double size)
{
  this->PolarAxesActor->SetLastRadialAxisMajorTickSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisTickRatioSize(double size)
{
  this->PolarAxesActor->SetPolarAxisTickRatioSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLastAxisTickRatioSize(double size)
{
  this->PolarAxesActor->SetLastAxisTickRatioSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcTickRatioSize(double size)
{
  this->PolarAxesActor->SetArcTickRatioSize(size);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisMajorTickThickness(double thickness)
{
  this->PolarAxesActor->SetPolarAxisMajorTickThickness(thickness);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLastRadialAxisMajorTickThickness(double thickness)
{
  this->PolarAxesActor->SetLastRadialAxisMajorTickThickness(thickness);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcMajorTickThickness(double thickness)
{
  this->PolarAxesActor->SetArcMajorTickThickness(thickness);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarAxisTickRatioThickness(double thickness)
{
  this->PolarAxesActor->SetPolarAxisTickRatioThickness(thickness);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetLastAxisTickRatioThickness(double thickness)
{
  this->PolarAxesActor->SetLastAxisTickRatioThickness(thickness);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetArcTickRatioThickness(double thickness)
{
  this->PolarAxesActor->SetArcTickRatioThickness(thickness);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDeltaAngleMajor(double range)
{
  this->PolarAxesActor->SetDeltaAngleMajor(range);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDeltaAngleMinor(double range)
{
  this->PolarAxesActor->SetDeltaAngleMinor(range);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialAxesVisibility(int visible)
{
  this->PolarAxesActor->SetRadialAxesVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRadialTitleVisibility(int visible)
{
  this->PolarAxesActor->SetRadialTitleVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarArcsVisibility(int visible)
{
  this->PolarAxesActor->SetPolarArcsVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetUse2DMode(int use)
{
  this->PolarAxesActor->SetUse2DMode(use);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRasterizeText(int use)
{
  this->PolarAxesActor->SetUseTextActor3D(use);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetRatio(double ratio)
{
  this->PolarAxesActor->SetRatio(ratio);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetPolarArcResolutionPerDegree(double resolution)
{
  this->PolarAxesActor->SetPolarArcResolutionPerDegree(resolution);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDeltaRangeMajor(double delta)
{
  this->PolarAxesActor->SetDeltaRangeMajor(delta);
}

//----------------------------------------------------------------------------
void vtkPolarAxesRepresentation::SetDeltaRangeMinor(double delta)
{
  this->PolarAxesActor->SetDeltaRangeMinor(delta);
}
