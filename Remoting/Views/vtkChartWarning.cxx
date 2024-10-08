// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkChartWarning.h"

// Get my new commands
#include "vtkCommand.h"

#include "vtkAxis.h"
#include "vtkBrush.h"
#include "vtkChart.h"
#include "vtkContext2D.h"
#include "vtkContextMouseEvent.h"
#include "vtkContextScene.h"
#include "vtkPen.h"
#include "vtkPlot.h"
#include "vtkTextProperty.h"
#include "vtkVector.h"

#include "vtkObjectFactory.h"

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkChartWarning);

//-----------------------------------------------------------------------------
vtkChartWarning::vtkChartWarning()
{
  this->TextPad = 16.;
}

//-----------------------------------------------------------------------------
vtkChartWarning::~vtkChartWarning() = default;

//-----------------------------------------------------------------------------
void vtkChartWarning::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "TextPad: " << this->TextPad << "\n";
}

//-----------------------------------------------------------------------------
bool vtkChartWarning::Paint(vtkContext2D* painter)
{
  vtkChart* parent = vtkChart::SafeDownCast(this->GetParent());
  if (!parent)
  {
    return true;
  }

  if (!this->ArePlotsImproperlyScaled(parent))
  { // Don't render anything when the plots are OK.
    return true;
  }

  vtkRectf cbds = parent->GetSize();
  float factorw = (this->Dimensions[0] + this->Dimensions[2]) / cbds.GetWidth();
  float factorh = (this->Dimensions[1] + this->Dimensions[3]) / cbds.GetHeight();
  float factor = factorw > factorh ? factorw : factorh;
  // Now convert factor from a proportion of the width/height to a scale factor for this->Dimension
  // if when the proportion (it's starting value) is too high.
  factor = factor > 0.5 ? 0.5 / factor : 1.;

  float dims[4];
  for (int i = 0; i < 4; ++i)
  {
    dims[i] = factor * this->Dimensions[i];
  }

  painter->GetTextProp()->SetVerticalJustificationToCentered();
  painter->GetTextProp()->SetJustificationToCentered();
  painter->GetTextProp()->SetBold(1);
  painter->GetTextProp()->SetColor(1.0, 1.0, 1.0);
  int fontSize;
  float lbds[4]; // label bounds
  for (fontSize = 64; fontSize > 4; --fontSize)
  {
    painter->GetTextProp()->SetFontSize(fontSize);
    painter->ComputeStringBounds(this->Label, lbds);
    if ((lbds[3] - lbds[1] < cbds.GetHeight() - dims[3] - dims[1] - 2 * this->TextPad) &&
      (lbds[2] - lbds[0] < cbds.GetWidth() - dims[2] - dims[0] - 2 * this->TextPad))
    {
      break;
    }
  }

  painter->GetPen()->SetColor(167, 40, 40);
  painter->GetBrush()->SetColor(167, 40, 40);
  painter->GetPen()->SetLineType(vtkPen::SOLID_LINE);
  painter->GetPen()->SetWidth(2);
  // Draw both a rect and a diagonal to avoid the ugly antialiasing
  // artifact generated by drawing the rectangle as 2 triangles.
  painter->DrawRect(cbds.GetX() + dims[0], cbds.GetY() + dims[1],
    cbds.GetWidth() - dims[2] - dims[0], cbds.GetHeight() - dims[3] - dims[1]);
  painter->DrawLine(cbds.GetX() + dims[0], cbds.GetY() + dims[1],
    cbds.GetX() + dims[0] + cbds.GetWidth() - dims[2] - dims[0],
    cbds.GetY() + dims[1] + cbds.GetHeight() - dims[3] - dims[1]);

  // float x = cbds.GetX() + dims[0] - lbds[0] + 0.5 * lbds[2];
  float x = cbds.GetX() + dims[0] - lbds[0] + 0.5 * (cbds.GetWidth() - dims[2] - dims[0]);
  float y = cbds.GetY() + dims[1] - lbds[1] + 0.5 * (cbds.GetHeight() - dims[3] - dims[1]);
  if (!this->Label.empty())
  {
    painter->DrawString(x, y, this->Label);
  }

  this->PaintChildren(painter);
  return true;
}

//-----------------------------------------------------------------------------
bool vtkChartWarning::Hit(const vtkContextMouseEvent&)
{
  return false;
}

//-----------------------------------------------------------------------------
bool vtkChartWarning::ArePlotsImproperlyScaled(vtkChart* chart)
{
  for (int i = 0; i < chart->GetNumberOfPlots(); ++i)
  {
    vtkPlot* plot = chart->GetPlot(i);
    if (!plot->GetVisible())
    {
      continue;
    }
    plot->Update();
    double bds[4];
    plot->GetUnscaledInputBounds(bds);
    // If the bounds are invalid, ignore the plot (it has no extent)
    if (bds[1] < bds[0])
    {
      continue;
    }
    // Are there improper values on the axes that are log-scaled?
    int badAxes = (bds[0] * bds[1] <= 0. ? 1 : 0) + (bds[2] * bds[3] <= 0. ? 2 : 0);
    int logAxes =
      (plot->GetXAxis()->GetLogScale() ? 1 : 0) + (plot->GetYAxis()->GetLogScale() ? 2 : 0);
    if (badAxes & logAxes)
    {
      if ((!(badAxes & logAxes & 1) || plot->GetXAxis()->GetLogScaleActive()) &&
        (!(badAxes & logAxes & 2) || plot->GetYAxis()->GetLogScaleActive()))
      {
        // Has the user changed the vtkAxis range so that no invalid
        // region of the unscaled axis is shown? If so, we do not
        // need to warn (for this plot).
        continue;
      }
      return true;
    }
  }
  return false;
}
