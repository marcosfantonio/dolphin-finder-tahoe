// SPDX-License-Identifier: GPL-2.0-or-later
#include "finderchrome.h"

#include "config-dolphin.h"
#include "dolphin_generalsettings.h"
#include "dolphin_iconsmodesettings.h"
#include "dolphinmainwindow.h"
#include "dolphinviewcontainer.h"
#include "views/dolphinview.h"

#include <KActionCollection>
#include <KColorScheme>
#include <KConfigGroup>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KFilePlacesView>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KWindowEffects>
#if HAVE_BALOO
#include <KFileMetaData/UserMetaData>
#endif

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QEnterEvent>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QIconEngine>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QToolBar>
#include <QToolButton>
#include <QWindow>
#include <algorithm>

namespace {
// Draw the small Finder-style symbols using the live system foreground color,
// rather than relying on an icon theme having matching light/dark variants.
class FinderIconEngine : public QIconEngine
{
public:
    explicit FinderIconEngine(const QString &symbol)
        : m_symbol(symbol)
    {
    }
    QIconEngine *clone() const override
    {
        return new FinderIconEngine(m_symbol);
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap image(size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        paint(&painter, image.rect(), mode, state);
        return image;
    }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const qreal side = qMin(rect.width(), rect.height());
        p->translate(rect.center().x() - side / 2, rect.center().y() - side / 2);
        p->scale(side / 20, side / 20);
        const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
        const QColor ink = qApp->palette().color(group, QPalette::WindowText);
        p->setPen(QPen(ink, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        if (m_symbol == QLatin1String("back") || m_symbol == QLatin1String("forward")) {
            const bool back = m_symbol == QLatin1String("back");
            p->drawPolyline(QPolygonF{QPointF(back ? 12 : 8, 4), QPointF(back ? 6 : 14, 10), QPointF(back ? 12 : 8, 16)});
        } else if (m_symbol == QLatin1String("icons") || m_symbol == QLatin1String("group")) {
            for (int y : {3, 11})
                for (int x : {3, 11})
                    p->drawRoundedRect(QRectF(x, y, 5, 5), 1, 1);
            if (m_symbol == QLatin1String("group")) {
                p->drawLine(QPointF(17, 7), QPointF(19, 9));
                p->drawLine(QPointF(19, 9), QPointF(21, 7));
            }
        } else if (m_symbol == QLatin1String("details")) {
            for (int y : {5, 10, 15}) {
                p->drawPoint(QPointF(3, y));
                p->drawLine(QPointF(7, y), QPointF(17, y));
            }
        } else if (m_symbol == QLatin1String("compact")) {
            p->drawRoundedRect(QRectF(2, 4, 16, 12), 1, 1);
            p->drawLine(QPointF(7, 4), QPointF(7, 16));
            p->drawLine(QPointF(12, 4), QPointF(12, 16));
        } else if (m_symbol == QLatin1String("preview")) {
            p->drawRoundedRect(QRectF(2, 3, 16, 10), 1, 1);
            p->drawLine(QPointF(2, 16), QPointF(18, 16));
        } else if (m_symbol == QLatin1String("share")) {
            QPainterPath path;
            path.moveTo(5, 8);
            path.lineTo(3, 8);
            path.lineTo(3, 17);
            path.lineTo(17, 17);
            path.lineTo(17, 8);
            path.lineTo(15, 8);
            p->drawPath(path);
            p->drawLine(QPointF(10, 12), QPointF(10, 2));
            p->drawPolyline(QPolygonF{QPointF(6, 6), QPointF(10, 2), QPointF(14, 6)});
        } else if (m_symbol == QLatin1String("tags")) {
            p->drawPolygon(QPolygonF{QPointF(2, 10), QPointF(10, 2), QPointF(17, 2), QPointF(18, 9), QPointF(10, 17)});
            p->drawEllipse(QPointF(14, 6), 1, 1);
        } else if (m_symbol == QLatin1String("search")) {
            p->drawEllipse(QRectF(2, 2, 11, 11));
            p->drawLine(QPointF(12, 12), QPointF(18, 18));
        } else {
            p->setBrush(ink);
            p->setPen(Qt::NoPen);
            for (int x : {4, 10, 16})
                p->drawEllipse(QPointF(x, 10), 1.2, 1.2);
        }
        p->restore();
    }

private:
    QString m_symbol;
};

QIcon finderIcon(const QString &symbol)
{
    return QIcon(new FinderIconEngine(symbol));
}

class FinderHeader : public QWidget
{
public:
    using QWidget::QWidget;
    QSize sizeHint() const override
    {
        return QSize(190, 52);
    }
    QSize minimumSizeHint() const override
    {
        return sizeHint();
    }
};

// Preserve Places' native device actions, group headings and drag-and-drop;
// only recolor its small navigation icons, never the actual file thumbnails.
class FinderPlacesDelegate : public QAbstractItemDelegate
{
public:
    FinderPlacesDelegate(KFilePlacesView *view)
        : QAbstractItemDelegate(view)
        , m_view(view)
        , m_original(view->itemDelegate())
    {
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return m_original->sizeHint(option, index);
    }
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        return m_original->helpEvent(event, view, option, index);
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        opt.palette = qApp->palette();
        opt.palette.setColor(QPalette::HighlightedText, opt.palette.color(QPalette::WindowText));
        m_original->paint(painter, opt, index);
        const int side = m_view->iconSize().width();
        if (side <= 0)
            return;
        QStyleOptionViewItem hint(opt);
        hint.features |= QStyleOptionViewItem::HasDecoration;
        hint.decorationSize = QSize(side, side);
        const int rowHeight = qApp->style()->sizeFromContents(QStyle::CT_ItemViewItem, &hint, {}, opt.widget).height();
        auto icon = index.data(Qt::DecorationRole).value<QIcon>().pixmap(side, side);
        QPainter tint(&icon);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(icon.rect(), qApp->palette().color(QPalette::WindowText));
        tint.end();
        const int x = opt.direction == Qt::LeftToRight ? opt.rect.left() + 4 : opt.rect.right() - 4 - side;
        painter->drawPixmap(x, opt.rect.bottom() + 1 - rowHeight + (rowHeight - side) / 2, icon);
    }

private:
    KFilePlacesView *m_view;
    QAbstractItemDelegate *m_original;
};

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
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(isDown() ? m_color.darker(120) : m_color);
        p.drawEllipse(QRectF(1, 1, 12, 12));
        if (!underMouse()) return;
        p.setPen(QPen(m_color.darker(290), 1.2, Qt::SolidLine, Qt::RoundCap));
        if (m_kind == 0) {
            p.drawLine(QPointF(4.5, 4.5), QPointF(9.5, 9.5));
            p.drawLine(QPointF(9.5, 4.5), QPointF(4.5, 9.5));
        } else if (m_kind == 1) {
            p.drawLine(QPointF(4, 7), QPointF(10, 7));
        } else {
            p.drawLine(QPointF(4, 10), QPointF(10, 4));
            p.drawLine(QPointF(6, 4), QPointF(10, 4));
            p.drawLine(QPointF(10, 4), QPointF(10, 8));
        }
    }
private:
    int m_kind;
    QColor m_color;
};

class FinderAppearance : public QObject
{
public:
    explicit FinderAppearance(QMainWindow *window)
        : QObject(window)
        , m_window(window)
    {
        // The application has an isolated profile; read the real desktop colors,
        // not the stale kdeglobals copy that older installers put in that profile.
        const auto configHome = qEnvironmentVariable("DOLPHIN_FINDER_SYSTEM_CONFIG_HOME", QDir::homePath() + QStringLiteral("/.config"));
        const auto configPath = configHome + QStringLiteral("/kdeglobals");
        m_systemConfig = KSharedConfig::openConfig(configPath, KConfig::SimpleConfig);
        auto *watcher = new QFileSystemWatcher(this);
        watcher->addPaths({configHome, configPath});
        const auto reload = [this, watcher, configPath] {
            // KDE writes configuration atomically, which replaces the watched inode.
            if (!watcher->files().contains(configPath))
                watcher->addPath(configPath);
            m_systemConfig->reparseConfiguration();
            const auto palette = KColorScheme::createApplicationPalette(m_systemConfig);
            if (qApp->palette() != palette)
                qApp->setPalette(palette);
        };
        connect(watcher, &QFileSystemWatcher::fileChanged, this, reload);
        connect(watcher, &QFileSystemWatcher::directoryChanged, this, reload);
        qApp->setPalette(KColorScheme::createApplicationPalette(m_systemConfig));
        qApp->installEventFilter(this);
        apply();
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if ((object == qApp && event->type() == QEvent::ApplicationPaletteChange) || (object == m_window && event->type() == QEvent::PaletteChange))
            apply();
        return false;
    }

private:
    void apply()
    {
        if (m_applying)
            return;
        QScopedValueRollback<bool> guard(m_applying, true);
        const auto palette = qApp->palette();
        const bool dark = palette.color(QPalette::Window).lightness() < 128;
        const auto rgba = [](QColor color, int alpha) {
            return QStringLiteral("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
        };
        const QColor foreground = palette.color(QPalette::WindowText);
        const QString css =
            QStringLiteral(
                "DolphinMainWindow { background: transparent; }"
                "QMainWindow::separator { width: 1px; height: 1px; background: transparent; }"
                "QDockWidget#placesDock { background: %1; border-radius: 14px; color: %2; }"
                "QDockWidget#placesDock QAbstractItemView { background: transparent; border: none; padding: 4px; }"
                "QDockWidget#placesDock QAbstractItemView::item:selected { background: %3; color: %4; border-radius: 5px; }"
                "QMainWindow#finderContent { background: %5; border-top-right-radius: 14px; border-bottom-right-radius: 14px; }"
                "QToolBar#mainToolBar { background: transparent; border: none; spacing: 8px; padding: 4px 8px; }"
                "QWidget#finderCapsule { background: %6; border: 1px solid %7; border-radius: 16px; }"
                "QWidget#finderCapsule QToolButton, QToolButton#finderSearchButton { background: transparent; border: none; border-radius: 13px; padding: 0; }"
                "QWidget#finderCapsule QToolButton:hover, QToolButton#finderSearchButton:hover { background: %8; }"
                "QWidget#finderCapsule QToolButton:checked, QWidget#finderCapsule QToolButton:pressed { background: %9; }"
                "QLabel#finderFolderTitle { color: %2; font-weight: 600; padding-left: 2px; }"
                "QDockWidget#infoDock { background: %5; border: none; }"
                "QDockWidget#infoDock QAbstractScrollArea { border: none; }")
                .arg(QStringLiteral("transparent"),
                     foreground.name(),
                     rgba(palette.color(QPalette::Highlight), dark ? 100 : 45),
                     foreground.name(),
                     palette.color(QPalette::Base).name(),
                     rgba(palette.color(QPalette::Window), 225),
                     rgba(foreground, dark ? 18 : 12),
                     rgba(foreground, 20),
                     rgba(foreground, 35));
        if (m_window->styleSheet() != css)
            m_window->setStyleSheet(css);
        m_window->update();
    }
    QMainWindow *m_window;
    KSharedConfigPtr m_systemConfig;
    bool m_applying = false;
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
        if (!widget || widget->window() != m_window || !m_window->windowHandle())
            return false;
        if (object == m_window && event->type() == QEvent::Show)
            KWindowEffects::enableBlurBehind(m_window->windowHandle(), true);
        if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick)
            return false;
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        const QPoint pos = m_window->mapFromGlobal(mouse->globalPosition().toPoint());
        Qt::Edges edges;
        if (!m_window->isMaximized() && !m_window->isFullScreen()) {
            if (pos.x() < 5)
                edges |= Qt::LeftEdge;
            if (pos.x() >= m_window->width() - 5)
                edges |= Qt::RightEdge;
            if (pos.y() < 5)
                edges |= Qt::TopEdge;
            if (pos.y() >= m_window->height() - 5)
                edges |= Qt::BottomEdge;
        }
        if (edges && event->type() == QEvent::MouseButtonPress)
            return m_window->windowHandle()->startSystemResize(edges);
        if (widget == m_header || widget == m_toolbar || widget->objectName() == QLatin1String("finderFolderTitle")) {
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

void setFinderLocationVisible(QMainWindow *window, bool visible)
{
    auto *dolphin = qobject_cast<DolphinMainWindow *>(window);
    if (auto *title = window->findChild<QAction *>(QStringLiteral("finderFolderTitleAction"))) {
        title->setVisible(!visible);
        dolphin->actionCollection()->action(QStringLiteral("url_navigators"))->setVisible(visible);
    }
}

void installFinderChrome(QMainWindow *window, QDockWidget *places, QToolBar *toolbar)
{
    auto *dolphin = qobject_cast<DolphinMainWindow *>(window);
    Q_ASSERT(dolphin && places && toolbar);
    auto *actions = dolphin->actionCollection();
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("FinderChrome"));
    if (config.readEntry("LayoutVersion", 0) < 2) {
        IconsModeSettings::setIconSize(64);
        IconsModeSettings::setPreviewSize(64);
        IconsModeSettings::setTextWidthIndex(1);
        IconsModeSettings::setMaximumTextLines(2);
        IconsModeSettings::self()->save();
        GeneralSettings::setShowStatusBar(GeneralSettings::EnumShowStatusBar::Disabled);
        GeneralSettings::self()->save();
        for (auto *dock : window->findChildren<QDockWidget *>())
            if (dock != places)
                dock->hide();
        window->resize(1000, 660);
        config.writeEntry("LayoutVersion", 2);
        config.sync();
    }
    window->setContentsMargins(6, 6, 6, 6);
    window->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

    QWidget *tabs = window->takeCentralWidget();
    window->removeToolBar(toolbar);
    auto *content = new QMainWindow(window);
    content->setWindowFlags(Qt::Widget);
    content->setObjectName(QStringLiteral("finderContent"));
    content->setMinimumWidth(490);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setFixedHeight(52);
    toolbar->clear();

    auto capsule = [toolbar]() {
        auto *group = new QWidget(toolbar);
        group->setObjectName(QStringLiteral("finderCapsule"));
        auto *layout = new QHBoxLayout(group);
        layout->setContentsMargins(3, 2, 3, 2);
        layout->setSpacing(0);
        group->setFixedHeight(32);
        toolbar->addWidget(group);
        return group;
    };
    auto addButton = [](QWidget *group, QAction *action, const QString &symbol) {
        auto *button = new QToolButton(group);
        if (action)
            action->setIcon(finderIcon(symbol));
        button->setDefaultAction(action);
        button->setObjectName(QStringLiteral("finder_%1").arg(symbol));
        button->setIconSize(QSize(17, 17));
        button->setFixedSize(30, 28);
        if (action && action->menu()) {
            button->setPopupMode(QToolButton::InstantPopup);
            button->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0px; }"));
        }
        group->layout()->addWidget(button);
        return button;
    };
    auto *navigation = capsule();
    addButton(navigation, actions->action(QStringLiteral("go_back")), QStringLiteral("back"));
    addButton(navigation, actions->action(QStringLiteral("go_forward")), QStringLiteral("forward"));

    auto *title = new QLabel(toolbar);
    title->setObjectName(QStringLiteral("finderFolderTitle"));
    title->setMinimumWidth(60);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *titleAction = toolbar->addWidget(title);
    titleAction->setObjectName(QStringLiteral("finderFolderTitleAction"));
    auto *navigators = actions->action(QStringLiteral("url_navigators"));
    toolbar->addAction(navigators);
    navigators->setVisible(false);
    const auto updateTitle = [dolphin, title, navigators, titleAction] {
        const auto url = dolphin->activeViewContainer()->url();
        const QString name = url.fileName().isEmpty() ? url.toDisplayString(QUrl::PreferLocalFile) : url.fileName();
        title->setText(title->fontMetrics().elidedText(name, Qt::ElideRight, qMax(60, title->width() - 8)));
        title->setToolTip(url.toDisplayString(QUrl::PreferLocalFile));
        navigators->setVisible(false);
        titleAction->setVisible(true);
    };
    QObject::connect(dolphin, &DolphinMainWindow::urlChanged, title, updateTitle);
    updateTitle();

    auto *views = capsule();
    for (const QString &name : {QStringLiteral("icons"), QStringLiteral("details"), QStringLiteral("compact")}) {
        addButton(views, actions->action(name), name);
    }
    auto *previewAction = actions->action(QStringLiteral("show_information_panel"));
    if (!previewAction)
        previewAction = actions->action(QStringLiteral("show_preview"));
    auto *preview = addButton(views, previewAction, QStringLiteral("preview"));
    preview->setToolTip(i18n("Show preview and information"));

    auto *group = capsule();
    auto *groupAction = new QAction(i18n("Sort and group"), toolbar);
    auto *groupMenu = new QMenu(toolbar);
    for (const auto &name : {QStringLiteral("sort"), QStringLiteral("group_by"), QStringLiteral("view_settings")}) {
        if (auto *action = actions->action(name))
            groupMenu->addAction(action);
    }
    groupAction->setMenu(groupMenu);
    addButton(group, groupAction, QStringLiteral("group"));

    auto *utilities = capsule();
    auto *shareAction = new QAction(i18n("Share"), toolbar);
    auto *shareMenu = new QMenu(toolbar);
    auto *fileActions = new KFileItemActions(shareMenu);
    fileActions->setParentWidget(window);
    QObject::connect(shareMenu, &QMenu::aboutToShow, shareMenu, [dolphin, actions, shareMenu, fileActions] {
        shareMenu->clear();
        auto items = dolphin->activeViewContainer()->view()->selectedItems();
        if (items.isEmpty())
            items.append(KFileItem(dolphin->activeViewContainer()->url()));
        fileActions->setItemListProperties(KFileItemListProperties(items));
        fileActions->addActionsTo(shareMenu);
        shareMenu->addSeparator();
        shareMenu->addAction(actions->action(QStringLiteral("copy_location")));
    });
    shareAction->setMenu(shareMenu);
    addButton(utilities, shareAction, QStringLiteral("share"));

#if HAVE_BALOO
    auto *tagsAction = new QAction(i18n("Edit tags…"), toolbar);
    QObject::connect(tagsAction, &QAction::triggered, window, [dolphin, window] {
        const auto items = dolphin->activeViewContainer()->view()->selectedItems();
        if (items.isEmpty())
            return;
        QStringList commonTags;
        bool first = true;
        for (const auto &item : items) {
            if (!item.url().isLocalFile())
                return;
            KFileMetaData::UserMetaData metadata(item.url().toLocalFile());
            if (first) {
                commonTags = metadata.tags();
                first = false;
            } else {
                const auto tags = metadata.tags();
                commonTags.removeIf([&tags](const QString &tag) {
                    return !tags.contains(tag);
                });
            }
        }
        bool accepted = false;
        const auto text =
            QInputDialog::getText(window, i18n("Tags"), i18n("Tags separated by commas:"), QLineEdit::Normal, commonTags.join(QStringLiteral(", ")), &accepted);
        if (!accepted)
            return;
        QStringList tags;
        for (const auto &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (!part.trimmed().isEmpty())
                tags.append(part.trimmed());
        }
        tags.removeDuplicates();
        for (const auto &item : items) {
            KFileMetaData::UserMetaData metadata(item.url().toLocalFile());
            // Only replace the common tags; keep each file's individual tags.
            auto updated = metadata.tags();
            for (const auto &tag : commonTags)
                updated.removeAll(tag);
            updated.append(tags);
            updated.removeDuplicates();
            metadata.setTags(updated);
        }
    });
    const auto updateTags = [dolphin, tagsAction] {
        const auto items = dolphin->activeViewContainer()->view()->selectedItems();
        tagsAction->setEnabled(!items.isEmpty() && std::all_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
            return item.url().isLocalFile();
        }));
    };
    QObject::connect(dolphin, &DolphinMainWindow::selectionChanged, toolbar, updateTags);
    updateTags();
    addButton(utilities, tagsAction, QStringLiteral("tags"));
#endif
    addButton(utilities, actions->action(QStringLiteral("hamburger_menu")), QStringLiteral("more"));
    auto *search = new QToolButton(toolbar);
    search->setObjectName(QStringLiteral("finderSearchButton"));
    search->setDefaultAction(actions->action(QStringLiteral("toggle_search")));
    search->defaultAction()->setIcon(finderIcon(QStringLiteral("search")));
    search->setFixedSize(30, 30);
    toolbar->addWidget(search);

    content->addToolBar(toolbar);
    content->setCentralWidget(tabs);
    window->setCentralWidget(content);
    toolbar->show();

    // Put the inspector beneath the content toolbar, not beside its right edge.
    if (auto *info = window->findChild<QDockWidget *>(QStringLiteral("infoDock"))) {
        const bool visible = !info->isHidden();
        window->removeDockWidget(info);
        content->addDockWidget(Qt::RightDockWidgetArea, info);
        info->setTitleBarWidget(new QWidget(info));
        info->setMinimumWidth(210);
        // Qt disables toggleViewAction when DockWidgetClosable is removed.
        // The title bar remains hidden, but the preview toolbar control must work.
        info->setFeatures(QDockWidget::DockWidgetClosable);
        content->resizeDocks({info}, {245}, Qt::Horizontal);
        info->setVisible(visible);
    }

    places->setAllowedAreas(Qt::LeftDockWidgetArea);
    places->setFeatures(QDockWidget::NoDockWidgetFeatures);
    // DolphinDockWidget paints the translucent background itself. A styled
    // background would blend a second layer over it and make it more opaque.
    places->setAttribute(Qt::WA_StyledBackground, false);
    window->addDockWidget(Qt::LeftDockWidgetArea, places);
    places->setFixedWidth(190);
    places->show();
    places->toggleViewAction()->setEnabled(false);
    places->widget()->setAutoFillBackground(false);
    for (auto *child : places->findChildren<QWidget *>())
        child->setAutoFillBackground(false);
    for (auto *view : places->findChildren<KFilePlacesView *>()) {
        view->setIconSize(QSize(16, 16));
        // Keep itemDelegate() as KFilePlacesView's native delegate: its mouse
        // event filter casts that object to a private KDE type. Override only
        // column painting, which Qt dispatches via itemDelegateForIndex().
        view->setItemDelegateForColumn(0, new FinderPlacesDelegate(view));
    }
    auto *header = new FinderHeader(places);
    header->setFixedHeight(52);
    auto *buttons = new QHBoxLayout(header);
    buttons->setContentsMargins(14, 0, 12, 0);
    buttons->setSpacing(7);
    const QStringList colors{QStringLiteral("#ff5f57"), QStringLiteral("#febc2e"), QStringLiteral("#28c840")};
    const QStringList labels{i18n("Close"), i18n("Minimize"), i18n("Maximize / restore")};
    for (int i = 0; i < 3; ++i) {
        auto *button = new TrafficLightButton(i, QColor(colors[i]), header);
        button->setFixedSize(14, 14);
        button->setToolTip(labels[i]);
        button->setAccessibleName(labels[i]);
        QObject::connect(button, &QToolButton::clicked, window, [window, i] {
            if (i == 0) window->close();
            else if (i == 1) window->showMinimized();
            else if (window->isMaximized()) window->showNormal();
            else window->showMaximized();
        });
        buttons->addWidget(button);
    }
    buttons->addStretch();
    places->setTitleBarWidget(header);
    new FinderAppearance(window);
    new WindowGestures(window, header, toolbar);
    if (window->windowHandle())
        KWindowEffects::enableBlurBehind(window->windowHandle(), true);
}
