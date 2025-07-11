// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkSMCoreUtilities.h"

#include "vtkCellTypes.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkSMArraySelectionDomain.h"
#include "vtkSMDomain.h"
#include "vtkSMDomainIterator.h"
#include "vtkSMFileListDomain.h"
#include "vtkSMInputProperty.h"
#include "vtkSMOrderedPropertyIterator.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyManager.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSmartPointer.h"
#include <vtksys/SystemTools.hxx>

#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

vtkStandardNewMacro(vtkSMCoreUtilities);
//----------------------------------------------------------------------------
vtkSMCoreUtilities::vtkSMCoreUtilities() = default;

//----------------------------------------------------------------------------
vtkSMCoreUtilities::~vtkSMCoreUtilities() = default;

//----------------------------------------------------------------------------
const char* vtkSMCoreUtilities::GetFileNameProperty(vtkSMProxy* proxy)
{
  if (!proxy)
  {
    return nullptr;
  }

  if (proxy->GetHints())
  {
    vtkPVXMLElement* filenameHint =
      proxy->GetHints()->FindNestedElementByName("DefaultFileNameProperty");
    if (filenameHint && filenameHint->GetAttribute("name") &&
      proxy->GetProperty(filenameHint->GetAttribute("name")))
    {
      return filenameHint->GetAttribute("name");
    }
  }

  // Find the first property that has a vtkSMFileListDomain. Assume that
  // it is the property used to set the filename.
  vtkSmartPointer<vtkSMPropertyIterator> piter;
  piter.TakeReference(proxy->NewPropertyIterator());
  piter->Begin();
  while (!piter->IsAtEnd())
  {
    vtkSMProperty* prop = piter->GetProperty();
    if (prop && prop->IsA("vtkSMStringVectorProperty"))
    {
      vtkSmartPointer<vtkSMDomainIterator> diter;
      diter.TakeReference(prop->NewDomainIterator());
      diter->Begin();
      while (!diter->IsAtEnd())
      {
        if (diter->GetDomain()->IsA("vtkSMFileListDomain"))
        {
          return piter->GetKey();
        }
        diter->Next();
      }
      if (!diter->IsAtEnd())
      {
        break;
      }
    }
    piter->Next();
  }
  return nullptr;
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkSMCoreUtilities::GetFileNameProperties(vtkSMProxy* proxy)
{
  std::vector<std::string> result;
  if (!proxy)
  {
    return result;
    ;
  }

  vtkNew<vtkSMOrderedPropertyIterator> piter;
  piter->SetProxy(proxy);
  for (piter->Begin(); !piter->IsAtEnd(); piter->Next())
  {
    if (auto property = piter->GetProperty())
    {
      if (property->FindDomain<vtkSMFileListDomain>() != nullptr)
      {
        result.push_back(piter->GetKey());
      }
    }
  }
  return result;
}

//----------------------------------------------------------------------------
// This is reimplemented in python's paraview.make_name_valid(). Keep both
// implementations consistent.
std::string vtkSMCoreUtilities::SanitizeName(const char* name)
{
  if (!name || name[0] == '\0')
  {
    return std::string();
  }

  std::ostringstream cname;
  for (size_t cc = 0; name[cc]; cc++)
  {
    if (isalnum(name[cc]) || name[cc] == '_')
    {
      cname << name[cc];
    }
  }
  // if first character is not an alphabet, add an 'a' to it.
  if (cname.str().empty() || isalpha(cname.str()[0]))
  {
    return cname.str();
  }
  else
  {
    return "a" + cname.str();
  }
}

//----------------------------------------------------------------------------
bool vtkSMCoreUtilities::AdjustRangeForLog(double range[2])
{
  assert(range[0] <= range[1]);
  if (range[0] <= 0.0 || range[1] <= 0.0)
  {
    // ranges not valid for log-space. Cannot convert.
    if (range[1] <= 0.0)
    {
      range[0] = 1.0e-4;
      range[1] = 1.0;
    }
    else
    {
      range[0] = range[1] * 0.0001;
      range[0] = (range[0] < 1.0) ? range[0] : 1.0;
    }
    return true;
  }
  return false;
}

namespace
{

template <typename T>
struct MinDelta
{
};
// This value seems to work well for float ranges we have tested
template <>
struct MinDelta<float>
{
  static const int value = 2048;
};
template <>
struct MinDelta<double>
{
  static const vtkTypeInt64 value = static_cast<vtkTypeInt64>(2048);
};

// Reperesents the following:
// T m = std::numeric_limits<T>::min();
// EquivSizeIntT im;
// std::memcpy(&im, &m, sizeof(T));
//
template <typename EquivSizeIntT>
struct MinRepresentable
{
};
template <>
struct MinRepresentable<float>
{
  static const int value = 8388608;
};
template <>
struct MinRepresentable<double>
{
  static const vtkTypeInt64 value = 4503599627370496L;
};

//----------------------------------------------------------------------------
template <typename T, typename EquivSizeIntT>
bool AdjustTRange(T range[2], EquivSizeIntT, EquivSizeIntT ulpsDiff = MinDelta<T>::value)
{
  if (range[1] < range[0])
  {
    // invalid range.
    return false;
  }

  const bool spans_zero_boundary = range[0] < 0 && range[1] > 0;
  if (spans_zero_boundary)
  { // nothing needs to be done, but this check is required.
    // if we convert into integer space the delta difference will overflow
    // an integer
    return false;
  }

  EquivSizeIntT irange[2];
  // needs to be a memcpy to avoid strict aliasing issues, doing a count
  // of 2*sizeof(T) to couple both values at the same time
  std::memcpy(irange, range, sizeof(T) * 2);

  const bool denormal = !std::isnormal(range[0]);
  const EquivSizeIntT minInt = MinRepresentable<T>::value;
  const EquivSizeIntT minDelta = denormal ? minInt + ulpsDiff : ulpsDiff;

  // determine the absolute delta between these two numbers.
  const EquivSizeIntT delta = std::abs(irange[1] - irange[0]);

  // if our delta is smaller than the min delta push out the max value
  // so that it is equal to minRange + minDelta. When our range is entirely
  // negative we should instead subtract from our max, to max a larger negative
  // value
  if (delta < minDelta)
  {
    if (irange[0] < 0)
    {
      irange[1] = irange[0] - minDelta;
    }
    else
    {
      irange[1] = irange[0] + minDelta;
    }
    std::memcpy(range, irange, sizeof(T) * 2);
    return true;
  }
  return false;
}
}

//----------------------------------------------------------------------------
bool vtkSMCoreUtilities::AlmostEqual(const double range[2], int ulpsDiff)
{
  double trange[2] = { range[0], range[1] };
  return AdjustTRange(trange, vtkTypeInt64(), vtkTypeInt64(ulpsDiff));
}

//----------------------------------------------------------------------------
bool vtkSMCoreUtilities::AdjustRange(double range[2])
{
  // If the numbers are not nearly equal, we don't touch them. This avoids running into
  // pitfalls like BUG #17152.
  if (!vtkSMCoreUtilities::AlmostEqual(range, 1024))
  {
    return false;
  }

  // Since the range is 0-range, we will offset range[1]. We've found it best to offset
  // it in float space, if possible.
  if (range[0] > VTK_FLOAT_MIN && range[0] < VTK_FLOAT_MAX)
  {
    float frange[2] = { static_cast<float>(range[0]), static_cast<float>(range[1]) };
    bool result = AdjustTRange(frange, vtkTypeInt32());
    if (result)
    { // range should be left untouched to avoid loss of precision when no
      // adjustment was needed
      range[1] = static_cast<double>(frange[1]);
    }
    return result;
  }

  return AdjustTRange(range, vtkTypeInt64());
}

//----------------------------------------------------------------------------
const char* vtkSMCoreUtilities::GetInputPropertyName(vtkSMProxy* proxy, int port)
{
  if (!proxy)
  {
    return nullptr;
  }

  vtkNew<vtkSMOrderedPropertyIterator> piter;
  piter->SetProxy(proxy);
  piter->Begin();
  while (!piter->IsAtEnd())
  {
    if (vtkSMInputProperty* ip = vtkSMInputProperty::SafeDownCast(piter->GetProperty()))
    {
      if (ip->GetPortIndex() == port)
      {
        return ip->GetXMLName();
      }
    }
    piter->Next();
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
std::string vtkSMCoreUtilities::FindLargestPrefix(const std::vector<std::string>& files)
{
  if (files.empty())
  {
    return std::string();
  }

  size_t numFiles = files.size();
  std::string regName = vtksys::SystemTools::GetFilenameName(files[0]);

  if (numFiles > 1)
  {
    for (size_t i = 1; i < numFiles; i++)
    {
      std::string nextFile = vtksys::SystemTools::GetFilenameName(files[i]);
      if (nextFile.substr(0, regName.size()) == regName)
      {
        continue;
      }
      std::string commonPrefix = regName;
      do
      {
        commonPrefix = commonPrefix.substr(0, commonPrefix.size() - 1);
      } while (
        nextFile.substr(0, commonPrefix.size() - 1) != commonPrefix && !commonPrefix.empty());
      if (commonPrefix.empty())
      {
        break;
      }
      regName = commonPrefix;
    }
    regName += '*';
  }

  return regName;
}

//----------------------------------------------------------------------------
void vtkSMCoreUtilities::ReplaceReaderFileName(
  vtkSMProxy* proxy, const std::vector<std::string>& files, const char* propName)
{
  auto newProxy = vtkSmartPointer<vtkSMSourceProxy>::Take(vtkSMSourceProxy::SafeDownCast(
    vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager()->NewProxy(
      proxy->GetXMLGroup(), proxy->GetXMLName())));
  auto fileNameProp = vtkSMStringVectorProperty::SafeDownCast(proxy->GetProperty(propName));
  auto newFileNameProp = vtkSMStringVectorProperty::SafeDownCast(newProxy->GetProperty(propName));

  std::set<std::string> fileNames;
  for (unsigned int fileId = 0; fileId < fileNameProp->GetNumberOfElements(); ++fileId)
  {
    fileNames.insert(std::string(fileNameProp->GetElement(fileId)));
  }

  unsigned int numberOfCommonFiles = 0;
  for (const std::string& file : files)
  {
    numberOfCommonFiles += static_cast<unsigned int>(fileNames.count(file));
  }

  if (numberOfCommonFiles == fileNameProp->GetNumberOfElements())
  {
    // No need to create a new reader, the user just selected the same set of files.
    return;
  }

  vtkNew<vtkSMParaViewPipelineController> controller;
  controller->PreInitializeProxy(newProxy);

  for (const std::string& file : files)
  {
    newFileNameProp->SetElement(0, file.c_str());
  }

  newProxy->UpdateVTKObjects();
  controller->PostInitializeProxy(newProxy);

  // We want to copy properties from the old proxy to the new proxy.  We have to handle array
  // names very carefully:
  // We create a lookup from array name to its state ("1" or "0" depending of if it is checked).
  // Then, we iterate on the new proxy, and for any array that was already present in the old proxy,
  // we copy its state.
  // This is only valid for properties that have a vtkSMArraySelectionDomain.
  // For other properties, we blindly copy what once was.
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> arraySelectionState;

  // Creating the lookup
  {
    auto iter = vtkSmartPointer<vtkSMPropertyIterator>::Take(proxy->NewPropertyIterator());
    for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
      if (auto prop = vtkSMStringVectorProperty::SafeDownCast(iter->GetProperty()))
      {
        // We only want to copy array selections, which shall have 2 elements per command
        // and have a pattern [str int] for each command.
        if (prop->FindDomain<vtkSMArraySelectionDomain>() &&
          prop->GetNumberOfElementsPerCommand() == 2 && prop->GetElementType(0) == 2 &&
          prop->GetElementType(1) == 0)
        {
          const std::vector<std::string>& elements = prop->GetElements();
          auto& selection = arraySelectionState[std::string(iter->GetKey())];
          for (int elementId = 0; elementId < static_cast<int>(elements.size()); elementId += 2)
          {
            // inserting elements of style { "name", "0" }  or { "name", "1" }
            selection[elements[elementId]] = elements[elementId + 1];
          }
        }
      }
    }
  }

  // Copying
  {
    auto iter = vtkSmartPointer<vtkSMPropertyIterator>::Take(newProxy->NewPropertyIterator());
    for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
      const char* key = iter->GetKey();
      vtkSMProperty* newProp = iter->GetProperty();
      if (newProp)
      {
        if (strcmp(key, propName) != 0)
        {
          if (vtkSMProperty* prop = proxy->GetProperty(key))
          {
            auto newPropStr = vtkSMStringVectorProperty::SafeDownCast(newProp);

            // For properties holding arrays, we copy the previous state as much as we can
            if (newPropStr && newPropStr->FindDomain<vtkSMArraySelectionDomain>() &&
              newPropStr->GetNumberOfElementsPerCommand() == 2 &&
              newPropStr->GetElementType(0) == 2 && newPropStr->GetElementType(1) == 0)
            {
              const std::vector<std::string>& elements = newPropStr->GetElements();
              const auto& selection = arraySelectionState.at(std::string(key));
              for (int elementId = 0; elementId < static_cast<int>(elements.size()); elementId += 2)
              {
                auto it = selection.find(elements[elementId]);
                if (it != selection.end())
                {
                  newPropStr->SetElement(elementId + 1, it->second.c_str());
                }
              }
            }
            // For other properties, we just copy
            else if (!vtkSMProxyProperty::SafeDownCast(newProp))
            {
              newProp->Copy(prop);
            }
          }
        }
      }
    }
  }
  newProxy->UpdateVTKObjects();

  std::string fileName = files.size() > 1 ? vtkSMCoreUtilities::FindLargestPrefix(files)
                                          : vtksys::SystemTools::GetFilenameName(files[0]);

  // If the user renamed the proxy, keep its name.
  const char* proxyName =
    vtkSMProxyManager::GetProxyManager()->GetProxyName(proxy->GetXMLGroup(), proxy);
  if (std::string(proxyName) != vtksys::SystemTools::GetFilenameName(*fileNames.begin()))
  {
    fileName = proxyName;
  }

  // We need to register the new proxy before we change the input property of the consumers of the
  // legacy proxy
  controller->RegisterPipelineProxy(newProxy, fileName.c_str());

  // For some reason, consumers from proxy get wiped out when setting new input proxies,
  // so we rely on a copy of the consumers
  unsigned int numberOfConsumers = proxy->GetNumberOfConsumers();
  std::vector<vtkSmartPointer<vtkSMProxy>> consumers(numberOfConsumers);
  std::vector<vtkSmartPointer<vtkSMProperty>> consumerProperties(numberOfConsumers);
  for (unsigned int id = 0; id < numberOfConsumers; ++id)
  {
    consumers[id] = proxy->GetConsumerProxy(id);
    consumerProperties[id] = proxy->GetConsumerProperty(id);
  }

  // Setting up consumers input to new proxy
  for (unsigned int id = 0; id < numberOfConsumers; ++id)
  {
    vtkSMProxy* consumer = consumers[id];
    auto prop = vtkSMProxyProperty::SafeDownCast(consumerProperties[id]);
    if (!prop)
    {
      continue;
    }
    for (unsigned int proxyId = 0; proxyId < prop->GetNumberOfProxies(); ++proxyId)
    {
      if (proxy == prop->GetProxy(proxyId))
      {
        vtkSMPropertyHelper helper(prop);
        helper.Set(proxyId, newProxy,
          vtkSMInputProperty::SafeDownCast(prop) ? helper.GetOutputPort(proxyId) : 0);
        consumer->UpdateVTKObjects();
        break;
      }
    }
  }

  // We don't need the legacy proxy anymore, we can delete it.
  controller->UnRegisterPipelineProxy(proxy);

  newProxy->UpdatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMCoreUtilities::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

const char* vtkSMCoreUtilities::GetStringForCellType(int cellType)
{
  switch (cellType)
  {
    case VTK_EMPTY_CELL:
      return "Empty";
    case VTK_VERTEX:
      return "Vertex";
    case VTK_POLY_VERTEX:
      return "Poly-Vertex";
    case VTK_LINE:
      return "Line";
    case VTK_POLY_LINE:
      return "Poly-Line";
    case VTK_TRIANGLE:
      return "Triangle";
    case VTK_TRIANGLE_STRIP:
      return "Triangle-Strip";
    case VTK_POLYGON:
      return "Polygon";
    case VTK_PIXEL:
      return "Pixel";
    case VTK_QUAD:
      return "Quad";
    case VTK_TETRA:
      return "Tetrahedron";
    case VTK_VOXEL:
      return "Voxel";
    case VTK_HEXAHEDRON:
      return "Hexahedron";
    case VTK_WEDGE:
      return "Wedge";
    case VTK_PYRAMID:
      return "Pyramid";
    case VTK_PENTAGONAL_PRISM:
      return "Pentagonal-Prism";
    case VTK_HEXAGONAL_PRISM:
      return "Hexagonal-Prism";

    case VTK_QUADRATIC_EDGE:
      return "Quadratic-Edge";
    case VTK_QUADRATIC_TRIANGLE:
      return "Quadratic-Triangle";
    case VTK_QUADRATIC_QUAD:
      return "Quadratic-Quad";
    case VTK_QUADRATIC_TETRA:
      return "Quadratic-Tetrahedron";
    case VTK_QUADRATIC_HEXAHEDRON:
      return "Quadratic Hexahedron";
    case VTK_QUADRATIC_WEDGE:
      return "Quadratic Wedge";
    case VTK_QUADRATIC_PYRAMID:
      return "Quadratic Pyramid";
    case VTK_QUADRATIC_POLYGON:
      return "Quadratic Polygon";
    case VTK_BIQUADRATIC_QUAD:
      return "Bi-Quadratic-Quad";
    case VTK_TRIQUADRATIC_HEXAHEDRON:
      return "Tri-Quadratic-Hexahedron";
    case VTK_TRIQUADRATIC_PYRAMID:
      return "Tri-Quadratic-Pyramid";
    case VTK_QUADRATIC_LINEAR_QUAD:
      return "Quadratic-Linear-Quad";
    case VTK_QUADRATIC_LINEAR_WEDGE:
      return "Quadratic-Linear-Wedge";
    case VTK_BIQUADRATIC_QUADRATIC_WEDGE:
      return "Bi-Quadratic-Wedge";
    case VTK_BIQUADRATIC_QUADRATIC_HEXAHEDRON:
      return "Bi-Quadratic-Quadratic-Hexahedron";
    case VTK_BIQUADRATIC_TRIANGLE:
      return "Bi-Quadratic-Triangle";
    case VTK_CUBIC_LINE:
      return "Cubic-Line";

    case VTK_CONVEX_POINT_SET:
      return "Convex-Point-Set";

    case VTK_POLYHEDRON:
      return "Polyhedron";
    case VTK_PARAMETRIC_CURVE:
      return "Parametric-Curve";
    case VTK_PARAMETRIC_SURFACE:
      return "Parametric-Surface";
    case VTK_PARAMETRIC_TRI_SURFACE:
      return "Parametric-Tri-Surface";
    case VTK_PARAMETRIC_QUAD_SURFACE:
      return "Parametric-Quad-Surface";
    case VTK_PARAMETRIC_TETRA_REGION:
      return "Parametric-Tetra-Region";
    case VTK_PARAMETRIC_HEX_REGION:
      return "Parametric-Hex-Region";

    case VTK_HIGHER_ORDER_EDGE:
      return "Higher-Order-Edge";
    case VTK_HIGHER_ORDER_TRIANGLE:
      return "Higher-Order-Triangle";
    case VTK_HIGHER_ORDER_QUAD:
      return "Higher-Order-Quad";
    case VTK_HIGHER_ORDER_POLYGON:
      return "Higher-Order-Polygon";
    case VTK_HIGHER_ORDER_TETRAHEDRON:
      return "Higher-Order-Tetrahedron";
    case VTK_HIGHER_ORDER_WEDGE:
      return "Higher-Order-Wedge";
    case VTK_HIGHER_ORDER_PYRAMID:
      return "Higher-Order-Pyramid";
    case VTK_HIGHER_ORDER_HEXAHEDRON:
      return "Higher-Order-Hexahedron";

    case VTK_LAGRANGE_CURVE:
      return "Lagrange Curve";
    case VTK_LAGRANGE_TRIANGLE:
      return "Lagrange Triangle";
    case VTK_LAGRANGE_QUADRILATERAL:
      return "Lagrange Quadrilateral";
    case VTK_LAGRANGE_TETRAHEDRON:
      return "Lagrange Tetrahedron";
    case VTK_LAGRANGE_HEXAHEDRON:
      return "Lagrange Hexahedron";
    case VTK_LAGRANGE_WEDGE:
      return "Lagrange Wedge";
    case VTK_LAGRANGE_PYRAMID:
      return "Lagrange Pyramid";

    case VTK_BEZIER_CURVE:
      return "Bezier Curve";
    case VTK_BEZIER_TRIANGLE:
      return "Bezier Triangle";
    case VTK_BEZIER_QUADRILATERAL:
      return "Bezier Quadrilateral";
    case VTK_BEZIER_TETRAHEDRON:
      return "Bezier Tetrahedron";
    case VTK_BEZIER_HEXAHEDRON:
      return "Bezier Hexahedron";
    case VTK_BEZIER_WEDGE:
      return "Bezier Wedge";
    case VTK_BEZIER_PYRAMID:
      return "Bezier Pyramid";
  }
  return "Unknown";
}
