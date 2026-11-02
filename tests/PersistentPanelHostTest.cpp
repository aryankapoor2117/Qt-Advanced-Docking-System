#include <QtTest/QtTest>
#include <QLineEdit>
#include <QPointer>
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "DockAreaTitleBar.h"
#include "DockAreaTabBar.h"
#include "DockWidgetTab.h"
#include "DockSplitter.h"
#include "FloatingDockContainer.h"

using namespace ads;

class PersistentPanelHostTest : public QObject
{
    Q_OBJECT
    int parentChanges = 0;
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::ParentChange) ++parentChanges;
        return false;
    }
    static void settle()
    {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
    static void verifyBounds(CDockWidget *dock)
    {
        auto *area = dock->dockAreaWidget();
        QVERIFY(area);
        QCOMPARE(QRect(area->mapFromGlobal(dock->mapToGlobal(QPoint())), dock->size()), area->contentAreaGeometry());
    }
private slots:
    void restoreBeforeFirstShowKeepsBodiesAboveDockTree()
    {
        CDockManager manager;
        manager.setPersistentDockWidgetHostsEnabled(true);
        auto *a = new CDockWidget("A"), *b = new CDockWidget("B");
        a->setWidget(new QLineEdit("Visible body A"));
        b->setWidget(new QLineEdit("Visible body B"));
        auto *left = manager.addDockWidget(LeftDockWidgetArea, a);
        manager.addDockWidget(RightDockWidgetArea, b, left);
        const auto original = manager.saveState();
        manager.addDockWidget(CenterDockWidgetArea, b, left);
        QVERIFY(manager.restoreState(original));
        manager.resize(1000,700);
        manager.show();
        settle();
        for (auto *dock : {a,b}) {
            QVERIFY(dock->isVisible());
            verifyBounds(dock);
            auto *hit = manager.childAt(dock->geometry().center());
            QVERIFY2(hit == dock || dock->isAncestorOf(hit),
                     "Live panel body is obscured by the dock tree after initial show");
        }
    }

    void customGroupsKeepSurfaceParentsAndState()
    {
        CDockManager manager;
        manager.setPersistentDockWidgetHostsEnabled(true);
        QList<CDockWidget*> docks;
        QList<QLineEdit*> views;
        for (int i = 0; i < 4; ++i) {
            auto *dock = new CDockWidget(QString::number(i));
            auto *view = new QLineEdit(QStringLiteral("active view %1").arg(i));
            dock->setWidget(view);
            docks << dock; views << view;
        }
        auto *left = manager.addDockWidget(LeftDockWidgetArea, docks[0]);
        manager.addDockWidget(CenterDockWidgetArea, docks[1], left);
        auto *right = manager.addDockWidget(RightDockWidgetArea, docks[2], left);
        manager.addDockWidget(CenterDockWidgetArea, docks[3], right);
        docks[1]->setAsCurrentTab(); docks[2]->setAsCurrentTab();
        const auto first = manager.saveState();
        manager.addDockWidget(CenterDockWidgetArea, docks[2], left);
        manager.addDockWidget(BottomDockWidgetArea, docks[0], right);
        manager.addDockWidget(CenterDockWidgetArea, docks[1], docks[0]->dockAreaWidget());
        docks[0]->setAsCurrentTab(); docks[3]->setAsCurrentTab();
        const auto second = manager.saveState();
        manager.resize(1000, 700); manager.show(); settle();
        docks[0]->winId(); // Exercise a native descendant as well as plain widgets.
        settle();
        parentChanges = 0;
        for (auto *dock : docks) dock->installEventFilter(this);
        for (auto *view : views) view->installEventFilter(this);
        for (int cycle = 0; cycle < 8; ++cycle) {
            const bool a = cycle % 2 == 0;
            QVERIFY(manager.restoreState(a ? first : second)); settle();
            QCOMPARE(parentChanges, 0);
            for (int i = 0; i < docks.size(); ++i) {
                QCOMPARE(docks[i]->parentWidget(), &manager);
                QCOMPARE(docks[i]->widget(), views[i]);
                QCOMPARE(views[i]->text(), QStringLiteral("active view %1").arg(i));
                if (docks[i]->isVisible()) verifyBounds(docks[i]);
            }
            QCOMPARE(docks[0]->dockAreaWidget()->currentDockWidget(), a ? docks[1] : docks[0]);
            QCOMPARE(docks[2]->dockAreaWidget()->currentDockWidget(), docks[2]);
            manager.resize(1000 + cycle * 10, 700 + cycle * 10); settle();
            for (auto *dock : docks) if (dock->isVisible()) verifyBounds(dock);
        }
        for (auto *dock : docks) dock->removeEventFilter(this);
        for (auto *view : views) view->removeEventFilter(this);
    }

    void tabsCloseReopenAndRedockRemainInteractive()
    {
        CDockManager manager;
        manager.setPersistentDockWidgetHostsEnabled(true);
        auto *a = new CDockWidget("A"), *b = new CDockWidget("B");
        a->setWidget(new QLineEdit("A")); b->setWidget(new QLineEdit("B"));
        auto *area = manager.addDockWidget(CenterDockWidgetArea, a);
        manager.addDockWidget(CenterDockWidgetArea, b, area);
        manager.resize(900, 600); manager.show(); settle();
        a->setAsCurrentTab(); settle();
        QVERIFY(a->isVisible()); QVERIFY(!b->isVisible()); verifyBounds(a);
        QTest::mouseClick(area->titleBar()->tabBar()->tab(1), Qt::LeftButton);
        settle();
        QVERIFY(b->isVisible()); QVERIFY(!a->isVisible()); verifyBounds(b);
        auto *input = qobject_cast<QLineEdit*>(b->widget());
        QTest::keyClicks(input, " edited");
        QCOMPARE(input->text(), QStringLiteral("B edited"));
        b->toggleView(false); settle();
        QVERIFY(!b->isVisible()); QVERIFY(a->isVisible());
        b->toggleView(true); settle(); QVERIFY(b->isVisible()); verifyBounds(b);
        manager.addDockWidget(BottomDockWidgetArea, b, a->dockAreaWidget()); settle();
        QVERIFY(a->isVisible()); QVERIFY(b->isVisible());
        verifyBounds(a); verifyBounds(b);
        manager.addDockWidget(CenterDockWidgetArea, b, a->dockAreaWidget()); settle();
        QCOMPARE(a->dockAreaWidget(), b->dockAreaWidget());
        QVERIFY(b->isVisible()); QVERIFY(!a->isVisible()); verifyBounds(b);
        manager.hide(); settle(); QVERIFY(!b->isVisible());
        manager.show(); settle(); QVERIFY(b->isVisible()); verifyBounds(b);
    }

    void floatingAndReturningChangesHostOnce()
    {
        CDockManager manager;
        manager.setPersistentDockWidgetHostsEnabled(true);
        auto *a = new CDockWidget("A"), *b = new CDockWidget("B");
        a->setWidget(new QLineEdit("A")); b->setWidget(new QLineEdit("B"));
        auto *area = manager.addDockWidget(CenterDockWidgetArea, a);
        manager.addDockWidget(RightDockWidgetArea, b, area);
        manager.resize(900, 600); manager.show(); settle();
        auto *floating = manager.addDockWidgetFloating(b); settle();
        QVERIFY(floating); QVERIFY(b->isFloating());
        QCOMPARE(b->parentWidget(), b->dockContainer());
        QVERIFY(b->isVisible()); verifyBounds(b);
        manager.addDockWidget(CenterDockWidgetArea, b, a->dockAreaWidget()); settle();
        QVERIFY(!b->isFloating()); QCOMPARE(b->parentWidget(), &manager);
        QVERIFY(b->isVisible()); verifyBounds(b);
    }
};
QTEST_MAIN(PersistentPanelHostTest)
#include "PersistentPanelHostTest.moc"
