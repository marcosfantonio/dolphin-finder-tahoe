// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef FINDERGTKBUTTONS_H
#define FINDERGTKBUTTONS_H

#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

// Render GTK's actual titlebutton CSS and symbolic icons, rather than guessing
// asset names inside a GTK theme (which may use CSS or compiled resources).
class FinderGtkWindowButton
{
public:
    FinderGtkWindowButton(int kind, const QString &systemConfigHome);
    ~FinderGtkWindowButton();
    QSize sizeHint() const;
    QImage render(bool maximized, bool active, bool hovered, bool pressed, qreal devicePixelRatio);

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif
