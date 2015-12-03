/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "visualstudioitemgroupfilter.h"
#include <tools/hostosinfo.h>
#include <QFileInfo>

namespace qbs {

VisualStudioItemGroupFilter::VisualStudioItemGroupFilter(const QSet<QString> &extensions, const QString &title, const QString &additionalOptions)
    : extensions(extensions), title(title), additionalOptions(additionalOptions)
{
}

VisualStudioItemGroupFilter::VisualStudioItemGroupFilter(const QString &extensions, const QString &title, const QString &additionalOptions)
    : extensions(QSet<QString>::fromList(extensions.split(Internal::HostOsInfo::pathListSeparator(Internal::HostOsInfo::HostOsWindows), QString::SkipEmptyParts)))
    , title(title)
    , additionalOptions(additionalOptions)
{
}

bool VisualStudioItemGroupFilter::matchesFilter(const QString &filePath) const
{
    return extensions.contains(QFileInfo(filePath).completeSuffix());
}

QList<VisualStudioItemGroupFilter> VisualStudioItemGroupFilter::defaultItemGroupFilters()
{
    // TODO: retrieve tags from groupData.
    return QList<VisualStudioItemGroupFilter> {
        VisualStudioItemGroupFilter(QStringLiteral("c;C;cpp;cxx;c++;cc;def;m;mm"), QStringLiteral("Source Files")),
        VisualStudioItemGroupFilter(QStringLiteral("h;H;hpp;hxx;h++"), QStringLiteral("Header Files")),
        VisualStudioItemGroupFilter(QStringLiteral("ui"), QStringLiteral("Form Files")),
        VisualStudioItemGroupFilter(QStringLiteral("qrc;rc;*"), QStringLiteral("Resource Files"), QStringLiteral("ParseFiles")),
        VisualStudioItemGroupFilter(QStringLiteral("moc"), QStringLiteral("Generated Files"), QStringLiteral("SourceControlFiles")),
        VisualStudioItemGroupFilter(QStringLiteral("ts"), QStringLiteral("Translation Files"), QStringLiteral("ParseFiles"))
    };
}

} // namespace qbs
