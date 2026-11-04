// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QPointer>
#include <QResizeEvent>
#include "DockSplitter.h"
namespace ads {
// Retained chrome stays laid out in memory. Only the active tree can paint;
// parking never hides a splitter subtree or reparents native panel content.
class CPreparedLayoutCanvas final : public QWidget
{
    QPointer<CDockSplitter> m_Current;
    void place(CDockSplitter *root) {
        root->setGeometry(root == m_Current
            ? rect() : QRect(-width() - 1, 0, width(), height()));
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        for (auto *root : findChildren<CDockSplitter*>(QString(), Qt::FindDirectChildrenOnly))
            place(root);
    }
public:
    explicit CPreparedLayoutCanvas(QWidget *parent) : QWidget(parent) {
        setObjectName(QStringLiteral("adsPreparedLayoutCanvas"));
    }
    void prepare(CDockSplitter *root) {
        if (root->parentWidget() != this) root->setParent(this);
        root->setUpdatesEnabled(false);
        place(root);
        root->show();
    }
    void activate(CDockSplitter *root) {
        if (m_Current && m_Current != root) {
            auto *previous = m_Current.data();
            previous->setUpdatesEnabled(false);
            m_Current = root;
            place(previous);
        } else m_Current = root;
        if (root->parentWidget() != this) root->setParent(this);
        place(root);
        if (root->isHidden()) root->show();
        root->setUpdatesEnabled(true);
        root->raise();
    }
    void park() {
        auto *previous = m_Current.data();
        m_Current = nullptr;
        if (previous) {
            previous->setUpdatesEnabled(false);
            place(previous);
        }
    }
};
} // namespace ads
