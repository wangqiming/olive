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

#include "timelineview.h"

#include <QDebug>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QtMath>
#include <QPen>

#include "common/flipmodifiers.h"
#include "common/timecodefunctions.h"
#include "core.h"
#include "node/input/media/media.h"
#include "project/item/footage/footage.h"

TimelineView::TimelineView(const TrackType &type, Qt::Alignment vertical_alignment, QWidget *parent) :
  TimelineViewBase(parent),
  connected_track_list_(nullptr),
  type_(type)
{
  Q_ASSERT(vertical_alignment == Qt::AlignTop || vertical_alignment == Qt::AlignBottom);
  setAlignment(Qt::AlignLeft | vertical_alignment);

  setDragMode(NoDrag);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setBackgroundRole(QPalette::Window);
  setContextMenuPolicy(Qt::CustomContextMenu);
  SetLimitYAxis(true);
}

void TimelineView::SelectAll()
{
  QList<QGraphicsItem*> all_items = items();

  foreach (QGraphicsItem* i, all_items) {
    i->setSelected(true);
  }
}

void TimelineView::DeselectAll()
{
  QList<QGraphicsItem*> all_items = items();

  foreach (QGraphicsItem* i, all_items) {
    i->setSelected(false);
  }
}

void TimelineView::mousePressEvent(QMouseEvent *event)
{
  if (PlayheadPress(event)) {
    // Let the parent handle this
    return;
  }

  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->modifiers());

  emit MousePressed(&timeline_event);
}

void TimelineView::mouseMoveEvent(QMouseEvent *event)
{
  if (PlayheadMove(event)) {
    // Let the parent handle this
    return;
  }

  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->modifiers());

  emit MouseMoved(&timeline_event);
}

void TimelineView::mouseReleaseEvent(QMouseEvent *event)
{
  if (PlayheadRelease(event)) {
    // Let the parent handle this
    return;
  }

  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->modifiers());

  emit MouseReleased(&timeline_event);
}

void TimelineView::mouseDoubleClickEvent(QMouseEvent *event)
{
  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->modifiers());

  emit MouseDoubleClicked(&timeline_event);
}

void TimelineView::dragEnterEvent(QDragEnterEvent *event)
{
  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->keyboardModifiers());

  timeline_event.SetMimeData(event->mimeData());
  timeline_event.SetEvent(event);

  emit DragEntered(&timeline_event);
}

void TimelineView::dragMoveEvent(QDragMoveEvent *event)
{
  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->keyboardModifiers());

  timeline_event.SetMimeData(event->mimeData());
  timeline_event.SetEvent(event);

  emit DragMoved(&timeline_event);
}

void TimelineView::dragLeaveEvent(QDragLeaveEvent *event)
{
  emit DragLeft(event);
}

void TimelineView::dropEvent(QDropEvent *event)
{
  TimelineViewMouseEvent timeline_event(ScreenToCoordinate(event->pos()),
                                        event->keyboardModifiers());

  timeline_event.SetMimeData(event->mimeData());
  timeline_event.SetEvent(event);

  emit DragDropped(&timeline_event);
}

Stream::Type TimelineView::TrackTypeToStreamType(TrackType track_type)
{
  switch (track_type) {
  case kTrackTypeNone:
  case kTrackTypeCount:
    break;
  case kTrackTypeVideo:
    return Stream::kVideo;
  case kTrackTypeAudio:
    return Stream::kAudio;
  case kTrackTypeSubtitle:
    return Stream::kSubtitle;
  }

  return Stream::kUnknown;
}

TimelineCoordinate TimelineView::ScreenToCoordinate(const QPoint& pt)
{
  return SceneToCoordinate(mapToScene(pt));
}

TimelineCoordinate TimelineView::SceneToCoordinate(const QPointF& pt)
{
  return TimelineCoordinate(SceneToTime(pt.x()), TrackReference(type_, SceneToTrack(pt.y())));
}

int TimelineView::GetTrackY(int track_index)
{
  int y = 0;

  if (alignment() & Qt::AlignBottom) {
    track_index++;
  }

  for (int i=0;i<track_index;i++) {
    y += GetTrackHeight(i);
  }

  if (alignment() & Qt::AlignBottom) {
    y = -y;
  }

  return y;
}

int TimelineView::GetTrackHeight(int track_index)
{
  if (!connected_track_list_ || track_index >= connected_track_list_->TrackCount()) {
    return TrackOutput::GetDefaultTrackHeight();
  }

  return connected_track_list_->TrackAt(track_index)->GetTrackHeight();
}

QPoint TimelineView::GetScrollCoordinates()
{
  return QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}

void TimelineView::SetScrollCoordinates(const QPoint &pt)
{
  horizontalScrollBar()->setValue(pt.x());
  verticalScrollBar()->setValue(pt.y());
}

void TimelineView::ConnectTrackList(TrackList *list)
{
  connected_track_list_ = list;
}

int TimelineView::SceneToTrack(double y)
{
  int track = -1;
  int heights = 0;

  if (alignment() & Qt::AlignBottom) {
    y = -y;
  }

  do {
    track++;
    heights += GetTrackHeight(track);
  } while (y > heights);

  return track;
}

void TimelineView::UserSetTime(const int64_t &time)
{
  SetTime(time);
  emit TimeChanged(time);
}
