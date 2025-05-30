// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include <QApplication>
#include <QComboBox>
#include <QMainWindow>
#include <QToolBar>
#include <QtCore>
#include <QtDebug>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqServer.h>
#include <pqServerManagerModel.h>
#include <pqTabbedMultiViewWidget.h>
#include <vtkSMViewLayoutProxy.h>

namespace
{

class MainWindow : public QMainWindow
{
  // Enable the use of the tr() method in a class without Q_OBJECT macro
  Q_DECLARE_TR_FUNCTIONS(MainWindow)
  QComboBox* ComboBox;
  pqTabbedMultiViewWidget* TMVWidget;

public:
  MainWindow()
  {
    auto tb = this->addToolBar(tr("Toolbar"));
    this->ComboBox = new QComboBox();
    this->ComboBox->addItem(tr("No filtering"), QString());
    tb->addWidget(this->ComboBox);

    this->TMVWidget = new pqTabbedMultiViewWidget(this);
    this->setCentralWidget(this->TMVWidget);

    this->setupPipeline();

    QObject::connect(this->ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
      [this](int)
      {
        const QString filter = this->ComboBox->currentData().toString();
        if (filter.isEmpty())
        {
          this->TMVWidget->disableAnnotationFilter();
        }
        else
        {
          this->TMVWidget->enableAnnotationFilter(filter);
        }
      });
  }

  ~MainWindow() override = default;

  void setupPipeline()
  {
    pqApplicationCore* core = pqApplicationCore::instance();
    pqObjectBuilder* ob = core->getObjectBuilder();

    // Create server only after a pipeline browser get created...
    pqServer* server = ob->createServer(pqServerResource("builtin:"));
    pqActiveObjects::instance().setActiveServer(server);

    QString viewType = "SpreadSheetView";
    pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
    QList<vtkSMViewLayoutProxy*> layouts;
    for (int cc = 0; cc < 4; ++cc)
    {
      this->TMVWidget->setCurrentTab(this->TMVWidget->createTab(server));
      layouts.push_back(this->TMVWidget->layoutProxy());
      // Create a view for each layout as now empty layouts are shown even when filtering
      // is enabled and the layout annotations don't match (so that it is possible to
      // create a new view).
      auto view = ob->createView(viewType, server);
      auto layoutProxy = smmodel->findItem<pqProxy*>(this->TMVWidget->layoutProxy());
      ob->addToLayout(view, layoutProxy);
    }

    this->ComboBox->addItem(tr("Filter 1 (Layouts 1,2,4)"), QString("Filter1"));
    layouts[0]->SetAnnotation("Filter1", "1");
    layouts[1]->SetAnnotation("Filter1", "1");
    layouts[3]->SetAnnotation("Filter1", "1");

    this->ComboBox->addItem(tr("Filter 2 (Layouts 3,4)"), QString("Filter2"));
    layouts[2]->SetAnnotation("Filter2", "1");
    layouts[3]->SetAnnotation("Filter2", "1");
  }

  bool doTest()
  {
    this->ComboBox->setCurrentIndex(1);
    if (!this->validate({ "Layout #1", "Layout #2", "Layout #4", "+" }))
    {
      return false;
    }
    this->ComboBox->setCurrentIndex(2);
    if (!this->validate({ "Layout #3", "Layout #4", "+" }))
    {
      return false;
    }
    this->ComboBox->setCurrentIndex(0);
    if (!this->validate({ "Layout #1", "Layout #2", "Layout #3", "Layout #4", "+" }))
    {
      return false;
    }
    return true;
  }

  bool validate(const QStringList& labels) const
  {
    if (this->TMVWidget->visibleTabLabels() != labels)
    {
      qCritical() << "ERROR! Mismatched tabs. Expected: " << labels
                  << " Got: " << this->TMVWidget->visibleTabLabels();
      return false;
    }
    return true;
  }

private:
  Q_DISABLE_COPY(MainWindow);
};

} // end of namespace

extern int TabbedMultiViewWidgetFilteringApp(int argc, char* argv[])
{
  QApplication app(argc, argv);
  pqApplicationCore appCore(argc, argv);

  MainWindow window;
  window.resize(800, 600);
  window.show();

  const bool success = window.doTest();
  const int retval = app.arguments().indexOf("--exit") == -1 ? app.exec() : EXIT_SUCCESS;
  return success ? retval : EXIT_FAILURE;
}
