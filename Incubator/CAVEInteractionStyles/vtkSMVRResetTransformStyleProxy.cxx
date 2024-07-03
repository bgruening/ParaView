// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkSMVRResetTransformStyleProxy.h"

#include "vtkCamera.h"
#include "vtkMatrix4x4.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkTransform.h"
#include "vtkVRQueue.h"

#include <algorithm>
#include <cmath>
#include <sstream>

// ----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSMVRResetTransformStyleProxy);

// ----------------------------------------------------------------------------
// Constructor method
vtkSMVRResetTransformStyleProxy::vtkSMVRResetTransformStyleProxy()
  : Superclass()
{
  this->AddButtonRole("Navigate world");
  this->AddButtonRole("Reset world");
  this->EnableNavigate = false;
  this->IsInitialRecorded = false;
}

// ----------------------------------------------------------------------------
// Destructor method
vtkSMVRResetTransformStyleProxy::~vtkSMVRResetTransformStyleProxy() = default;

// ----------------------------------------------------------------------------
// PrintSelf() method
void vtkSMVRResetTransformStyleProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "EnableNavigate: " << this->EnableNavigate << endl;
  os << indent << "IsInitialRecorded: " << this->IsInitialRecorded << endl;
}

// ----------------------------------------------------------------------------
// HandleButton() method
void vtkSMVRResetTransformStyleProxy::HandleButton(const vtkVREvent& event)
{
  std::string role = this->GetButtonRole(event.name);

  if (role == "Navigate world")
  {
    vtkCamera* camera = nullptr;
    this->EnableNavigate = event.data.button.state;
    if (!this->EnableNavigate)
    {
      this->IsInitialRecorded = false;
    }
  }
  else if (event.data.button.state && role == "Reset world")
  {
    vtkNew<vtkMatrix4x4> transformMatrix;
    vtkSMPropertyHelper(this->ControlledProxy, this->ControlledPropertyName)
      .Set(&transformMatrix.GetPointer()->Element[0][0], 16);
    this->IsInitialRecorded = false;
  }
}

// ----------------------------------------------------------------------------
// HandleTracker() method
void vtkSMVRResetTransformStyleProxy::HandleTracker(const vtkVREvent& event)
{
  vtkCamera* camera = nullptr;
  std::string role = this->GetTrackerRole(event.name);

  if (role != "Tracker")
    return;
  if (this->EnableNavigate)
  {
    camera = vtkSMVRInteractorStyleProxy::GetActiveCamera();
    if (!camera)
    {
      vtkWarningMacro(<< " HandleTracker: Cannot grab active camera.");
      return;
    }
    double speed = GetSpeedFactor(camera);

    if (!this->IsInitialRecorded)
    {
      // save the ModelView Matrix the Wand Matrix
      this->SavedModelViewMatrix->DeepCopy(camera->GetModelTransformMatrix());
      this->SavedInverseWandMatrix->DeepCopy(event.data.tracker.matrix);

      this->SavedInverseWandMatrix->SetElement(0, 3, event.data.tracker.matrix[3] * speed);
      this->SavedInverseWandMatrix->SetElement(1, 3, event.data.tracker.matrix[7] * speed);
      this->SavedInverseWandMatrix->SetElement(2, 3, event.data.tracker.matrix[11] * speed);
      vtkMatrix4x4::Invert(
        this->SavedInverseWandMatrix.GetPointer(), this->SavedInverseWandMatrix.GetPointer());
      this->IsInitialRecorded = true;
      vtkWarningMacro(<< "button is pressed");
    }
    else
    {
      // Apply the transfromation and get the new transformation matrix
      vtkNew<vtkMatrix4x4> transformMatrix;
      transformMatrix->DeepCopy(event.data.tracker.matrix);
      transformMatrix->SetElement(0, 3, event.data.tracker.matrix[3] * speed);
      transformMatrix->SetElement(1, 3, event.data.tracker.matrix[7] * speed);
      transformMatrix->SetElement(2, 3, event.data.tracker.matrix[11] * speed);

      vtkMatrix4x4::Multiply4x4(this->SavedInverseWandMatrix.GetPointer(),
        transformMatrix.GetPointer(), transformMatrix.GetPointer());
      vtkMatrix4x4::Multiply4x4(this->SavedModelViewMatrix.GetPointer(),
        transformMatrix.GetPointer(), transformMatrix.GetPointer());

      // Set the new matrix for the proxy.
      vtkSMPropertyHelper(this->ControlledProxy, this->ControlledPropertyName)
        .Set(&transformMatrix->Element[0][0], 16);
      this->ControlledProxy->UpdateVTKObjects();
      vtkWarningMacro(<< "button is continuously pressed");
    }
  }
}

// ----------------------------------------------------------------------------
// GetSpeedFactor(): ...
float vtkSMVRResetTransformStyleProxy::GetSpeedFactor(vtkCamera* cam)
{
  // Return the distance between the camera and the focal point.
  // WRS: and the distance between the camera and the focal point is a speed factor???
  double pos[3];
  double foc[3];

  cam->GetPosition(pos);
  cam->GetFocalPoint(foc);
  pos[0] -= foc[0];
  pos[1] -= foc[1];
  pos[2] -= foc[2];
  pos[0] *= pos[0];
  pos[1] *= pos[1];
  pos[2] *= pos[2];
  return sqrt(pos[0] + pos[1] + pos[2]);
}
