// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause

/**
 * \file pqCustomFilterDefinitionModel.h
 * \date 6/19/2006
 */

#ifndef pqCustomFilterDefinitionModel_h
#define pqCustomFilterDefinitionModel_h

#include "pqComponentsModule.h"
#include <QAbstractItemModel>

#include "pqProxySelection.h" // for pqProxySelection.

class pqCustomFilterDefinitionModelItem;
class pqPipelineSource;
class QPixmap;
class vtkCollection;

/**
 * \class pqCustomFilterDefinitionModel
 * \brief
 *   The pqCustomFilterDefinitionModel class stores the sources that
 *   define a compound proxy in a hierarchical format.
 *
 * The hierarchical format is similar to the pqPipelineModel. The
 * custom filter definition model contains only sources. It does not
 * include any server objects, since the custom filter must be
 * defined on one server.
 */
class PQCOMPONENTS_EXPORT pqCustomFilterDefinitionModel : public QAbstractItemModel
{
public:
  enum ItemType
  {
    Invalid = -1,
    Source = 0,
    Filter,
    CustomFilter,
    Link,
    LastType = Link
  };

  pqCustomFilterDefinitionModel(QObject* parent = nullptr);
  ~pqCustomFilterDefinitionModel() override;

  /**
   * \name QAbstractItemModel Methods
   */
  ///@{
  /**
   * \brief
   *   Gets the number of rows for a given index.
   * \param parent The parent index.
   * \return
   *   The number of rows for the given index.
   */
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;

  /**
   * \brief
   *   Gets the number of columns for a given index.
   * \param parent The parent index.
   * \return
   *   The number of columns for the given index.
   */
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;

  /**
   * \brief
   *   Gets whether or not the given index has child items.
   * \param parent The parent index.
   * \return
   *   True if the given index has child items.
   */
  bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

  /**
   * \brief
   *   Gets a model index for a given location.
   * \param row The row number.
   * \param column The column number.
   * \param parent The parent index.
   * \return
   *   A model index for the given location.
   */
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

  /**
   * \brief
   *   Gets the parent for a given index.
   * \param index The model index.
   * \return
   *   A model index for the parent of the given index.
   */
  QModelIndex parent(const QModelIndex& index) const override;

  /**
   * \brief
   *   Gets the data for a given model index.
   * \param index The model index.
   * \param role The role to get data for.
   * \return
   *   The data for the given model index.
   */
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  /**
   * \brief
   *   Gets the flags for a given model index.
   *
   * The flags for an item indicate if it is enabled, editable, etc.
   *
   * \param index The model index.
   * \return
   *   The flags for the given model index.
   */
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  ///@}

  /**
   * \brief
   *   Sets the contents of the custom filter definition model.
   *
   * The \c items are added to the custom filter definition model in
   * a hierarchy similar to the pipeline model. Only the items in the
   * list are added to the hierarchy.
   *
   * \param items The list of selected model items.
   */
  void setContents(const pqProxySelection& items);

  /**
   * \brief
   *   Gets the next index in the model's tree hierarchy.
   * \param index The current model index.
   * \return
   *   A model index for the next item in the hierarchy.
   */
  QModelIndex getNextIndex(const QModelIndex& index) const;

  /**
   * \brief
   *   Gets the source associated with an index.
   * \param index The model index to look up.
   * \return
   *   A pointer to the source object or null if there is none.
   */
  pqPipelineSource* getSourceFor(const QModelIndex& index) const;

private:
  /**
   * \brief
   *   Gets the model object associated with the index.
   * \param index The model index to convert.
   * \return
   *   A pointer to the model object or null if there is none.
   */
  pqCustomFilterDefinitionModelItem* getModelItemFor(const QModelIndex& index) const;

  /**
   * \brief
   *   Gets the next item in the tree.
   * \param item The current model item.
   * \return
   *   A pointer to the next model item or null if the end is reached.
   */
  pqCustomFilterDefinitionModelItem* getNextItem(pqCustomFilterDefinitionModelItem* item) const;

  pqCustomFilterDefinitionModelItem* Root; ///< The root of the model tree.
  QList<QPixmap> PixmapList;               ///< Stores the item icons.
};

#endif
