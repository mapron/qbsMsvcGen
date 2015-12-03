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

#ifndef QBS_VISUALSTUDIOXMLPROJECTWRITER_H
#define QBS_VISUALSTUDIOXMLPROJECTWRITER_H

#include "msvspreparedproject.h"
#include "visualstudioitemgroupfilter.h"
#include <tools/visualstudioversioninfo.h>

QT_BEGIN_NAMESPACE
class QXmlStreamWriter;
class QTextStream;
QT_END_NAMESPACE

namespace qbs {

/*!
 * \brief The VisualStudioXmlProjectWriter class is an abstract class providing the base interface
 * and functionality for the VCBuild and MSBuild project writers.
 */
class VisualStudioXmlProjectWriter
{
public:
    VisualStudioXmlProjectWriter(const Internal::VisualStudioVersionInfo &versionInfo);

    virtual QString projectFileExtension() const = 0;
    QString targetFilePath(const MsvsPreparedProduct &product,
                           const QString &baseBuildDirectory) const;

    virtual bool writeProjectFile(const MsvsPreparedProduct &product,
                                  const QString &baseBuildDirectory) const;

    Internal::VisualStudioVersionInfo versionInfo() const;

protected:
    QString qbsCommandLine(const QString &subCommand,
                           const MsvsPreparedProduct &product,
                           const MsvsProjectConfiguration &buildTask) const;

    typedef QHash<QString, QSet<MsvsProjectConfiguration> > ProjectConfigurations;

    virtual void writeHeader(QXmlStreamWriter &xmlWriter,
                             const MsvsPreparedProduct &product) const = 0;
    virtual void writeConfigurations(QXmlStreamWriter &xmlWriter,
                                     const MsvsPreparedProduct &product,
                                     const QSet<MsvsProjectConfiguration> &allConfigurations) const;
    virtual void writeConfiguration(QXmlStreamWriter &xmlWriter,
                                    const MsvsPreparedProduct &product,
                                    const MsvsProjectConfiguration &buildTask,
                                    const ProductData &productData) const = 0;
    virtual void writeFiles(QXmlStreamWriter &xmlWriter,
                            const QSet<MsvsProjectConfiguration> &allConfigurations,
                            const ProjectConfigurations &allProjectFilesConfigurations) const = 0;
    virtual void writeFooter(QXmlStreamWriter &xmlWriter) const = 0;

    const Internal::VisualStudioVersionInfo m_versionInfo;
    const QList<VisualStudioItemGroupFilter> m_filterOptions;
};

} // namespace qbs

#endif // QBS_VISUALSTUDIOXMLPROJECTWRITER_H
