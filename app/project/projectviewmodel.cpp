/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2019 Olive Team

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include "projectviewmodel.h"

#include <QDebug>
#include <QMimeData>

ProjectViewModel::ProjectViewModel(QObject *parent) :
  QAbstractItemModel(parent),
  project_(nullptr)
{
  // TODO make this configurable
  columns_.append(kName);
  columns_.append(kDuration);
  columns_.append(kRate);
}

Project *ProjectViewModel::project()
{
  return project_;
}

void ProjectViewModel::set_project(Project *p)
{
  beginResetModel();

  project_ = p;

  endResetModel();
}

QModelIndex ProjectViewModel::index(int row, int column, const QModelIndex &parent) const
{
  // I'm actually not 100% sure what this does, but it seems logical and was in the earlier code
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  // Get the parent object (project root if the index is invalid)
  Item* item_parent = (parent.isValid()) ? static_cast<Item*>(parent.internalPointer()) : project_->root();

  // Return an index to this object
  return createIndex(row, column, item_parent->child(row));
}

QModelIndex ProjectViewModel::parent(const QModelIndex &child) const
{
  // Get the Item object from the index
  Item* item = static_cast<Item*>(child.internalPointer());

  // Get Item's parent object
  Item* par = item->parent();

  // If the parent is the root, return an empty index
  if (par == project_->root()) {
    return QModelIndex();
  }

  // Otherwise return a true index to its parent
  int parent_index = indexOfChild(par);

  // Make sure the index is valid (there's no reason it shouldn't be)
  Q_ASSERT(parent_index > -1);

  // Return an index to the parent
  return createIndex(parent_index, 0, par);
}

int ProjectViewModel::rowCount(const QModelIndex &parent) const
{
  // If there's no project, there are obviously no items to show
  if (project_ == nullptr) {
    return 0;
  }

  // If the index is the root, return the root child count
  if (parent == QModelIndex()) {
    return project_->root()->child_count();
  }

  // Otherwise, the index must contain a valid pointer, so we just return its child count
  return static_cast<Item*>(parent.internalPointer())->child_count();
}

int ProjectViewModel::columnCount(const QModelIndex &parent) const
{
  Q_UNUSED(parent)

  // Not strictly necessary, but a decent visual cue that there's no project currently active
  if (project_ == nullptr) {
    return 0;
  }

  return columns_.size();
}

QVariant ProjectViewModel::data(const QModelIndex &index, int role) const
{
  Item* internal_item = static_cast<Item*>(index.internalPointer());

  switch (role) {
  case Qt::DisplayRole:
  {
    // Standard text role
    ColumnType column_type = columns_.at(index.column());

    switch (column_type) {
    case kName:
      return internal_item->name();
    case kDuration:
      // FIXME Return actual information
      return "00:00:00;00";
    case kRate:
      // FIXME Return actual information
      return "29.97 FPS";
    }
  }
    break;
  case Qt::DecorationRole:
    // If this is the first column, return the Item's icon
    if (index.column() == 0) {
      return internal_item->icon();
    }
    break;
  case Qt::ToolTipRole:
    return internal_item->tooltip();
  }

  return QVariant();
}

QVariant ProjectViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  // Check if we need text data (DisplayRole) and orientation is horizontal
  // FIXME I'm not 100% sure what happens if the orientation is vertical/if that check is necessary
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    ColumnType column_type = columns_.at(section);

    // Return the name based on the column's current type
    switch (column_type) {
    case kName:
      return tr("Name");
    case kDuration:
      return tr("Duration");
    case kRate:
      return tr("Rate");
    }
  }

  return QAbstractItemModel::headerData(section, orientation, role);
}

bool ProjectViewModel::hasChildren(const QModelIndex &parent) const
{
  // Check if this is a valid index
  if (parent.isValid()) {
    Item* item = static_cast<Item*>(parent.internalPointer());

    // Check if this item is a kFolder type
    // If it's a folder, we always return TRUE in order to always show the "expand triangle" icon,
    // even when there are no "physical" children
    if (item->type() == Item::kFolder) {
      return true;
    }
  }

  // Otherwise, return default behavior
  return QAbstractItemModel::hasChildren(parent);
}

Qt::ItemFlags ProjectViewModel::flags(const QModelIndex &index) const
{
  return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | QAbstractItemModel::flags(index);
}

QStringList ProjectViewModel::mimeTypes() const
{
  return {"application/x-oliveprojectitemdata"};
}

QMimeData *ProjectViewModel::mimeData(const QModelIndexList &indexes) const
{
  // Encode mime data for the rows/items that were dragged
  QMimeData* data = new QMimeData();

  // Use QDataStream to stream the item data into a byte array
  QByteArray encoded_data;
  QDataStream stream(&encoded_data, QIODevice::WriteOnly);

  // The indexes list includes indexes for each column which we don't use. To make sure each row only gets sent *once*,
  // we keep a list of dragged items
  QVector<void*> dragged_items;

  foreach (QModelIndex index, indexes) {
    if (index.isValid()) {
      // Check if we've dragged this item before
      if (!dragged_items.contains(index.internalPointer())) {
        // If not, add it to the stream (and also keep track of it in the vector)s
        stream << index.row() << reinterpret_cast<quintptr>(index.internalPointer());
        dragged_items.append(index.internalPointer());
      }
    }
  }

  // Set byte array as the mime data and return the mime data
  data->setData("application/x-oliveprojectitemdata", encoded_data);

  return data;
}

bool ProjectViewModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &drop)
{
  // Default recommended checks from https://doc.qt.io/qt-5/model-view-programming.html#using-drag-and-drop-with-item-views
  if (!canDropMimeData(data, action, row, column, drop)) {
    return false;
  }

  if (action == Qt::IgnoreAction) {
    return true;
  }

  // Probe mime data for its format
  QStringList mime_formats = data->formats();

  if (mime_formats.contains("application/x-oliveprojectitemdata")) {
    // Data is drag/drop data from this model
    QByteArray model_data = data->data("application/x-oliveprojectitemdata");

    // Use QDataStream to deserialize the data
    QDataStream stream(&model_data, QIODevice::ReadOnly);

    // Get the Item object that the items were dropped on
    Item* drop_location;

    // If the index is valid, move them to the containing object
    if (drop.isValid()) {
      drop_location = static_cast<Item*>(drop.internalPointer());

      // If this is not a folder, we cannot drop these items here
      if (drop_location->type() != Item::kFolder) {
        return false;
      }
    } else {
      // If the index isn't valid, the items are moving to the root
      drop_location = project_->root();
    }

    // Variables to deserialize into
    quintptr item_ptr;
    int r;

    // Loop through all data
    while (!stream.atEnd()) {
      stream >> r >> item_ptr;

      // Get the Item pointer from the mime data
      Item* item = reinterpret_cast<Item*>(item_ptr);

      // Get the Item's parent
      Item* parent_item = item->parent();

      // If the Drop Item is the Item or its parent already, this is a no-op
      if (item != drop_location && parent_item != drop_location) {

        // Get an index to the parent
        QModelIndex parent_index;

        // If the parent item is not the root, we'll need to actually create an index
        if (parent_item != project()->root()) {
          parent_index = createIndex(indexOfChild(parent_item), 0, parent_item);
        }

        // Signal to all views that we're about to move an object
        beginMoveRows(parent_index, r, r, drop, drop_location->child_count());

        // Move the object
        item->set_parent(drop_location);

        endMoveRows();
      }
    }

    return true;
  }

  return false;
}

int ProjectViewModel::indexOfChild(Item *item) const
{
  // Find parent's index within its own parent
  // (TODO: this model should handle sorting, which means it'll have to "know" the indices)

  Item* parent = item->parent();

  if (parent != nullptr) {
    for (int i=0;i<parent->child_count();i++) {
      if (parent->child(i) == item) {
        return i;
      }
    }
  }

  return -1;
}