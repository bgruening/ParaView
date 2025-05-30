// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#include "pqVRStarter.h"
#include "pqApplicationCore.h"
#include "pqTestUtility.h"
#include "pqVRConnectionManager.h"
#include "pqVRQueueHandler.h"
#include "pqWidgetEventPlayer.h"
#include "vtkPVVRConfig.h"
#include "vtkPVXMLElement.h"
#include "vtkProcessModule.h"
#include "vtkVRInteractorStyleFactory.h"
#include "vtkVRQueue.h"

#include <QRegularExpression>
#include <QTimer>
#include <QtDebug>

// Used for testing:
#if PARAVIEW_PLUGIN_CAVEInteraction_USE_VRPN
#include <pqVRPNConnection.h>
class pqVREventPlayer : public pqWidgetEventPlayer
{
  typedef pqWidgetEventPlayer Superclass;

public:
  pqVREventPlayer(QObject* _parent)
    : Superclass(_parent)
  {
  }
  virtual bool playEvent(QObject*, const QString& command, const QString& arguments, bool& error)
  {
    if (command == "pqVREvent")
    {
      if (arguments.startsWith("vrpn_trackerEvent"))
      {
        // Syntax is (one line:)
        // "vrpn_trackerEvent:[connName];[sensorid];[pos_x],[pos_y],[pos_z];
        // [quat_w],[quat_x],[quat_y],[quat_z]"
        QRegularExpression capture("vrpn_trackerEvent:"
                                   "([\\w.@]+);" // Connection name
                                   "(\\d+);"     // sensor id
                                   "([\\d.-]+)," // pos_x
                                   "([\\d.-]+)," // pos_y
                                   "([\\d.-]+);" // pos_z
                                   "([\\d.-]+)," // quat_w
                                   "([\\d.-]+)," // quat_x
                                   "([\\d.-]+)," // quat_y
                                   "([\\d.-]+)$" // quat_z
        );
        auto match = capture.match(arguments);
        if (!match.hasMatch())
        {
          qWarning() << "pqVREventPlayer: bad arguments:" << command;
          error = true;
          return false;
        }
        vrpn_TRACKERCB event;
        QString connName;
        connName = match.captured(1);
        event.sensor = match.captured(2).toInt();
        event.pos[0] = match.captured(3).toDouble();
        event.pos[1] = match.captured(4).toDouble();
        event.pos[2] = match.captured(5).toDouble();
        event.quat[0] = match.captured(6).toDouble();
        event.quat[1] = match.captured(7).toDouble();
        event.quat[2] = match.captured(8).toDouble();
        event.quat[3] = match.captured(9).toDouble();
        pqVRConnectionManager* mgr = pqVRConnectionManager::instance();
        pqVRPNConnection* conn = mgr->GetVRPNConnection(connName);
        if (!conn)
        {
          qWarning() << "pqVREventPlayer: bad connection name:" << command;
          error = true;
          return false;
        }
        conn->newTrackerValue(event);
        return true;
      }
      else
      {
        error = true;
      }
      return true;
    }
    else
    {
      return false;
    }
  }
};
#endif // PARAVIEW_PLUGIN_CAVEInteraction_USE_VRPN

//-----------------------------------------------------------------------------
class pqVRStarter::pqInternals
{
public:
  pqVRConnectionManager* ConnectionManager;
  vtkVRQueue* EventQueue;
  pqVRQueueHandler* Handler;
  vtkVRInteractorStyleFactory* StyleFactory;
};

//-----------------------------------------------------------------------------
pqVRStarter::pqVRStarter(QObject* _parent /*=0*/)
  : QObject(_parent)
{
  this->Internals = new pqInternals;
  this->Internals->EventQueue = nullptr;
  this->Internals->Handler = nullptr;
  this->Internals->StyleFactory = nullptr;

#if PARAVIEW_PLUGIN_CAVEInteraction_USE_VRPN
  pqVREventPlayer* player = new pqVREventPlayer(nullptr);
  pqApplicationCore::instance()->testUtility()->eventPlayer()->addWidgetEventPlayer(player);
#endif // PARAVIEW_PLUGIN_CAVEInteraction_USE_VRPN

  this->IsShutdown = true;
}

//-----------------------------------------------------------------------------
pqVRStarter::~pqVRStarter()
{
  if (!this->IsShutdown)
  {
    this->onShutdown();
  }
}

//-----------------------------------------------------------------------------
void pqVRStarter::onStartup()
{
  if (!this->IsShutdown)
  {
    qWarning() << "pqVRStarter: Cannot startup -- already started.";
    return;
  }
  this->IsShutdown = false;
  this->Internals->EventQueue = vtkVRQueue::New();
  this->Internals->ConnectionManager = new pqVRConnectionManager(this->Internals->EventQueue, this);
  pqVRConnectionManager::setInstance(this->Internals->ConnectionManager);
  this->Internals->Handler = new pqVRQueueHandler(this->Internals->EventQueue, this);
  pqVRQueueHandler::setInstance(this->Internals->Handler);
  this->Internals->StyleFactory = vtkVRInteractorStyleFactory::New();
  vtkVRInteractorStyleFactory::SetInstance(this->Internals->StyleFactory);
}

//-----------------------------------------------------------------------------
void pqVRStarter::onShutdown()
{
  if (this->IsShutdown)
  {
    qWarning() << "pqVRStarter: Cannot shutdown -- not started yet.";
    return;
  }
  this->IsShutdown = true;
  pqVRConnectionManager::setInstance(nullptr);
  pqVRQueueHandler::setInstance(nullptr);
  vtkVRInteractorStyleFactory::SetInstance(nullptr);
  delete this->Internals->Handler;
  delete this->Internals->ConnectionManager;
  this->Internals->EventQueue->Delete();
  this->Internals->StyleFactory->Delete();
}
