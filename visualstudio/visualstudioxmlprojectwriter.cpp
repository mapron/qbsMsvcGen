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

#include "visualstudioxmlprojectwriter.h"
#include "visualstudiosolutionwriter.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUuid>
#include <QXmlStreamWriter>

#include <logging/translator.h>
#include <tools/shellutils.h>

using namespace qbs;

VisualStudioXmlProjectWriter::VisualStudioXmlProjectWriter(const Internal::VisualStudioVersionInfo &versionInfo)
    : m_versionInfo(versionInfo)
    , m_filterOptions(VisualStudioItemGroupFilter::defaultItemGroupFilters())
{
}

QString VisualStudioXmlProjectWriter::targetFilePath(const MsvsPreparedProduct &product, const QString &baseBuildDirectory) const
{
    return QDir(baseBuildDirectory).absoluteFilePath(product.name + projectFileExtension());
}

bool VisualStudioXmlProjectWriter::writeProjectFile(const MsvsPreparedProduct &product, const QString &baseBuildDirectory) const
{
    ProjectConfigurations allProjectFilesConfigurations;
    const QSet<MsvsProjectConfiguration> allConfigurations = product.configurations.keys().toSet();
    foreach (const MsvsProjectConfiguration &buildTask, allConfigurations) {
        const ProductData &productData = product.configurations[buildTask];
        foreach (const GroupData &groupData, productData.groups()) {
            if (groupData.isEnabled()) {
                foreach (const QString &filePath, groupData.allFilePaths()) {
                    allProjectFilesConfigurations[filePath] << buildTask;
                }
            }
        }
    }

    QFile file(targetFilePath(product, baseBuildDirectory));
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);

    writeHeader(xmlWriter, product);
    writeConfigurations(xmlWriter, product, allConfigurations);
    writeFiles(xmlWriter, allConfigurations, allProjectFilesConfigurations);
    writeFooter(xmlWriter);

    return !xmlWriter.hasError() && file.flush();
}

Internal::VisualStudioVersionInfo VisualStudioXmlProjectWriter::versionInfo() const
{
    return m_versionInfo;
}

QString VisualStudioXmlProjectWriter::qbsCommandLine(const QString &subCommand,
                                       const MsvsPreparedProduct &product,
                                       const MsvsProjectConfiguration &buildTask) const
{
    // "path/to/qbs.exe" {build|clean} -f "path/to/project.qbs" -d "/build/directory/" -p product_name {debug|release} profile:<profileName>
    QStringList commandLineArgs = QStringList()
            << QStringLiteral("-f") << QDir::toNativeSeparators(buildTask.qbsProjectFile)
            << QStringLiteral("-d") << QDir::toNativeSeparators(buildTask.buildDirectory)
            << QStringLiteral("-p") << product.name
            << buildTask.variant
            << QStringLiteral("profile:") + buildTask.profile
            << buildTask.commandLineParameters;

    if (subCommand == QStringLiteral("install") && !buildTask.installRoot.isEmpty())
        commandLineArgs = QStringList() << QStringLiteral("--install-root")
                                        << QDir::toNativeSeparators(buildTask.installRoot)
                                        << commandLineArgs;

    return Internal::shellQuote(QDir::toNativeSeparators(buildTask.qbsExecutablePath),
                                QStringList() << subCommand << commandLineArgs,
                                Internal::HostOsInfo::HostOsWindows);
}

void VisualStudioXmlProjectWriter::writeConfigurations(QXmlStreamWriter &xmlWriter,
                                                const MsvsPreparedProduct &product,
                                                const QSet<MsvsProjectConfiguration> &allConfigurations) const
{
    for (const MsvsProjectConfiguration &buildTask : allConfigurations)
        writeConfiguration(xmlWriter, product, buildTask, product.configurations[buildTask]);
}
