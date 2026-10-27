// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef FINDERPOPUPSTYLE_H
#define FINDERPOPUPSTYLE_H

#include <QMenu>
#include <QPalette>
#include <QWidget>

inline void applyFinderPopupStyle(QWidget *popup, const QPalette &palette)
{
    popup->setPalette(palette);
    popup->setAttribute(Qt::WA_TranslucentBackground);
    popup->setAutoFillBackground(false);
    const QColor windowColor = palette.color(QPalette::Window);
    const QString background = QStringLiteral("rgba(%1,%2,%3,185)").arg(windowColor.red()).arg(windowColor.green()).arg(windowColor.blue());
    const QString foreground = palette.color(QPalette::WindowText).name();
    const QString arrowColor = palette.color(QPalette::Window).lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black");
    if (qobject_cast<QMenu *>(popup)) {
        popup->setStyleSheet(QStringLiteral("QMenu { background-color: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 5px; }"
                                            "QMenu::item { background: transparent; color: %2; padding: 6px 24px; border-radius: 4px; }"
                                            "QMenu::item:selected { background-color: %4; color: %5; }"
                                            "QMenu::item:disabled { color: %6; }"
                                            "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }"
                                            "QMenu::right-arrow { image: url(:/dolphin-finder/icons/menu-arrow-right-%7.svg); width: 8px; height: 12px; }"
                                            "QMenu::left-arrow { image: url(:/dolphin-finder/icons/menu-arrow-left-%7.svg); width: 8px; height: 12px; }")
                                 .arg(background,
                                      foreground,
                                      palette.color(QPalette::Mid).name(),
                                      palette.color(QPalette::Highlight).name(),
                                      palette.color(QPalette::HighlightedText).name(),
                                      palette.color(QPalette::Disabled, QPalette::WindowText).name(),
                                      arrowColor));
    } else if (popup->windowType() == Qt::ToolTip) {
        popup->setStyleSheet(QStringLiteral("QLabel { background-color: %1; color: %2; border: 1px solid %3; border-radius: 5px; padding: 4px; }")
                                 .arg(background, foreground, palette.color(QPalette::Mid).name()));
    }
}

#endif
