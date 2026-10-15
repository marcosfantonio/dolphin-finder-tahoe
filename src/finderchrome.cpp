// SPDX-License-Identifier: GPL-2.0-or-later
#include "finderchrome.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KWindowEffects>
#include <QApplication>
#include <QDockWidget>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace
{
class TrafficLightButton : public QToolButton
{
public:
    TrafficLightButton(int kind, const QColor &color, QWidget *parent)
        : QToolButton(parent)
        , m_kind(kind)
        , m_color(color)
    {
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        QToolButton::enterEvent(event);
        update();
    }
    void leaveEvent(QEvent *event) override
    {
        QToolButton::leaveEvent(event);
        update();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(isDown() ? m_color.darker(120) : m_color);
        painter.drawEllipse(QRectF(0, 0, 14, 14));
        if (!underMouse())
            return;

        const QColor ink = m_color.darker(290);
        painter.setPen(QPen(ink, 1.25, Qt::SolidLine, Qt::RoundCap));
        if (m_kind == 0) {
            painter.drawLine(QPointF(4.5, 4.5), QPointF(9.5, 9.5));
            painter.drawLine(QPointF(9.5, 4.5), QPointF(4.5, 9.5));
        } else if (m_kind == 1) {
            painter.drawLine(QPointF(3.5, 7), QPointF(10.5, 7));
        } else {
            // macOS-style diagonal expand/restore triangles.
            painter.setPen(Qt::NoPen);
            painter.setBrush(ink);
            if (window()->isMaximized()) {
                painter.drawPolygon(QPolygonF{QPointF(6, 6), QPointF(2.5, 6), QPointF(6, 2.5)});
                painter.drawPolygon(QPolygonF{QPointF(8, 8), QPointF(11.5, 8), QPointF(8, 11.5)});
            } else {
                painter.drawPolygon(QPolygonF{QPointF(3.5, 3.5), QPointF(8, 3.5), QPointF(3.5, 8)});
                painter.drawPolygon(QPolygonF{QPointF(10.5, 10.5), QPointF(6, 10.5), QPointF(10.5, 6)});
            }
        }
    }

private:
    int m_kind;
    QColor m_color;
};

class FinderHeader : public QWidget
{
public:
    using QWidget::QWidget;
    QSize sizeHint() const override
    {
        return QSize(240, 48);
    }
    QSize minimumSizeHint() const override
    {
        return sizeHint();
    }
};

class WindowGestures : public QObject
{
public:
    WindowGestures(QMainWindow *window, QWidget *header, QToolBar *toolbar)
        : QObject(window)
        , m_window(window)
        , m_header(header)
        , m_toolbar(toolbar)
    {
        qApp->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto *widget = qobject_cast<QWidget *>(object);
        if (!widget || widget->window() != m_window || !m_window->windowHandle()) {
            return false;
        }
        if (object == m_window && event->type() == QEvent::Show) {
            KWindowEffects::enableBlurBehind(m_window->windowHandle(), true);
        }
        if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick) {
            return false;
        }
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        const QPoint pos = m_window->mapFromGlobal(mouse->globalPosition().toPoint());
        Qt::Edges edges;
        if (!m_window->isMaximized() && !m_window->isFullScreen()) {
            if (pos.x() < 6)
                edges |= Qt::LeftEdge;
            if (pos.x() >= m_window->width() - 6)
                edges |= Qt::RightEdge;
            if (pos.y() < 6)
                edges |= Qt::TopEdge;
            if (pos.y() >= m_window->height() - 6)
                edges |= Qt::BottomEdge;
        }
        if (edges && event->type() == QEvent::MouseButtonPress) {
            return m_window->windowHandle()->startSystemResize(edges);
        }
        if (widget == m_header || widget == m_toolbar) {
            if (event->type() == QEvent::MouseButtonDblClick) {
                m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized();
                return true;
            }
            return m_window->windowHandle()->startSystemMove();
        }
        return false;
    }

private:
    QMainWindow *m_window;
    QWidget *m_header;
    QToolBar *m_toolbar;
};
}

void installFinderChrome(QMainWindow *window, QDockWidget *places, QToolBar *toolbar)
{
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("FinderChrome"));
    if (config.readEntry("LayoutVersion", 0) < 1) {
        for (auto *dock : window->findChildren<QDockWidget *>()) {
            if (dock != places)
                dock->hide();
        }
        window->resize(1000, 700);
        config.writeEntry("LayoutVersion", 1);
        config.sync();
    }
    window->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    window->setContentsMargins(6, 6, 6, 6);
    window->setStyleSheet(
        QStringLiteral("DolphinMainWindow { background: transparent; }"
                       "QMainWindow::separator { width: 6px; background: transparent; }"
                       "QDockWidget#placesDock QAbstractItemView { background: transparent; border: none; padding: 6px; }"
                       "QDockWidget#placesDock QAbstractItemView::item:selected { background: rgba(255,255,255,28); color: #3584e4; border-radius: 6px; }"
                       "QWidget#finderContent { background: #1e1e20; border-radius: 18px; }"
                       "QToolBar { background: transparent; border: none; spacing: 6px; padding: 4px; }"));

    // Keep the toolbar in the content column so Places extends to the top.
    QWidget *tabs = window->takeCentralWidget();
    window->removeToolBar(toolbar);
    auto *content = new QMainWindow(window);
    content->setWindowFlags(Qt::Widget);
    content->setMinimumWidth(450);
    content->setObjectName(QStringLiteral("finderContent"));
    content->setContentsMargins(6, 0, 6, 6);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setFixedHeight(48);
    content->addToolBar(Qt::TopToolBarArea, toolbar);
    content->setCentralWidget(tabs);
    window->setCentralWidget(content);
    toolbar->show();

    places->setAllowedAreas(Qt::LeftDockWidgetArea);
    places->setAttribute(Qt::WA_StyledBackground, true);
    places->setFeatures(QDockWidget::NoDockWidgetFeatures);
    window->addDockWidget(Qt::LeftDockWidgetArea, places);
    places->setFixedWidth(240);
    places->show();
    places->toggleViewAction()->setEnabled(false);
    window->resizeDocks({places}, {240}, Qt::Horizontal);
    places->widget()->setAutoFillBackground(false);
    for (auto *child : places->findChildren<QWidget *>()) {
        child->setAutoFillBackground(false);
    }

    auto *header = new FinderHeader(places);
    header->setFixedHeight(48);
    auto *buttons = new QHBoxLayout(header);
    buttons->setContentsMargins(16, 0, 12, 0);
    auto *lights = new QWidget(header);
    auto *lightsLayout = new QHBoxLayout(lights);
    lightsLayout->setContentsMargins(0, 0, 0, 0);
    lightsLayout->setSpacing(10);
    lights->setFixedSize(62, 18);
    buttons->addWidget(lights);
    const QStringList colors{QStringLiteral("#ff5f57"), QStringLiteral("#febc2e"), QStringLiteral("#28c840")};
    const QStringList labels{QStringLiteral("Fechar"), QStringLiteral("Minimizar"), QStringLiteral("Maximizar/restaurar")};
    for (int i = 0; i < 3; ++i) {
        auto *button = new TrafficLightButton(i, QColor(colors[i]), lights);
        button->setFixedSize(14, 14);
        button->setToolTip(labels[i]);
        button->setAccessibleName(labels[i]);
        QObject::connect(button, &QToolButton::clicked, window, [window, i] {
            if (i == 0)
                window->close();
            else if (i == 1)
                window->showMinimized();
            else if (window->isMaximized())
                window->showNormal();
            else
                window->showMaximized();
        });
        lightsLayout->addWidget(button);
    }
    buttons->addStretch();
    places->setTitleBarWidget(header);
    new WindowGestures(window, header, toolbar);
    if (window->windowHandle()) {
        KWindowEffects::enableBlurBehind(window->windowHandle(), true);
    }
}
