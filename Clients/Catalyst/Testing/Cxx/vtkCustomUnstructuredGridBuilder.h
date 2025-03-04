// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkCustomUnstructuredGridBuilder
 * @brief   Class for creating unstructured grids.
 *
 * Class for creating vtkUnstructuredGrids for a test driver.  The
 * UnstructuredGrid is built directly from a vtkUniformGrid to demonstrate
 * how to input a grid into the coprocessor.
 */

#ifndef vtkCustomUnstructuredGridBuilder_h
#define vtkCustomUnstructuredGridBuilder_h

#include "vtkCPUnstructuredGridBuilder.h"

class vtkDataObject;
class vtkGenericCell;
class vtkIdList;
class vtkPoints;
class vtkUnstructuredGrid;

class vtkCustomUnstructuredGridBuilder : public vtkCPUnstructuredGridBuilder
{
public:
  static vtkCustomUnstructuredGridBuilder* New();
  vtkTypeMacro(vtkCustomUnstructuredGridBuilder, vtkCPUnstructuredGridBuilder);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Return a grid.  BuiltNewGrid is set to 0 if the grids
   * that were returned were already built before.
   * vtkCustomUnstructuredGridBuilder will also delete the grid.
   */
  vtkDataObject* GetGrid(unsigned long timeStep, double time, int& builtNewGrid) override;

  /**
   * Customized function to build UnstructuredGrid.
   */
  void BuildGrid();

protected:
  vtkCustomUnstructuredGridBuilder();
  ~vtkCustomUnstructuredGridBuilder() override;

  /**
   * Method to compute the centroid of Cell and return the values in xyz.
   */
  void ComputeCellCentroid(vtkGenericCell* cell, double xyz[3]);

private:
  vtkCustomUnstructuredGridBuilder(const vtkCustomUnstructuredGridBuilder&) = delete;
  void operator=(const vtkCustomUnstructuredGridBuilder&) = delete;
};
#endif
