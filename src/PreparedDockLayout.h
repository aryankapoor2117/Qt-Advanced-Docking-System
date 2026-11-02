// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QPointer>
#include <QVector>
#include <QSize>
#include "DockAreaWidget.h"
#include "DockSplitter.h"
#include "DockWidget.h"
namespace ads {
struct PreparedDockArea {
    QPointer<CDockAreaWidget> area;
    QList<QPointer<CDockWidget>> widgets;
    QList<bool> closed;
    int selected = -1;
};
struct PreparedDockLayout {
    QPointer<CDockSplitter> root;
    QVector<PreparedDockArea> areas;
    QVector<QPair<QPointer<CDockSplitter>, QList<int>>> sizes;
    QSize viewport;
    bool geometryEdited = false;
    QVector<QMetaObject::Connection> geometryWatches;
    ~PreparedDockLayout() {
        for (const auto &connection : geometryWatches) QObject::disconnect(connection);
    }
    bool matchesActive() const {
        if (!valid() || geometryEdited) return false;
        for (const auto &group : areas) {
            if (group.area->currentIndex() != group.selected) return false;
            for (int i = 0; i < group.widgets.size(); ++i)
                if (group.widgets[i]->isClosed() != group.closed[i]
                    || group.widgets[i]->dockAreaWidget() != group.area) return false;
        }
        return true;
    }
    bool valid() const {
        if (!root) return false;
        for (const auto &group : areas) {
            if (!group.area || group.area->dockWidgetsCount() != group.widgets.size()) return false;
            for (int i = 0; i < group.widgets.size(); ++i)
                if (!group.widgets[i] || group.area->dockWidget(i) != group.widgets[i]) return false;
        }
        return true;
    }
};
}
