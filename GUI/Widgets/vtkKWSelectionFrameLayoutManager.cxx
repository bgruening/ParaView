/*=========================================================================

Copyright (c) 1998-2003 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/

// we need all of windows.h
#define VTK_WINDOWS_FULL

#include "vtkKWSelectionFrameLayoutManager.h"

#include "vtkImageData.h"
#include "vtkKWApplication.h"
#include "vtkKWEntry.h"
#include "vtkKWEntryWithLabel.h"
#include "vtkKWInternationalization.h"
#include "vtkKWLabel.h"
#include "vtkKWMenu.h"
#include "vtkKWMenuButton.h"
#include "vtkKWMessageDialog.h"
#include "vtkKWRenderWidget.h"
#include "vtkKWSaveImageDialog.h"
#include "vtkKWSelectionFrame.h"
#include "vtkKWSimpleEntryDialog.h"
#include "vtkKWTkUtilities.h"
#include "vtkKWToolbar.h"
#include "vtkKWWindowBase.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkWindows.h"

// Readers / Writers

#include "vtkErrorCode.h"
#include "vtkBMPWriter.h"
#include "vtkJPEGWriter.h"
#include "vtkPNGWriter.h"
#include "vtkPNMWriter.h"
#include "vtkTIFFWriter.h"

#include "vtkWindowToImageFilter.h"
#include "vtkImageAppend.h"
#include "vtkImageConstantPad.h"

#include <vtksys/stl/vector>
#include <vtksys/stl/string>

#include "Resources/vtkKWWindowLayoutResources.h"

#define VTK_KW_SFLMGR_LABEL_PATTERN "%d x %d"
#define VTK_KW_SFLMGR_HELP_PATTERN "Set window layout to %d column(s) by %d row(s)"
#define VTK_KW_SFLMGR_ICON_PATTERN "KWWindowLayout%dx%d"
#define VTK_KW_SFLMGR_RESOLUTIONS {{ 1, 1}, { 1, 2}, { 2, 1}, { 2, 2}, { 2, 3}, { 3, 2}}
#define VTK_KW_SFLMGR_MAX_SIZE 100

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkKWSelectionFrameLayoutManager);
vtkCxxRevisionMacro(vtkKWSelectionFrameLayoutManager, "1.59");

//----------------------------------------------------------------------------
class vtkKWSelectionFrameLayoutManagerInternals
{
public:
  struct PoolNode
  {
    vtksys_stl::string Tag;
    vtksys_stl::string Group;
    vtkKWSelectionFrame *Widget;
    int Position[2];
  };

  typedef vtksys_stl::vector<PoolNode> PoolType;
  typedef vtksys_stl::vector<PoolNode>::iterator PoolIterator;

  int ResolutionBeforeMaximize[2];
  int SelectionPositionBeforeMaximize[2];

  PoolType Pool;
};

//----------------------------------------------------------------------------
vtkKWSelectionFrameLayoutManager::vtkKWSelectionFrameLayoutManager()
{
  this->Internals = new vtkKWSelectionFrameLayoutManagerInternals;

  this->Resolution[0] = 0;
  this->Resolution[1] = 0;

  this->ResolutionEntriesMenu    = NULL;
  this->ResolutionEntriesToolbar = NULL;

  this->SelectionChangedCommand = NULL;

  this->GetResolution(this->Internals->ResolutionBeforeMaximize);
  this->Internals->SelectionPositionBeforeMaximize[0] = 0;
  this->Internals->SelectionPositionBeforeMaximize[1] = 0;

  this->SetResolution(1, 1);
}

//----------------------------------------------------------------------------
vtkKWSelectionFrameLayoutManager::~vtkKWSelectionFrameLayoutManager()
{
  if (this->SelectionChangedCommand)
    {
    delete [] this->SelectionChangedCommand;
    this->SelectionChangedCommand = NULL;
    }

  // Remove all widgets

  this->RemoveAllWidgets();

  // Delete our pool

  delete this->Internals;

  // Delete the menu

  if (this->ResolutionEntriesMenu)
    {
    this->ResolutionEntriesMenu->Delete();
    this->ResolutionEntriesMenu = NULL;
    }

  // Delete the toolbar

  if (this->ResolutionEntriesToolbar)
    {
    this->ResolutionEntriesToolbar->Delete();
    this->ResolutionEntriesToolbar = NULL;
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::Create()
{
  // Check if already created

  if (this->IsCreated())
    {
    vtkErrorMacro(<< this->GetClassName() << " already created");
    return;
    }

  // Call the superclass to create the whole widget

  this->Superclass::Create();

  this->SetBackgroundColor(0.2, 0.2, 0.2);

  // Pack

  this->Pack();

  // Update enable state

  this->UpdateEnableState();
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::Pack()
{
  if (!this->IsAlive())
    {
    return;
    }

  // Unpack everything

  this->UnpackChildren();

  // Pack each widgets, column first

  ostrstream tk_cmd;
  int i, j;

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget)
      {
      this->CreateWidget(it->Widget);
      if (it->Widget->IsCreated())
        {
        if (it->Position[0] < this->Resolution[0] && 
            it->Position[1] < this->Resolution[1])
          {
          tk_cmd << "grid " << it->Widget->GetWidgetName() 
                 << " -sticky news "
                 << " -column " << it->Position[0] 
                 << " -row " << it->Position[1] << endl;
          }
        }
      }
    }

  // columns and rows can resize
  // Make sure we reset the columns/rows that are not used (even if we
  // unpacked the children, those settings are kept since they are set
  // on the master)

  int nb_of_cols = 10, nb_of_rows = 10;
  vtkKWTkUtilities::GetGridSize(this, &nb_of_cols, &nb_of_rows);

  for (j = 0; j < this->Resolution[1]; j++)
    {
    tk_cmd << "grid rowconfigure " << this->GetWidgetName() << " " << j 
           << " -weight 1" << endl;
    }
  for (j = this->Resolution[1]; j < nb_of_rows; j++)
    {
    tk_cmd << "grid rowconfigure " << this->GetWidgetName() << " " << j 
           << " -weight 0" << endl;
    }
  for (i = 0; i < this->Resolution[0]; i++)
    {
    tk_cmd << "grid columnconfigure " << this->GetWidgetName() << " " << i
           << " -weight 1" << endl;
    }
  for (i = this->Resolution[0]; i < nb_of_cols; i++)
    {
    tk_cmd << "grid columnconfigure " << this->GetWidgetName() << " " << i
           << " -weight 0" << endl;
    }
  
  tk_cmd << ends;
  this->Script(tk_cmd.str());
  tk_cmd.rdbuf()->freeze(0);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::SetWidgetPosition(
  vtkKWSelectionFrame *widget, int col, int row)
{
  if (widget)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && it->Widget == widget)
        {
        it->Position[0] = col;
        it->Position[1] = row;
        this->Pack();
        return 1;
        }
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::GetWidgetPosition(
  vtkKWSelectionFrame *widget, int *col, int *row)
{
  if (widget)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && it->Widget == widget)
        {
        *col = it->Position[0];
        *row = it->Position[1];
        return 1;
        }
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* 
vtkKWSelectionFrameLayoutManager::GetWidgetAtPosition(int col, int row)
{
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget && 
        it->Position[0] == col && it->Position[1] == row)
      {
      return it->Widget;
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::ReorganizeWidgetPositions()
{
  // Given the resolution, fill in the corresponding grid with 
  // widgets that have a valid position inside that grid

  vtksys_stl::vector<int> grid;
  grid.assign(this->Resolution[0] * this->Resolution[1], 0);

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget &&
        (it->Position[0] >= 0 && it->Position[0] < this->Resolution[0] && 
         it->Position[1] >= 0 && it->Position[1] < this->Resolution[1]))
      {
      grid[it->Position[1] * this->Resolution[0] + it->Position[0]] = 1;
      }
    }

  // Fill the holes in the grid with whatever widgets
  // which positions were out of the grid

  it = this->Internals->Pool.begin();
  int i, j;
  for (j = 0; j < this->Resolution[1] && it != end; j++)
    {
    for (i = 0; i < this->Resolution[0] && it != end; i++)
      {
      if (grid[j * this->Resolution[0] + i] == 0)
        {
        while (it != end)
          {
          if (it->Widget &&
              (it->Position[0] < 0 || it->Position[0] >= this->Resolution[0] ||
               it->Position[1] < 0 || it->Position[1] >= this->Resolution[1]))
            {
            it->Position[0] = i;
            it->Position[1] = j;
            ++it;
            break;
            }
          ++it;
          }
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::SetResolution(int i, int j)
{
  if (i < 0 || j < 0 || 
      (i == this->Resolution[0] && j == this->Resolution[1]))
    {
    return;
    }

  this->Resolution[0] = i;
  this->Resolution[1] = j;

  this->UpdateResolutionEntriesMenu();
  this->UpdateResolutionEntriesToolbar();

  this->ReorganizeWidgetPositions();

  this->Pack();
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::AdjustResolution()
{
  int i = this->Resolution[0];
  int j = this->Resolution[1];

  int pool_size = (int)this->Internals->Pool.size();

  // Increase the resolution so that all widgets can potentially be shown
  // If there is the same number of row/column, add a row first

  while (pool_size && 
         (i * j) < pool_size)
    {
    if (i < j)
      {
      i++;
      }
    else
      {
      j++;
      }
    }

  // Decrease the resolution so that all widgets can potentially be shown
  // without extra columns or holes.
  // If there is the same number of row/column, remove a row first

  while (pool_size &&
         (pool_size <= ((i - 1) * j)  ||
          pool_size <= (i * (j - 1))))
    {
    if (i > j)
      {
      i--;
      }
    else
      {
      j--;
      }
    }

  this->SetResolution(i, j);
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::CreateResolutionEntriesMenu(
  vtkKWMenu *parent)
{
  if (!parent)
    {
    return;
    }

  if (!this->ResolutionEntriesMenu)
    {
    this->ResolutionEntriesMenu = vtkKWMenu::New();
    }

  if (!this->ResolutionEntriesMenu->IsCreated())
    {
    this->ResolutionEntriesMenu->SetParent(parent);
    this->ResolutionEntriesMenu->Create();
    }

  // Allowed resolutions

  vtksys_stl::string varname(this->GetWidgetName());
  varname += "reschoice";

  char label[64], command[128], help[128];  

  int res[][2] = VTK_KW_SFLMGR_RESOLUTIONS;
  for (size_t idx = 0; idx < sizeof(res) / sizeof(res[0]); idx++)
    {
    sprintf(label, VTK_KW_SFLMGR_LABEL_PATTERN, 
            res[idx][0], res[idx][1]);
    sprintf(command, "ResolutionCallback %d %d", res[idx][0], res[idx][1]);
    sprintf(help, VTK_KW_SFLMGR_HELP_PATTERN, 
            res[idx][0], res[idx][1]);
    int value = 
      ((res[idx][0] - 1) * VTK_KW_SFLMGR_MAX_SIZE + res[idx][1] - 1);
    int index = this->ResolutionEntriesMenu->AddRadioButton(
      label, this, command);
    this->ResolutionEntriesMenu->SetItemVariable(
      index, varname.c_str());
    this->ResolutionEntriesMenu->SetItemSelectedValueAsInt(index, value);
    this->ResolutionEntriesMenu->SetItemHelpString(index, help);
    }

  this->UpdateResolutionEntriesMenu();
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::UpdateResolutionEntriesMenu()
{
  if (!this->ResolutionEntriesMenu ||
      !this->ResolutionEntriesMenu->IsCreated())
    {
    return;
    }

  // Enabled/Disabled some resolutions

  int normal_state = 
    this->GetEnabled() ? vtkKWTkOptions::StateNormal : vtkKWTkOptions::StateDisabled;
  size_t size = this->Internals->Pool.size();

  char label[64];

  int res[][2] = VTK_KW_SFLMGR_RESOLUTIONS;
  for (size_t idx = 0; idx < sizeof(res) / sizeof(res[0]); idx++)
    {
    sprintf(label, VTK_KW_SFLMGR_LABEL_PATTERN, res[idx][0], res[idx][1]);
    this->ResolutionEntriesMenu->SetItemState(
      label, 
      (size_t)(res[idx][0] * res[idx][1]) <= 
      (size + (res[idx][0] != 1 && res[idx][1] != 1 ? 1 : 0))
      ? normal_state : vtkKWTkOptions::StateDisabled);
    }

  // Select the right one

  int value = 
    (this->Resolution[0]-1) * VTK_KW_SFLMGR_MAX_SIZE + this->Resolution[1]-1;

  vtksys_stl::string rbv(this->GetWidgetName());
  rbv += "reschoice";
  if (atoi(this->Script("set %s", rbv.c_str())) != value)
    {
    this->Script("set %s %d", rbv.c_str(), value);
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::CreateResolutionEntriesToolbar(
  vtkKWWidget *parent)
{
  if (!parent)
    {
    return;
    }

  if (!this->ResolutionEntriesToolbar)
    {
    this->ResolutionEntriesToolbar = vtkKWToolbar::New();
    this->ResolutionEntriesToolbar->SetName(
      ks_("Toolbar|Window Layout"));
    }

  if (!this->ResolutionEntriesToolbar->IsCreated())
    {
    this->ResolutionEntriesToolbar->SetParent(parent);
    this->ResolutionEntriesToolbar->Create();
    }

  // Got to create the icons

  vtkKWTkUtilities::UpdateOrLoadPhoto(
    parent->GetApplication(),
    "KWWindowLayout1x1",
    NULL,
    NULL,
    image_KWWindowLayout1x1, 
    image_KWWindowLayout1x1_width, 
    image_KWWindowLayout1x1_height,
    image_KWWindowLayout1x1_pixel_size,
    image_KWWindowLayout1x1_length);

  vtkKWTkUtilities::UpdateOrLoadPhoto(
    parent->GetApplication(),
    "KWWindowLayout1x2",
    NULL,
    NULL,
    image_KWWindowLayout1x2, 
    image_KWWindowLayout1x2_width, 
    image_KWWindowLayout1x2_height,
    image_KWWindowLayout1x2_pixel_size,
    image_KWWindowLayout1x2_length);

  vtkKWTkUtilities::UpdateOrLoadPhoto(
    parent->GetApplication(),
    "KWWindowLayout2x1",
    NULL,
    NULL,
    image_KWWindowLayout2x1, 
    image_KWWindowLayout2x1_width, 
    image_KWWindowLayout2x1_height,
    image_KWWindowLayout2x1_pixel_size,
    image_KWWindowLayout2x1_length);

  vtkKWTkUtilities::UpdateOrLoadPhoto(
    parent->GetApplication(),
    "KWWindowLayout2x2",
    NULL,
    NULL,
    image_KWWindowLayout2x2, 
    image_KWWindowLayout2x2_width, 
    image_KWWindowLayout2x2_height,
    image_KWWindowLayout2x2_pixel_size,
    image_KWWindowLayout2x2_length);

  vtkKWTkUtilities::UpdateOrLoadPhoto(
    parent->GetApplication(),
    "KWWindowLayout2x3",
    NULL,
    NULL,
    image_KWWindowLayout2x3, 
    image_KWWindowLayout2x3_width, 
    image_KWWindowLayout2x3_height,
    image_KWWindowLayout2x3_pixel_size,
    image_KWWindowLayout2x3_length);

  vtkKWTkUtilities::UpdateOrLoadPhoto(
    parent->GetApplication(),
    "KWWindowLayout3x2",
    NULL,
    NULL,
    image_KWWindowLayout3x2, 
    image_KWWindowLayout3x2_width, 
    image_KWWindowLayout3x2_height,
    image_KWWindowLayout3x2_pixel_size,
    image_KWWindowLayout3x2_length);

  // Allowed resolutions

  vtksys_stl::string rbv(this->GetWidgetName());
  rbv += "reschoice";

  char command[128], help[128], icon[128];  

  int res[][2] = VTK_KW_SFLMGR_RESOLUTIONS;
  for (size_t idx = 0; idx < sizeof(res) / sizeof(res[0]); idx++)
    {
    sprintf(command, "ResolutionCallback %d %d", 
            res[idx][0], res[idx][1]);
    sprintf(help, VTK_KW_SFLMGR_HELP_PATTERN, 
            res[idx][0], res[idx][1]);
    sprintf(icon, VTK_KW_SFLMGR_ICON_PATTERN, 
            res[idx][0], res[idx][1]);
    int value = 
      ((res[idx][0] - 1) * VTK_KW_SFLMGR_MAX_SIZE + res[idx][1] - 1);
    this->ResolutionEntriesToolbar->AddRadioButtonImage(
      value, icon, icon, rbv.c_str(), this, command, help);
    }

  this->UpdateResolutionEntriesToolbar();
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::UpdateResolutionEntriesToolbar()
{
  if (!this->ResolutionEntriesToolbar ||
      !this->ResolutionEntriesToolbar->IsCreated())
    {
    return;
    }

  // Enabled/Disabled some resolutions

  size_t size = this->Internals->Pool.size();
  char icon[128];

  int res[][2] = VTK_KW_SFLMGR_RESOLUTIONS;
  for (size_t idx = 0; idx < sizeof(res) / sizeof(res[0]); idx++)
    {
    sprintf(icon, VTK_KW_SFLMGR_ICON_PATTERN, res[idx][0], res[idx][1]);
    vtkKWWidget *w = this->ResolutionEntriesToolbar->GetWidget(icon);
    if (w)
      {
      w->SetEnabled(
        (size_t)(res[idx][0] * res[idx][1]) <= 
        (size + (res[idx][0] != 1 && res[idx][1] != 1 ? 1 : 0)) 
        ? this->GetEnabled() : 0);
      }
    }

  // Select the right one

  int value = 
    (this->Resolution[0]-1) * VTK_KW_SFLMGR_MAX_SIZE + this->Resolution[1]-1;

  vtksys_stl::string rbv(this->GetWidgetName());
  rbv += "reschoice";
  if (atoi(this->Script("set %s", rbv.c_str())) != value)
    {
    this->Script("set %s %d", rbv.c_str(), value);
    }
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::GetWidgetWithTag(
  const char *tag)
{
  if (tag && *tag)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && !it->Tag.compare(tag))
        {
        return it->Widget;
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* 
vtkKWSelectionFrameLayoutManager::GetWidgetWithTagAndGroup(
  const char *tag, const char *group)
{
  if (tag && *tag && group && *group)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && !it->Tag.compare(tag)  && !it->Group.compare(group))
        {
        return it->Widget;
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::GetNthWidget(
  int index)
{
  if (index < 0 || index >= (int)this->Internals->Pool.size())
    {
    return NULL;
    }

#if 0  
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  while (index != 0)
    {
    ++it;
    index--;
    }
  return it->Widget;
#else
  return this->Internals->Pool[index].Widget;
#endif
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::GetNthWidgetNotMatching(
  int index, vtkKWSelectionFrame *avoid)
{
  if (index >= 0)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && it->Widget != avoid)
        {
        index--;
        if (index < 0)
          {
          return it->Widget;
          }
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::GetNthWidgetWithGroup(
  int index, const char *group)
{
  if (index >= 0 && group && *group)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && !it->Group.compare(group))
        {
        index--;
        if (index < 0)
          {
          return it->Widget;
          }
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::GetWidgetWithTitle(
  const char *title)
{
  if (title)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && 
          it->Widget->GetTitle() && 
          !strcmp(title, it->Widget->GetTitle()))
        {
        return it->Widget;
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::GetNumberOfWidgets()
{
  return this->Internals->Pool.size();
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::GetNumberOfWidgetsWithTag(
  const char *tag)
{
  int count = 0;
  if (tag && *tag)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && !it->Tag.compare(tag))
        {
        count++;
        }
      }
    }

  return count;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::GetNumberOfWidgetsWithGroup(
  const char *group)
{
  int count = 0;
  if (group && *group)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget && !it->Group.compare(group))
        {
        count++;
        }
      }
    }

  return count;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::HasWidget(
  vtkKWSelectionFrame *widget)
{
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget && it->Widget == widget)
      {
      return 1;
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::HasWidgetWithTag(const char *tag)
{
  return this->GetWidgetWithTag(tag) ? 1 : 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::HasWidgetWithTagAndGroup(
  const char *tag, const char *group)
{
  return this->GetWidgetWithTagAndGroup(tag, group) ? 1 : 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::AddWidget(
  vtkKWSelectionFrame *widget)
{
  if (!widget)
    {
    return 0;
    }

  // If we have that widget already

  if (this->HasWidget(widget))
    {
    return 0;
    }

  // Create a new node

  vtkKWSelectionFrameLayoutManagerInternals::PoolNode node;
  node.Widget = widget;
  node.Widget->Register(this);

  // Create the widget (if needed), configure the callbacks

  if (!node.Widget->IsCreated())
    {
    this->CreateWidget(node.Widget);
    }
  else
    {
    this->ConfigureWidget(node.Widget);
    }

  // Unitialize its position. It will be updated automatically the first
  // time this widget is packed.

  node.Position[0] = node.Position[1] = -1;

  // Add it to the pool

  this->Internals->Pool.push_back(node);

  this->NumberOfWidgetsHasChanged();

  // If we just added a widget, and there was nothing else before, let's
  // select it for convenience purposes

  if (this->GetNumberOfWidgets() == 1 && !this->GetSelectedWidget())
    {
    this->SelectWidget(this->GetNthWidget(0));
    }

  return 1;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::AllocateAndAddWidget()
{
  // Allocate a widget and add it

  vtkKWSelectionFrame *widget = this->AllocateWidget();
  if (widget)
    {
    int ok =  this->AddWidget(widget); // this will Register() the widget
    widget->Delete();
    if (!ok)
      {
      widget = NULL;
      }
    }

  return widget;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::AllocateWidget()
{
  vtkKWSelectionFrame *widget = vtkKWSelectionFrame::New();
  widget->AllowChangeTitleOn();
  widget->AllowCloseOn();
  return widget;
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::CreateWidget(
  vtkKWSelectionFrame *widget)
{
  if (this->IsCreated() && widget && !widget->IsCreated())
    {
    widget->SetParent(this);
    widget->Create();
    widget->SetWidth(350);
    widget->SetHeight(350);
    this->ConfigureWidget(widget);
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::ConfigureWidget(
  vtkKWSelectionFrame *widget)
{
  this->PropagateEnableState(widget);
  this->AddCallbacksToWidget(widget);
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::AddCallbacksToWidget(
  vtkKWSelectionFrame *widget)
{
  if (widget)
    {
    widget->SetCloseCommand(this, "CloseWidgetCallback");
    widget->SetTitleChangedCommand(this, "WidgetTitleChangedCallback");
    widget->SetChangeTitleCommand(this, "ChangeWidgetTitleCallback");
    widget->SetSelectCommand(this, "SelectWidgetCallback");
    widget->SetDoubleClickCommand(this, "SelectAndMaximizeWidgetCallback");
    widget->SetSelectionListCommand(this, "SwitchWidgetCallback");
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::RemoveCallbacksFromWidget(
  vtkKWSelectionFrame *widget)
{
  if (widget)
    {
    widget->SetCloseCommand(NULL, NULL);
    widget->SetChangeTitleCommand(NULL, NULL);
    widget->SetSelectCommand(NULL, NULL);
    widget->SetDoubleClickCommand(NULL, NULL);
    widget->SetSelectionListCommand(NULL, NULL);
    }
}

//----------------------------------------------------------------------------
vtkKWRenderWidget* vtkKWSelectionFrameLayoutManager::GetVisibleRenderWidget(
  vtkKWSelectionFrame *widget)
{
  vtkKWRenderWidget *rw = NULL;
  if (widget)
    {
    vtkKWFrame *frame = widget->GetBodyFrame();
    if (frame)
      {
      int nb_children = frame->GetNumberOfChildren();
      for (int i = 0; i < nb_children; i++)
        {
        vtkKWWidget *child = frame->GetNthChild(i);
        if (child)
          {
          rw = vtkKWRenderWidget::SafeDownCast(child);
          if (rw)
            {
            return rw;
            }
          int nb_grand_children = child->GetNumberOfChildren();
          for (int j = 0; j < nb_grand_children; j++)
            {
            vtkKWWidget *grand_child = child->GetNthChild(j);
            if (grand_child)
              {
              rw = vtkKWRenderWidget::SafeDownCast(grand_child);
              if (rw)
                {
                return rw;
                }
              }
            }
          }
        }
      }
    }
  return rw;
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::NumberOfWidgetsHasChanged()
{
  // Update all selection lists, so that this new widget can be selected

  this->UpdateSelectionLists();

  // Adjust the resolution

  this->AdjustResolution();
  this->ReorganizeWidgetPositions();
  this->UpdateEnableState();

  // Pack

  this->Pack();
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::DeleteWidget(
  vtkKWSelectionFrame *widget)
{
  if (widget)
    {
    this->RemoveCallbacksFromWidget(widget);
    widget->Close();
    widget->UnRegister(this);
    }
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::RemoveWidget(
  vtkKWSelectionFrame *widget)
{
  if (this->Internals && widget)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget == widget)
        {
        // If we are removing the selectiong, make sure we select another one
        // instead
        vtkKWSelectionFrame *sel = this->GetSelectedWidget();
        this->Internals->Pool.erase(it);
        if (sel == widget)
          {
          this->SelectWidget(this->GetNthWidget(0));
          }
        this->DeleteWidget(widget);
        this->NumberOfWidgetsHasChanged();
        return 1;
        }
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::RemoveAllWidgets()
{
  // Is faster than calling RemoveWidget on each widget
  // since the selection is set to NULL first, and no callbacks is going
  // to be invoked until every widget is cleared.

  if (this->Internals)
    {
    this->SelectWidget((vtkKWSelectionFrame*)NULL);

    int nb_deleted = 0;
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget)
        {
        this->DeleteWidget(it->Widget);
        nb_deleted++;
        }
      }
    
    this->Internals->Pool.clear();
    if (nb_deleted)
      {
      this->NumberOfWidgetsHasChanged();
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::RemoveAllWidgetsWithGroup(
  const char *group)
{
  // Is faster than calling RemoveWidget on each widget
  // since the selection is saved first, and no callbacks is going
  // to be invoked until every widget is cleared.

  if (this->Internals && group && *group)
    {
    vtkKWSelectionFrame *sel = this->GetSelectedWidget();
    
    int nb_deleted = 0;
    int done = 0;
    while (!done)
      {
      done = 1;
      vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
        this->Internals->Pool.begin();
      vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
        this->Internals->Pool.end();
      for (; it != end; ++it)
        {
        if (it->Widget && !it->Group.compare(group))
          {
          vtkKWSelectionFrame *widget = it->Widget;
          this->Internals->Pool.erase(it);
          this->DeleteWidget(widget);
          nb_deleted++;
          done = 0;
          break;
          }
        }
      }
    
    if (nb_deleted)
      {
      if (!this->HasWidget(sel))
        {
        this->SelectWidget(this->GetNthWidget(0));
        }
      this->NumberOfWidgetsHasChanged();
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::SetWidgetTag(
  vtkKWSelectionFrame *widget, 
  const char *tag)
{
  // Valid tag ?

  if (!widget || !tag || !*tag)
    {
    return 0;
    }

  // OK, tag it

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget && it->Widget == widget)
      {
      it->Tag = tag;
      return 1;
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
const char* vtkKWSelectionFrameLayoutManager::GetWidgetTag(
  vtkKWSelectionFrame *widget)
{
  if (widget)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget == widget)
        {
        return it->Tag.c_str();
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::SetWidgetGroup(
  vtkKWSelectionFrame *widget, 
  const char *group)
{
  // Valid group ?

  if (!widget || !group || !*group)
    {
    return 0;
    }

  // OK, group it

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget && it->Widget == widget)
      {
      it->Group = group;
      return 1;
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
const char* vtkKWSelectionFrameLayoutManager::GetWidgetGroup(
  vtkKWSelectionFrame *widget)
{
  if (widget)
    {
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
      this->Internals->Pool.begin();
    vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
      this->Internals->Pool.end();
    for (; it != end; ++it)
      {
      if (it->Widget == widget)
        {
        return it->Group.c_str();
        }
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::ShowWidgetsWithGroup(const char *group)
{
  if (!group || !*group)
    {
    return 0;
    }

  int nb_widgets_in_group = this->GetNumberOfWidgetsWithGroup(group);
  int row, col, i;

  int sel_row, sel_col;
  vtkKWSelectionFrame *old_selection = this->GetSelectedWidget();
  if (old_selection)
    {
    this->GetWidgetPosition(old_selection, &sel_col, &sel_row);
    }

  // Inspect all selection frame, and check if they already display the group
  // we want to make visible

  for (row = 0; row < this->Resolution[1]; row++)
    {
    for (col = 0; col < this->Resolution[0]; col++)
      {
      vtkKWSelectionFrame *widget = this->GetWidgetAtPosition(col, row);
      if (widget)
        {
        const char *widget_group = this->GetWidgetGroup(widget);
        if (widget_group && strcmp(widget_group, group))
          {
          // The selection frame is not the right group, look for another one
          // with the right group, and exchange both

          for (i = 0; i < nb_widgets_in_group; i++)
            {
            vtkKWSelectionFrame *new_widget = 
              this->GetNthWidgetWithGroup(i, group);
            if (new_widget)
              {
              int new_row, new_col;
              this->GetWidgetPosition(new_widget, &new_col, &new_row);
              if (new_col < 0 || new_row < 0 || 
                  new_row > row || (new_row == row && new_col > col))
                {
                this->SetWidgetPosition(new_widget, col, row);
                this->SetWidgetPosition(widget, new_col, new_row);
                break;
                }
              }
            }
          }
        }
      }
    }

  // Restore the selection

  if (old_selection)
    {
    vtkKWSelectionFrame *atpos = this->GetWidgetAtPosition(sel_col, sel_row);
    if (atpos && atpos != old_selection)
      {
      this->SelectWidget(atpos);
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
vtkKWSelectionFrame* vtkKWSelectionFrameLayoutManager::GetSelectedWidget()
{
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget && it->Widget->GetSelected())
      {
      return it->Widget;
      }
    }

  return NULL;
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::SelectWidget(
  vtkKWSelectionFrame *widget)
{
  // Deselect all widgets and select the right one (if any)

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    if (it->Widget && it->Widget != widget)
      {
      it->Widget->SelectedOff();
      }
    }
  if (widget)
    {
    widget->SelectedOn();

    this->InvokeSelectionChangedCommand();
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::SetSelectionChangedCommand(
  vtkObject *object, const char *method)
{
  this->SetObjectMethodCommand(&this->SelectionChangedCommand, object, method);
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::InvokeSelectionChangedCommand()
{
  this->InvokeObjectMethodCommand(this->SelectionChangedCommand);
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::SelectWidgetCallback(
  vtkKWSelectionFrame *selection)
{
  this->SelectWidget(selection);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::IsWidgetMaximized(
  vtkKWSelectionFrame *widget)
{
  int col, row;
  return (widget && 
          this->GetWidgetPosition(widget, &col, &row) &&
          col == 0 && row == 0 && 
          this->Resolution[0] == 1 && this->Resolution[1] == 1);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::MaximizeWidget(
  vtkKWSelectionFrame *widget)
{
  // Save the resolution and the position so that both can
  // be restored on UndoMaximize

  if (widget && this->Internals &&
      this->GetWidgetPosition(
        widget, 
        this->Internals->SelectionPositionBeforeMaximize))
    {
    this->GetResolution(this->Internals->ResolutionBeforeMaximize);

    // Set the resolution to full (1, 1)
    // then switch whichever widget was at [0, 0] with our widget

    this->SetResolution(1, 1);
    
    vtkKWSelectionFrame *at00 = this->GetWidgetAtPosition(0, 0);
    if (at00)
      {
      this->SwitchWidgetsPosition(widget, at00);
      }
    else
      {
      this->SetWidgetPosition(widget, 0, 0);
      }

    return 1;
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::UndoMaximizeWidget()
{
  if (this->Resolution[0] == 1 && this->Resolution[1] == 1 && this->Internals)
    {
    const int nw = this->GetNumberOfWidgets();
    
    // If we have only one widget, do not go back (to having nothing)
    if ( nw == 1 )
      {
      return 0;
      }

    // And if the previous resolution is 0 and we have more than one
    // widget (yes this case occurs), estimate a sensible resolution.
    if ( this->Internals->ResolutionBeforeMaximize[0] == 0 && 
         this->Internals->ResolutionBeforeMaximize[1] == 0 &&
         nw > 1 )
      {
      if (nw > 6)
        {
        this->SetResolution( 2, 3 );
        }
      else if (nw >= 4)
        {
        this->SetResolution( 2, 2 );
        }
      else if (nw >= 2)
        { 
        this->SetResolution( 2, 1 );
        }
      else
        {
        this->SetResolution( 1, 1 );
        }
      return 1;
      }
    
    this->SetResolution(this->Internals->ResolutionBeforeMaximize);

    vtkKWSelectionFrame *at00 = this->GetWidgetAtPosition(0, 0);
    vtkKWSelectionFrame *atpos = this->GetWidgetAtPosition(
      this->Internals->SelectionPositionBeforeMaximize);
    if (atpos && at00)
      {
      this->SwitchWidgetsPosition(at00, atpos);
      }
    else if (at00)
      {
      this->SetWidgetPosition(
        at00, this->Internals->SelectionPositionBeforeMaximize);
      }
    else if (atpos)
      {
      this->SetWidgetPosition(atpos, 0, 0);
      }
    return 1;
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::ToggleMaximizeWidget(
  vtkKWSelectionFrame *widget)
{
  if (this->IsWidgetMaximized(widget))
    {
    return this->UndoMaximizeWidget();
    }
  else
    {
    return this->MaximizeWidget(widget);
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::SelectAndMaximizeWidgetCallback(
  vtkKWSelectionFrame *selection)
{
  this->SelectWidget(selection);
  this->ToggleMaximizeWidget(selection);
}

//---------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::SwitchWidgetsPosition(
  vtkKWSelectionFrame *w1, vtkKWSelectionFrame *w2)
{
  if (!w1 || !w2 || w1 == w2)
    {
    return 0;
    }

  int pos1[2], pos2[2];
  if (!this->GetWidgetPosition(w1, pos1) ||
      !this->GetWidgetPosition(w2, pos2))
    {
    return 0;
    }
  
  this->SetWidgetPosition(w1, pos2);
  this->SetWidgetPosition(w2, pos1);
  
  return 1;
}

//---------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::SwitchWidgetCallback(
  const char *title, vtkKWSelectionFrame *widget)
{
  // Get the widget we want to see in place of the current
  // widget

  vtkKWSelectionFrame *new_widget = this->GetWidgetWithTitle(title);
  if (!new_widget || new_widget == widget)
    {
    return;
    }

  // Switch both

  this->SwitchWidgetsPosition(widget, new_widget);

  // Select the new one

  new_widget->SelectCallback();

  // Make sure each selection list is updated to point at the right title
  // (since this callback was most likely triggered by selecting a 
  // *different* title in the list
  
  if (widget->GetSelectionList() && widget->GetTitle())
    {
    widget->GetSelectionList()->SetValue(widget->GetTitle());
    }
  if (new_widget->GetSelectionList() && new_widget->GetTitle())
    {
    new_widget->GetSelectionList()->SetValue(new_widget->GetTitle());
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::CloseWidgetCallback(
  vtkKWSelectionFrame *widget)
{
  this->RemoveWidget(widget);
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::WidgetTitleChangedCallback(
  vtkKWSelectionFrame *)
{
  this->UpdateSelectionLists();
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::ChangeWidgetTitleCallback(
  vtkKWSelectionFrame *widget)
{
  if (!widget)
    {
    return 0;
    }

  // Create a dialog to ask for a new title

  vtkKWSimpleEntryDialog *dlg = vtkKWSimpleEntryDialog::New();
  dlg->SetMasterWindow(this->GetParentTopLevel());
  dlg->SetDisplayPositionToPointer();
  dlg->SetTitle(
    ks_("Selection Frame Dialog|Title|Change frame title"));
  dlg->SetStyleToOkCancel();
  dlg->Create();
  dlg->GetEntry()->GetLabel()->SetText("Name:");
  dlg->SetText(
    ks_("Selection Frame Dialog|Enter a new title for this frame"));

  int ok = dlg->Invoke();
  if (ok)
    {
    vtksys_stl::string new_title(dlg->GetEntry()->GetWidget()->GetValue());
    ok = this->CanWidgetTitleBeChanged(widget, new_title.c_str());
    if (!ok)
      {
      vtkKWMessageDialog::PopupMessage(
        this->GetApplication(), this->GetParentTopLevel(), 
        ks_("Selection Frame Dialog|Title|Change frame title - Error!"),
        ks_("Selection Frame Dialog|There is a problem with the new title you provided."),
        vtkKWMessageDialog::ErrorIcon);
      }
    else
      {
      widget->SetTitle(new_title.c_str());
      this->UpdateSelectionLists();
      }
    }

  dlg->Delete();
  return ok; 
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::CanWidgetTitleBeChanged(
    vtkKWSelectionFrame *widget, const char *new_title)
{
  return (widget && 
          new_title && 
          *new_title && 
          (!widget->GetTitle() || strcmp(widget->GetTitle(), new_title)));
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::ResolutionCallback(int i, int j)
{
  int old_i, old_j;
  this->GetResolution(old_i, old_j);
  this->SetResolution(i, j);
  this->GetResolution(i, j);
  if (i != old_i || j != old_j)
    {
    this->InvokeEvent(
      vtkKWSelectionFrameLayoutManager::ResolutionChangedEvent, 
      NULL);
    }
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::UpdateEnableState()
{
  this->Superclass::UpdateEnableState();

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = 
    this->Internals->Pool.begin();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  for (; it != end; ++it)
    {
    this->PropagateEnableState(it->Widget);
    }

  this->PropagateEnableState(this->ResolutionEntriesMenu);
  this->PropagateEnableState(this->ResolutionEntriesToolbar);

  // Enable/Disable some entries

  this->UpdateResolutionEntriesMenu();
  this->UpdateResolutionEntriesToolbar();
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::UpdateSelectionLists()
{
  if (!this->Internals ||
      !this->Internals->Pool.size())
    {
    return;
    }
  
  // Allocate array of titles
  // Separate each group

  const char **titles_list = 
    new const char *[this->Internals->Pool.size() * 2];

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator end = 
    this->Internals->Pool.end();
  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator begin = 
    this->Internals->Pool.begin();

  int nb_titles = 0;
  const char *separator = "--";
  const char *prev_group = (begin != end ? begin->Group.c_str() : NULL);

  vtkKWSelectionFrameLayoutManagerInternals::PoolIterator it = begin;
  for (; it != end; ++it)
    {
    if (it->Widget && it->Widget->GetTitle())
      {
      if (strcmp(it->Group.c_str(), prev_group))
        {
        titles_list[nb_titles++] = separator;
        prev_group = it->Group.c_str();
        }
      titles_list[nb_titles++] = it->Widget->GetTitle();
      }
    }

  it = begin;
  for (; it != end; ++it)
    {
    if (it->Widget)
      {
      it->Widget->SetSelectionList(nb_titles, titles_list);
      if (it->Widget->GetSelectionList() && it->Widget->GetTitle())
        {
        it->Widget->GetSelectionList()->SetValue(it->Widget->GetTitle());
        }
      }
    }

  // Free titles

  delete [] titles_list;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::AppendWidgetsToImageData(
  vtkImageData *image, int selection_only, int direct, 
  int ForceUpdateOnScreenRendering)
{
  int nb_slots = this->Resolution[0] * this->Resolution[1];

  // We need a window to image filter for each widget in the grid

  vtksys_stl::vector<vtkWindowToImageFilter*> w2i_filters;
  w2i_filters.assign(nb_slots, (vtkWindowToImageFilter*)NULL);

  // We also need a pad filter to add a small margin for each widget 

  vtksys_stl::vector<vtkImageConstantPad*> pad_filters;
  pad_filters.assign(nb_slots, (vtkImageConstantPad*)NULL);

  // We need an append filter for each row in the grid, to append
  // widgets horizontally

  vtksys_stl::vector<vtkImageAppend*> append_filters;
  append_filters.assign(this->Resolution[1], (vtkImageAppend*)NULL);

  // We need an append filter to append each rows (see above) and form
  // the final picture

  vtkImageAppend *append_all = vtkImageAppend::New();
  append_all->SetAppendAxis(1);

  int spacing = 4;

  // Build the whole pipeline

  int i, j;
  for (j = this->Resolution[1] - 1; j >= 0; j--)
    {
    append_filters[j] = vtkImageAppend::New();
    append_filters[j]->SetAppendAxis(0);
    for (i = 0; i < this->Resolution[0]; i++)
      {
      int pos[2]; pos[0] = i; pos[1] = j;
      vtkKWSelectionFrame *widget = this->GetWidgetAtPosition(pos);
      if (widget && (!selection_only || widget->GetSelected()))
        {
        vtkKWRenderWidget *rwwidget = this->GetVisibleRenderWidget(widget);
        if (rwwidget)
          {
          int idx = j * this->Resolution[0] + i;
          w2i_filters[idx] = vtkWindowToImageFilter::New();
          int offscreen = rwwidget->GetOffScreenRendering();
          if (direct)
            {
            if (!ForceUpdateOnScreenRendering) // true by default.
              {
              w2i_filters[idx]->ShouldRerenderOff();
              }
            }
          else
            {
            rwwidget->SetOffScreenRendering(1);
            }
          w2i_filters[idx]->SetInput(rwwidget->GetRenderWindow());
          w2i_filters[idx]->Update();
          rwwidget->SetOffScreenRendering(offscreen);

          int ext[6];
          w2i_filters[idx]->GetOutput()->GetWholeExtent(ext);
          pad_filters[idx] = vtkImageConstantPad::New();
          pad_filters[idx]->SetInput(w2i_filters[idx]->GetOutput());
          pad_filters[idx]->SetConstant(255);
          pad_filters[idx]->SetOutputWholeExtent(
            ext[0] - spacing, ext[1] + spacing,
            ext[2] - spacing, ext[3] + spacing,
            ext[4], ext[5]);
          pad_filters[idx]->Update();

          append_filters[j]->AddInput(pad_filters[idx]->GetOutput());
          }
        }
      }

    if (append_filters[j]->GetNumberOfInputConnections(0))
      {
      append_all->AddInput(append_filters[j]->GetOutput());
      append_filters[j]->Update();
      }
    }

  // Create the final output

  if (append_all->GetNumberOfInputConnections(0))
    {
    append_all->Update();
    image->ShallowCopy(append_all->GetOutput());
    }

  // Deallocate

  append_all->Delete();
  
  for (j = 0; j < this->Resolution[1]; j++)
    {
    append_filters[j]->Delete();
    for (i = 0; i < this->Resolution[0]; i++)
      {
      int pos[2]; pos[0] = i; pos[1] = j;
      vtkKWSelectionFrame *widget = this->GetWidgetAtPosition(pos);
      if (widget && (!selection_only || widget->GetSelected()))
        {
        vtkKWRenderWidget *rwwidget = this->GetVisibleRenderWidget(widget);
        if (rwwidget && !direct)
          {
          rwwidget->Render();
          }
        }
      int idx = j * this->Resolution[0] + i;
      if (w2i_filters[idx])
        {
        w2i_filters[idx]->Delete();
        }
      if (pad_filters[idx])
        {
        pad_filters[idx]->Delete();
        }
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::AppendAllWidgetsToImageData(
  vtkImageData *image, int OnScreenRendering)
{
  if (OnScreenRendering)
    {
    return this->AppendWidgetsToImageData(image, 0, 1, OnScreenRendering);
    }
  return this->AppendWidgetsToImageData(image, 0, 0, OnScreenRendering);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::AppendAllWidgetsToImageDataFast(
  vtkImageData *image)
{
  return this->AppendWidgetsToImageData(image, 0, 1);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::AppendSelectedWidgetToImageData(
  vtkImageData *image)
{
  return this->AppendWidgetsToImageData(image, 1, 0);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::AppendSelectedWidgetToImageDataFast(
  vtkImageData *image)
{
  return this->AppendWidgetsToImageData(image, 1, 1);
}

//---------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::SaveScreenshotAllWidgets()
{
  if (!this->IsCreated())
    {
    return 0;
    }

  vtkKWSaveImageDialog *save_dialog = vtkKWSaveImageDialog::New();
  save_dialog->SetParent(this->GetParentTopLevel());
  save_dialog->Create();
  save_dialog->SetTitle(
    ks_("Selection Frame Dialog|Title|Save Screenshot"));
  save_dialog->RetrieveLastPathFromRegistry("SavePath");
  
  int res = 0;
  if (save_dialog->Invoke() && 
      this->SaveScreenshotAllWidgetsToFile(save_dialog->GetFileName()))
    {
    save_dialog->SaveLastPathToRegistry("SavePath");
    res = 1;
    }

  save_dialog->Delete();

  return res;
}

//---------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::SaveScreenshotAllWidgetsToFile(
  const char* fname)
{
  if (!fname)
    {
    return 0;
    }

  // Append all widgets to an image

  vtkImageData *iData = vtkImageData::New();
  if (!this->AppendAllWidgetsToImageData(iData))
    {
    iData->Delete();
    return 0;
    }

  int extent[6];
  iData->GetExtent(extent);
  if (extent[0] > extent[1] && extent[2] > extent[3] && extent[4] > extent[5])
    {
    iData->Delete();
    return 0;
    }

  // Now save it

  const char *ext = fname + strlen(fname) - 4;
  
  int success = 1;

  if (!strcmp(ext, ".bmp"))
    {
    vtkBMPWriter *bmp = vtkBMPWriter::New();
    bmp->SetInput(iData);
    bmp->SetFileName(fname);
    bmp->Write();
    if (bmp->GetErrorCode() == vtkErrorCode::OutOfDiskSpaceError)
      {
      success = 0;
      }
    bmp->Delete();
    }
  else if (!strcmp(ext, ".tif"))
    {
    vtkTIFFWriter *tif = vtkTIFFWriter::New();
    tif->SetInput(iData);
    tif->SetFileName(fname);
    tif->Write();
    if (tif->GetErrorCode() == vtkErrorCode::OutOfDiskSpaceError)
      {
      success = 0;
      }
    tif->Delete();
    }
  else if (!strcmp(ext, ".ppm"))
    {
    vtkPNMWriter *pnm = vtkPNMWriter::New();
    pnm->SetInput(iData);
    pnm->SetFileName(fname);
    pnm->Write();
    if (pnm->GetErrorCode() == vtkErrorCode::OutOfDiskSpaceError)
      {
      success = 0;
      }
    pnm->Delete();
    }
  else if (!strcmp(ext, ".png"))
    {
    vtkPNGWriter *png = vtkPNGWriter::New();
    png->SetInput(iData);
    png->SetFileName(fname);
    png->Write();
    if (png->GetErrorCode() == vtkErrorCode::OutOfDiskSpaceError)
      {
      success = 0;
      }
    png->Delete();
    }
  else if (!strcmp(ext, ".jpg"))
    {
    vtkJPEGWriter *jpg = vtkJPEGWriter::New();
    jpg->SetInput(iData);
    jpg->SetFileName(fname);
    jpg->Write();
    if (jpg->GetErrorCode() == vtkErrorCode::OutOfDiskSpaceError)
      {
      success = 0;
      }
    jpg->Delete();
    }
  
  if (!success)
    {
    vtkKWMessageDialog::PopupMessage(
      this->GetApplication(), this->GetParentTopLevel(), 
      ks_("Selection Frame Dialog|Title|Save Screenshot - Error!"),
      k_("There was a problem writing the image file.\n"
         "Please check the location and make sure you have write\n"
         "permissions and enough disk space."),
      vtkKWMessageDialog::ErrorIcon);
    }
  iData->Delete();

  return success;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::CopyScreenshotAllWidgetsToClipboard()
{
  // Append all widgets to an image

  vtkImageData *iData = vtkImageData::New();
  if (!this->AppendAllWidgetsToImageData(iData))
    {
    iData->Delete();
    return 0;
    }

  int *extent = iData->GetExtent();
  if (extent[0] > extent[1] && extent[2] > extent[3] && extent[4] > extent[5])
    {
    iData->Delete();
    return 0;
    }

  // Save to clipboard

#ifdef _WIN32

  vtkKWSelectionFrame *widget = this->GetSelectedWidget();
  if (!widget)
    {
    return 0;
    }

  vtkKWRenderWidget *rwwidget = this->GetVisibleRenderWidget(widget);
  if (!rwwidget)
    {
    return 0;
    }

  if (::OpenClipboard((HWND)rwwidget->GetRenderWindow()->GetGenericWindowId()))
    {
    extent = iData->GetWholeExtent();
    
    int size[2];
    size[0] = extent[1] - extent[0] + 1;
    size[1] = extent[3] - extent[2] + 1;

    int data_width = ((size[0] * 3 + 3) / 4) * 4;
    int src_width = size[0] * 3;
  
    EmptyClipboard();

    DWORD dwLen = sizeof(BITMAPINFOHEADER) + data_width * size[1];
    HANDLE hDIB = ::GlobalAlloc(GHND, dwLen);
    LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER) ::GlobalLock(hDIB);
    
    lpbi->biSize = sizeof(BITMAPINFOHEADER);
    lpbi->biWidth = size[0];
    lpbi->biHeight = size[1];
    lpbi->biPlanes = 1;
    lpbi->biBitCount = 24;
    lpbi->biCompression = BI_RGB;
    lpbi->biClrUsed = 0;
    lpbi->biClrImportant = 0;
    lpbi->biSizeImage = data_width * size[1];
    
    // Copy the data to the clipboard

    unsigned char *ptr = (unsigned char *)(iData->GetScalarPointer());
    unsigned char *dest = (unsigned char *)lpbi + lpbi->biSize;

    int i,j;
    for (i = 0; i < size[1]; i++)
      {
      for (j = 0; j < size[0]; j++)
        {
        *dest++ = ptr[2];
        *dest++ = ptr[1];
        *dest++ = *ptr;
        ptr += 3;
        }
      dest = dest + (data_width - src_width);
      }
    
    SetClipboardData (CF_DIB, hDIB);
    ::GlobalUnlock(hDIB);
    CloseClipboard();
    }           
#endif

  iData->Delete();
  
  return 1;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::PrintWidgets(
  double dpi, int selection_only)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  vtkKWSelectionFrame *first_widget = this->GetNthWidget(0);
  if (!first_widget)
    {
    }
  
  PRINTDLG pd;
  DOCINFO di;
  RECT rcDest = { 0, 0, 0, 0};
  
  memset((void *)&pd, 0, sizeof(PRINTDLG));

  pd.lStructSize = sizeof(PRINTDLG);
  vtkKWRenderWidget *first_rwwidget = 
    this->GetVisibleRenderWidget(first_widget);
  if (first_rwwidget)
    {
    pd.hwndOwner = (HWND)first_rwwidget->GetRenderWindow()->GetGenericWindowId();
    }
  pd.Flags = PD_RETURNDC;
  pd.hInstance = NULL;
  
  PrintDlg(&pd);
  HDC ghdc = pd.hDC;

  if (!ghdc)
    {
    return 0;
    }

  if (pd.hDevMode)
    {
    GlobalFree(pd.hDevMode);
    }
  if (pd.hDevNames)
    {
    GlobalFree(pd.hDevNames);
    }
  
  if (this->IsCreated())
    {
    vtkKWTkUtilities::SetTopLevelMouseCursor(this, "watch");
    this->GetApplication()->ProcessPendingEvents();
    }
  
  di.cbSize = sizeof(DOCINFO);
  di.lpszDocName = "Kitware Test";
  di.lpszOutput = NULL;
  
  StartDoc(ghdc, &di);
  StartPage(ghdc);

  // Get size of printer page in pixels

  int cxPage = GetDeviceCaps(ghdc, HORZRES);
  int cyPage = GetDeviceCaps(ghdc, VERTRES);

  // Get printer DPI

  int cxInch = GetDeviceCaps(ghdc, LOGPIXELSX);
  int cyInch = GetDeviceCaps(ghdc, LOGPIXELSY);

  double scale = (double)cxInch / dpi;
  
  SetStretchBltMode(ghdc, HALFTONE);
 
  // If only the selection is to be printed, set the res to 1, 1

  int res[2];
  if (selection_only)
    {
    res[0] = res[1] = 1;
    }
  else
    {
    res[0] = this->Resolution[0];
    res[1] = this->Resolution[1];
    }

  // First pass to compute the total size (i.e. the resolution * biggest win)

  int max_size[2] = { -1, -1 };

  int i, j;
  for (j = 0; j < this->Resolution[1]; j++)
    {
    for (i = 0; i < this->Resolution[0]; i++)
      {
      int pos[2]; pos[0] = i; pos[1] = j;
      vtkKWSelectionFrame *widget = this->GetWidgetAtPosition(pos);
      if (widget && (!selection_only || widget->GetSelected()))
        {
        vtkKWRenderWidget *rwwidget = this->GetVisibleRenderWidget(widget);
        if (rwwidget)
          {
          int *size = rwwidget->GetRenderWindow()->GetSize();
          if (max_size[0] < size[0])
            {
            max_size[0] = size[0];
            }
          if (max_size[1] < size[1])
            {
            max_size[1] = size[1];
            }
          }
        }
      }
    }

  int spacing = 4;

  int total_size[2];
  total_size[0] = res[0] * (max_size[0] + 2 * spacing);
  total_size[1] = res[1] * (max_size[1] + 2 * spacing);

  double ratio[2];
  ratio[0] = (double)max_size[0] / (double)total_size[0];
  ratio[1] = (double)max_size[1] / (double)total_size[1];

  // Print each widget (or the selection only)

  for (j = 0; j < this->Resolution[1]; j++)
    {
    for (i = 0; i < this->Resolution[0]; i++)
      {
      int pos[2]; pos[0] = i; pos[1] = j;
      vtkKWSelectionFrame *widget = this->GetWidgetAtPosition(pos);
      if (widget && (!selection_only || widget->GetSelected()))
        {
        vtkKWRenderWidget *rwwidget = this->GetVisibleRenderWidget(widget);
        if (rwwidget)
          {
          int i2, j2;
          if (selection_only)
            {
            i2 = j2 = 0;
            }
          else
            {
            i2 = i;
            j2 = j;
            }
          int printing = rwwidget->GetPrinting();
          rwwidget->SetPrinting(1);
          rwwidget->SetupPrint(
            rcDest, ghdc, cxPage, cyPage, cxInch, cyInch,
            ratio[0], ratio[1], total_size[0], total_size[1]);
          rwwidget->Render();

          StretchBlt(
            ghdc, 
            (double)rcDest.right * 
            (spacing + i2 * (max_size[0] + 2 * spacing))/(double)total_size[0],
            (double)rcDest.top * 
            (spacing + j2 * (max_size[1] + 2 * spacing))/(double)total_size[1],
            (double)rcDest.right * ratio[0], 
            (double)rcDest.top * ratio[1],
            (HDC)rwwidget->GetMemoryDC(), 
            0, 
            0,
            (double)rcDest.right / scale * ratio[0], 
            (double)rcDest.top / scale * ratio[1], 
            SRCCOPY);

          rwwidget->SetPrinting(printing);
          }
        }
      }
    }
  
  // Close the page

  EndPage(ghdc);
  EndDoc(ghdc);
  DeleteDC(ghdc);

  if (this->IsCreated())
    {
    vtkKWTkUtilities::SetTopLevelMouseCursor(this, NULL);
    }

  // At that point the Print Dialog does not seem to disappear.
  // Let's Render()

  if (this->IsCreated())
    {
    this->GetApplication()->ProcessPendingEvents();
    }
#else

  (void)dpi;
  (void)selection_only;

#endif

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::PrintAllWidgets()
{
  if (this->GetApplication())
    {
    return this->PrintAllWidgetsAtResolution(
      this->GetApplication()->GetPrintTargetDPI());
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::PrintAllWidgetsAtResolution(double dpi)
{
  return this->PrintWidgets(dpi, 0);
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::PrintSelectedWidget()
{
  if (this->GetApplication())
    {
    return this->PrintSelectedWidgetAtResolution(
      this->GetApplication()->GetPrintTargetDPI());
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkKWSelectionFrameLayoutManager::PrintSelectedWidgetAtResolution(
  double dpi)
{
  return this->PrintWidgets(dpi, 1);
}

//----------------------------------------------------------------------------
void vtkKWSelectionFrameLayoutManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Resolution: " << this->Resolution[0] << " x " 
     << this->Resolution[1] << endl;

  os << indent << "ResolutionEntriesMenu: " << this->ResolutionEntriesMenu << endl;
  os << indent << "ResolutionEntriesToolbar: " << this->ResolutionEntriesToolbar << endl;
}
