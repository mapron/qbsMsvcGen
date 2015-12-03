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

#include "visualstudiosolutionwriter.h"
#include <tools/visualstudioversioninfo.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace qbs {

// Project file GUID (Do NOT change!)
static const QString kProjectFolderGUID = QStringLiteral("{2150E333-8FDC-42A3-9474-1A3956D46DE8}");

VisualStudioSolutionWriter::VisualStudioSolutionWriter(const VisualStudioXmlProjectWriter &projectWriter)
    : m_projectWriter(projectWriter), m_solutionGuid(QUuid::createUuid())
{
}

QString VisualStudioSolutionWriter::fileExtension()
{
    return QStringLiteral(".sln");
}

bool VisualStudioSolutionWriter::write(const MsvsPreparedProject &project, const QString &filePath)
{
    QFile solutionFile(filePath);
    if (!solutionFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream solutionOutStream(&solutionFile);
    solutionOutStream << QStringLiteral("Microsoft Visual Studio Solution File, "
                                        "Format Version %1\n"
                                        "# Visual Studio %2\n")
                         .arg(m_projectWriter.versionInfo().solutionVersion())
                         .arg(m_projectWriter.versionInfo().version().majorVersion());

    foreach (QSharedPointer<MsvsPreparedProduct> product, project.allProducts()) {
        QString relativeProjectFilePath = QDir::toNativeSeparators(QDir(QFileInfo(filePath).path()).relativeFilePath(m_projectWriter.targetFilePath(*product.data(), QFileInfo(filePath).path())));

        solutionOutStream << QStringLiteral("Project(\"%1\") = \"%2\", \"%3\", \"%4\"\n")
                             .arg(m_solutionGuid.toString())
                             .arg(product->name)
                             .arg(relativeProjectFilePath)
                             .arg(product->guid);
        solutionOutStream << "EndProject\n";
    }

    writeProjectSubFolders(solutionOutStream, project);

    solutionOutStream << "Global\n";

    solutionOutStream << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";
    foreach (const MsvsProjectConfiguration &buildTask, project.enabledConfigurations) {
        solutionOutStream << QStringLiteral("\t\t%1 = %1\n").arg(buildTask.fullName());
    }

    solutionOutStream << "\tEndGlobalSection\n";

    solutionOutStream << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";
    foreach (QSharedPointer<MsvsPreparedProduct> product, project.allProducts()) {
        foreach (const MsvsProjectConfiguration &buildTask, product->configurations.keys()) {
            solutionOutStream << QStringLiteral("\t\t%1.%2.ActiveCfg = %2\n"
                                                "\t\t%1.%2.Build.0 = %2\n")
                                 .arg(product->guid)
                                 .arg(buildTask.fullName());
        }
    }

    solutionOutStream << "\tEndGlobalSection\n";
    solutionOutStream << "\tGlobalSection(SolutionProperties) = preSolution\n";
    solutionOutStream << "\t\tHideSolutionNode = FALSE\n";
    solutionOutStream << "\tEndGlobalSection\n";

    solutionOutStream << "\tGlobalSection(NestedProjects) = preSolution\n";
    writeNestedProjects(solutionOutStream, project);
    solutionOutStream << "\tEndGlobalSection\n";
    solutionOutStream << "EndGlobal\n";

    return solutionOutStream.status() == QTextStream::Ok && solutionFile.flush();
}

void VisualStudioSolutionWriter::writeProjectSubFolders(QTextStream &solutionOutStream, const MsvsPreparedProject &project) const
{
    foreach (const MsvsPreparedProject &subProject, project.subProjects) {
        solutionOutStream << QStringLiteral("Project(\"%1\") = \"%2\", \"%2\", \"%3\"\n"
                                            "EndProject\n")
                             .arg(kProjectFolderGUID)
                             .arg(subProject.name)
                             .arg(subProject.guid);
        writeProjectSubFolders(solutionOutStream, subProject);
    }
}

void VisualStudioSolutionWriter::writeNestedProjects(QTextStream &solutionOutStream, const MsvsPreparedProject &project) const
{
    foreach (const MsvsPreparedProject& subProject, project.subProjects)
        writeNestedProjects(solutionOutStream, subProject);

    foreach (QSharedPointer<MsvsPreparedProduct> product, project.products.values())
        solutionOutStream << QStringLiteral("\t\t%1 = %2\n").arg(product->guid).arg(project.guid);
}

} // namespace qbs
