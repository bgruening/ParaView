// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#ifndef pqServer_h
#define pqServer_h

class vtkObject;
class pqTimeKeeper;
class vtkProcessModule;
class vtkPVServerInformation;
class vtkPVXMLElement;
class vtkSMApplication;
class vtkSMProxy;
class vtkSMProxySelectionModel;
class vtkSMRenderViewProxy;
class vtkSMSession;
class vtkSMSessionProxyManager;

#include "pqCoreModule.h"
#include "pqServerManagerModelItem.h"
#include "pqServerResource.h"
#include "pqTimer.h"
#include "vtkSMMessageMinimal.h"
#include "vtkSmartPointer.h"
#include "vtkWeakPointer.h"

#include <QPointer>

class QProcess;

/**
 * pqServer (should be renamed to pqSession) is a pqServerManagerModelItem
 * subclass that represents a vtkSMSession. Besides providing API to access
 * vtkSMSession, it also performs some initialization of session-related
 * proxies such as time-keeper and global-mapper-properties proxies.
 */
class PQCORE_EXPORT pqServer : public pqServerManagerModelItem
{
  Q_OBJECT
public:
  pqServer(vtkIdType connectionId, QObject* parent = nullptr);
  ~pqServer() override;

  const pqServerResource& getResource();
  void setResource(const pqServerResource& server_resource);

  /**
   * Returns the session instance which the pqServer represents.
   */
  vtkSMSession* session() const;

  /**
   * Returns the connection id for the server connection.
   */
  vtkIdType GetConnectionID() const;
  vtkIdType sessionId() const { return this->GetConnectionID(); }

  /**
   * Returns the proxy manager for this session.
   */
  vtkSMSessionProxyManager* proxyManager() const;

  /**
   * Sources selection model is used to keep track of sources currently
   * selected on this session/server-connection.
   */
  vtkSMProxySelectionModel* activeSourcesSelectionModel() const;

  /**
   * View selection model is used to keep track of active view.
   */
  vtkSMProxySelectionModel* activeViewSelectionModel() const;

  /**
   * Return the number of data server partitions on this server connection. A
   * convenience method.
   */
  int getNumberOfPartitions();

  /**
   * Returns is this connection is a connection to a remote server or a
   * built-in server.
   */
  bool isRemote() const;

  /**
   * Returns true if the client is currently master. For non-collaborative
   * session, it always return true.
   */
  bool isMaster() const;

  /**
   * Returns true if the client is currently processing remote messages
   * and still have more to process.
   * This method is used to deffered the tryRender.
   */
  bool isProcessingPending() const;

  /**
   * Returns true is this connection has a separate render-server and
   * data-server.
   */
  bool isRenderServerSeparate();

  /**
   * Returns the time keeper for this connection.
   */
  pqTimeKeeper* getTimeKeeper() const;

  /**
   * Returns the vtkPVServerInformation object which contains information about
   * the command line options specified on the remote server, if any.
   */
  vtkPVServerInformation* getServerInformation() const;

  /**
   * Returns true if the client is waiting on some actions from the server that
   * typically result in progress events.
   */
  bool isProgressPending() const;

  /**
   * Get/Set the application wide heart beat timeout setting.
   * Heartbeats are used in case of remote server connections to avoid the
   * connection timing out due to inactivity. When set, the client send a
   * heartbeat message to all servers every \c msec milliseconds.
   */
  static void setHeartBeatTimeoutSetting(int msec);
  static int getHeartBeatTimeoutSetting();

  /**
   * enable/disable monitoring of server notifications.
   */
  void setMonitorServerNotifications(bool);

  /**
   * Get the server remaining life time in minutes.
   */
  int getRemainingLifeTime() const;

  /**
   * Set the time (in minutes) remaining.
   */
  void setRemainingLifeTime(int value);

  ///@{
  /**
   * Set/Get the QProcess used to run the server script
   * Will be deleted by the destructor of this class if set
   */
  void setScriptProcess(QProcess* process);
  QProcess* getScriptProcess() const;
  ///@}

Q_SIGNALS:
  /**
   * Fired when the name of the proxy is changed.
   */
  void nameChanged(pqServerManagerModelItem*);

  /**
   * Fired about 5 minutes before the server timesout. This signal will not be
   * fired at all if server timeout < 5 minutes. The server timeout is
   * specified by --timeout option on the server process.
   * This is not fired if timeout is not specified on the server process.
   */
  void fiveMinuteTimeoutWarning();

  /**
   * Fired about 1 minute before the server timesout.
   * The server timeout is specified by --timeout option on the server process.
   * This is not fired if timeout is not specified on the server process.
   */
  void finalTimeoutWarning();

  /**
   * Fired if any server side crash or disconnection occurred.
   */
  void serverSideDisconnected();

protected:
  /**
   * Returns the string key used for the heart beat time interval.
   */
  static const char* HEARBEAT_TIME_SETTING_KEY();

  /**
   * Set the heartbeat timeout for this instance of pqServer.
   */
  void setHeartBeatTimeout(int msec);

  // ---- Collaboration client-to-clients communication mechanisme ----

Q_SIGNALS:
  /**
   * Allow user to listen messages from other clients.
   * But if you plan to push some state by for example calling
   * the sendToOtherClients(vtkSMMessage*) slot, you MUST queued your slot.
   * Otherwise your communication will not be sent to the server.
   * Here is a code sample on how to connect to that signal:
   *
   *    QObject::connect( server, SIGNAL(sentFromOtherClient(vtkSMMessage*)),
   *                      this,   SLOT(onClientMessage(vtkSMMessage*)),
   *                      Qt::QueuedConnection);
   */
  void sentFromOtherClient(pqServer*, vtkSMMessage* msg);

  /**
   * Signal triggered when user information get updated
   */
  void triggeredMasterUser(int);
  void triggeredUserName(int, QString&);
  void triggeredUserListChanged();
  void triggerFollowCamera(int);

public Q_SLOTS:
  /**
   * Allow user to broadcast to other client a given message
   */
  void sendToOtherClients(vtkSMMessage* msg);

  // ---- Collaboration client-to-clients communication mechanisme ----

protected Q_SLOTS:
  /**
   * Called to send a heartbeat to the server.
   */
  void heartBeat();

  /**
   * Called to update the server life time.
   */
  void updateRemainingLifeTime();

  /**
   * Called when idle to look for server notification for collaboration purpose
   */
  void processServerNotification();

  /**
   * Called by vtkSMCollaborationManager when associated message happen.
   * This will convert the given parameter into vtkSMMessage and
   * emit sentFromOtherClient(pqServer*,vtkSMMessage*) signal.
   */
  void onCollaborationCommunication(vtkObject*, unsigned long, void*, void*);

  /**
   * Called by vtkSMSessionClient is any communication error occurred with the
   * server. This usually mean that the server side is dead.
   */
  void onConnectionLost(vtkObject*, unsigned long, void*, void*);

private:
  Q_DISABLE_COPY(pqServer)

  pqServerResource Resource;
  vtkIdType ConnectionID;
  vtkWeakPointer<vtkSMSession> Session;
  vtkSmartPointer<vtkSMProxy> ExecutableRunnerProxy;

  pqTimer IdleCollaborationTimer;

  class pqInternals;
  pqInternals* Internals;
};

#endif // !pqServer_h
