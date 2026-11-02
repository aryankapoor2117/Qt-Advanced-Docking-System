// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <QEvent>
#include <QPointer>
#include <QTimer>
#include <QVector>
#include "DockAreaWidget.h"
#include "DockContainerWidget.h"
#include "DockWidget.h"
#include "DockManager.h"

namespace ads {

// A layout-owned slot for a container-owned panel. No content is copied and
// no pixels are cached. The real panel remains interactive at the slot bounds.
class CPersistentDockAreaContent final : public QWidget
{
    QPointer<CDockAreaWidget> m_Area;
    QPointer<CDockWidget> m_Dock;
    QVector<QPointer<QWidget>> m_Ancestors;
    bool m_Syncing = false;
    bool m_Queued = false;

    void watchAncestors()
    {
        for (auto &widget : m_Ancestors)
            if (widget) widget->removeEventFilter(this);
        m_Ancestors.clear();
        for (QWidget *widget = parentWidget(); widget; widget = widget->parentWidget()) {
            m_Ancestors.append(widget);
            widget->installEventFilter(this);
        }
    }

    void queueSync()
    {
        if (m_Queued) return;
        m_Queued = true;
        QTimer::singleShot(0, this, [this] {
            m_Queued = false;
            sync();
        });
    }

protected:
    bool event(QEvent *event) override
    {
        const bool result = QWidget::event(event);
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
        case QEvent::Hide:
            sync();
            break;
        default: break;
        }
        return result;
    }

    bool eventFilter(QObject *, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::ParentChange:
            watchAncestors();
            sync();
            queueSync();
            break;
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::LayoutRequest:
            sync();
            queueSync();
            break;
        default: break;
        }
        return false;
    }

public:
    explicit CPersistentDockAreaContent(CDockAreaWidget *area)
        : QWidget(area), m_Area(area)
    {
        setObjectName(QStringLiteral("adsPersistentContentSlot"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        watchAncestors();
    }

    ~CPersistentDockAreaContent() override
    {
        for (auto &widget : m_Ancestors)
            if (widget) widget->removeEventFilter(this);
    }

    QSize minimumSizeHint() const override
    {
        return m_Dock ? m_Dock->minimumSizeHint().expandedTo(m_Dock->minimumSize()) : QSize(0, 0);
    }

    QSize sizeHint() const override
    {
        return m_Dock ? m_Dock->sizeHint() : QSize(0, 0);
    }

    void setDockWidget(CDockWidget *dock)
    {
        if (m_Dock && m_Dock != dock && m_Dock->dockAreaWidget() == m_Area)
            m_Dock->hide();
        m_Dock = dock;
        updateGeometry();
        sync();
        queueSync();
    }

    void sync()
    {
        if (m_Syncing || !m_Dock || !m_Area || m_Dock->dockAreaWidget() != m_Area
            || m_Area->dockManager()->isRestoringState())
            return;
        m_Syncing = true;
        auto *host = m_Area->dockContainer();
        if (host) {
            // Reparent only for a genuine move between native containers.
            const bool changedHost = m_Dock->parentWidget() != host;
            if (changedHost)
                m_Dock->setParent(host);
            const bool visible = isVisibleTo(host) && !m_Dock->isClosed();
            if (visible) {
                const QRect bounds(mapTo(host, QPoint()), size());
                const bool changedBounds = m_Dock->geometry() != bounds;
                if (changedBounds)
                    m_Dock->setGeometry(bounds);
                // CDockWidget::raise() selects a tab (and may activate a
                // floating window). Raise the actual sibling surface instead.
                if (changedHost || changedBounds || m_Dock->isHidden())
                    m_Dock->QWidget::raise();
            }
            if (m_Dock->isHidden() == visible)
                m_Dock->setVisible(visible);
        }
        m_Syncing = false;
    }
};
} // namespace ads
