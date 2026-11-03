// SPDX-License-Identifier: GPL-2.0-or-later
#include "findergtkbuttons.h"
#include "config-dolphin.h"

#include <QFileInfo>
#include <QSettings>

#if HAVE_GTK3
#include <gtk/gtk.h>
#endif

class FinderGtkWindowButton::Private
{
public:
    QSize size = QSize(20, 20);
#if HAVE_GTK3
    GtkWidget *window = nullptr;
    GtkWidget *header = nullptr;
    GtkWidget *button = nullptr;
    GtkWidget *image = nullptr;
    int kind = 0;
    bool maximized = false;

    ~Private()
    {
        if (window)
            gtk_widget_destroy(window);
    }
#endif
};

FinderGtkWindowButton::FinderGtkWindowButton(int kind, const QString &systemConfigHome)
    : d(std::make_unique<Private>())
{
#if HAVE_GTK3
    static const bool available = gtk_init_check(nullptr, nullptr);
    if (!available)
        return;

    // GTK otherwise reads the isolated Qt profile instead of the desktop's
    // settings.ini. GtkSettings still handles XSettings/GSettings defaults.
    for (const auto &version : {QStringLiteral("gtk-3.0"), QStringLiteral("gtk-4.0")}) {
        const QString path = systemConfigHome + QLatin1Char('/') + version + QStringLiteral("/settings.ini");
        if (!QFileInfo::exists(path))
            continue;
        QSettings config(path, QSettings::IniFormat);
        config.beginGroup(QStringLiteral("Settings"));
        const QByteArray theme = config.value(QStringLiteral("gtk-theme-name")).toString().toUtf8();
        if (!theme.isEmpty()) {
            g_object_set(gtk_settings_get_default(), "gtk-theme-name", theme.constData(), nullptr);
            if (config.contains(QStringLiteral("gtk-application-prefer-dark-theme")))
                g_object_set(gtk_settings_get_default(),
                             "gtk-application-prefer-dark-theme",
                             config.value(QStringLiteral("gtk-application-prefer-dark-theme")).toBool(),
                             nullptr);
        }
        const QByteArray iconTheme = config.value(QStringLiteral("gtk-icon-theme-name")).toString().toUtf8();
        if (!iconTheme.isEmpty())
            g_object_set(gtk_settings_get_default(), "gtk-icon-theme-name", iconTheme.constData(), nullptr);
        break;
    }

    d->kind = kind;
    d->window = gtk_offscreen_window_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(d->window), "csd");
    d->header = gtk_header_bar_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(d->header), "titlebar");
    d->button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(d->button), GTK_RELIEF_NONE);
    auto *context = gtk_widget_get_style_context(d->button);
    gtk_style_context_add_class(context, "titlebutton");
    gtk_style_context_add_class(context, kind == 0 ? "close" : kind == 1 ? "minimize" : "maximize");
    // This widget is rendered on demand by Qt, without a GTK frame clock.
    // Resolve theme states immediately rather than freezing at animation t=0.
    auto *instantStates = gtk_css_provider_new();
    gtk_css_provider_load_from_data(instantStates, "* { transition: none; animation: none; }", -1, nullptr);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(instantStates), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(instantStates);
    d->image = gtk_image_new_from_icon_name(kind == 0       ? "window-close-symbolic"
                                                : kind == 1 ? "window-minimize-symbolic"
                                                            : "window-maximize-symbolic",
                                            GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(d->button), d->image);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(d->header), d->button);
    gtk_container_add(GTK_CONTAINER(d->window), d->header);
    gtk_widget_show_all(d->window);
    GtkRequisition natural;
    gtk_widget_get_preferred_size(d->button, nullptr, &natural);
    d->size = QSize(qBound(16, natural.width, 40), qBound(16, natural.height, 40));
#else
    Q_UNUSED(kind)
    Q_UNUSED(systemConfigHome)
#endif
}

FinderGtkWindowButton::~FinderGtkWindowButton() = default;

QSize FinderGtkWindowButton::sizeHint() const
{
    return d->size;
}

QImage FinderGtkWindowButton::render(bool maximized, bool active, bool hovered, bool pressed, qreal devicePixelRatio)
{
#if HAVE_GTK3
    if (!d->window)
        return {};
    if (d->kind == 2 && d->maximized != maximized) {
        gtk_image_set_from_icon_name(GTK_IMAGE(d->image), maximized ? "window-restore-symbolic" : "window-maximize-symbolic", GTK_ICON_SIZE_MENU);
        d->maximized = maximized;
    }
    auto *windowContext = gtk_widget_get_style_context(d->window);
    if (maximized)
        gtk_style_context_add_class(windowContext, "maximized");
    else
        gtk_style_context_remove_class(windowContext, "maximized");
    const auto backdrop = active ? GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_BACKDROP;
    gtk_widget_set_state_flags(d->window, backdrop, TRUE);
    gtk_widget_set_state_flags(d->header, backdrop, TRUE);
    const auto state = static_cast<GtkStateFlags>(backdrop | (hovered ? GTK_STATE_FLAG_PRELIGHT : 0) | (pressed ? GTK_STATE_FLAG_ACTIVE : 0));
    gtk_widget_set_state_flags(d->button, state, TRUE);
    GtkAllocation allocation{0, 0, d->size.width(), d->size.height()};
    gtk_widget_size_allocate(d->button, &allocation);
    QImage image((QSizeF(d->size) * devicePixelRatio).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    cairo_surface_t *surface = cairo_image_surface_create_for_data(image.bits(), CAIRO_FORMAT_ARGB32, image.width(), image.height(), image.bytesPerLine());
    cairo_t *painter = cairo_create(surface);
    cairo_scale(painter, devicePixelRatio, devicePixelRatio);
    gtk_widget_draw(d->button, painter);
    cairo_destroy(painter);
    cairo_surface_destroy(surface);
    image.setDevicePixelRatio(devicePixelRatio);
    return image;
#else
    Q_UNUSED(maximized)
    Q_UNUSED(active)
    Q_UNUSED(hovered)
    Q_UNUSED(pressed)
    Q_UNUSED(devicePixelRatio)
    return {};
#endif
}
