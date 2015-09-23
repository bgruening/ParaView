#include <algorithm>
#include <iostream>
#include <vector>
#include "FEAdaptor.h"

// Example of a C++ adaptor for a simulation code
// where the simulation code has a fixed topology
// grid. The grid in this case is a vtkOverlappingAMR
// data set without any attributes specified
// over the grid.

int main(int argc, char* argv[])
{
  FEAdaptor::Initialize(argc, argv);
  unsigned int numberOfTimeSteps = 100;
  for(unsigned int timeStep=0;timeStep<numberOfTimeSteps;timeStep++)
    {
    // use a time step length of 0.1
    double time = timeStep * 0.1;
    FEAdaptor::CoProcess(time, timeStep, timeStep == numberOfTimeSteps-1);
    }
  FEAdaptor::Finalize();

  return 0;
}
