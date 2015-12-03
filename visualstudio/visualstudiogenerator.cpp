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

#include "visualstudiogenerator.h"
#include "msbuildprojectwriter.h"
#include "vcbuildprojectwriter.h"
#include "visualstudiosolutionwriter.h"

#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/shellutils.h>
#include <tools/visualstudioversioninfo.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace qbs;
using namespace qbs::Internal;

VisualStudioGenerator::VisualStudioGenerator(const VisualStudioVersionInfo &versionInfo)
    : m_versionInfo(versionInfo)
{
}

QString VisualStudioGenerator::generatorName() const
{
    return QStringLiteral("visualstudio%1").arg(m_versionInfo.marketingVersion());
}

void VisualStudioGenerator::setupGenerator()
{
    QSet<QString> profileNames;
    QSet<QString> projectNames;
    QSet<QString> qbsProjectFiles;
    QSet<QString> buildDirectories;

    foreach (const qbs::Project &proj, projects()) {
        profileNames << proj.profile();
        projectNames << proj.projectData().name();
        qbsProjectFiles << proj.projectData().location().filePath();

        QDir baseBuildDirectory(proj.projectData().buildDirectory());
        baseBuildDirectory.cdUp();
        buildDirectories << baseBuildDirectory.absolutePath();
    }

    QBS_CHECK(!profileNames.isEmpty());
    QBS_CHECK(projectNames.size() == 1);
    QBS_CHECK(qbsProjectFiles.size() == 1);
    QBS_CHECK(buildDirectories.size() == 1);

    m_multipleProfiles = profileNames.size() > 1;
    m_projectName = projectNames.toList().first();
    m_qbsProjectFile = qbsProjectFiles.toList().first();
    m_baseBuildDirectory = buildDirectories.toList().first();

    QBS_CHECK(m_qbsProjectFile.isAbsolute() && m_qbsProjectFile.exists());
    QBS_CHECK(m_baseBuildDirectory.isAbsolute() && m_baseBuildDirectory.exists());

    const QString qbsInstallDir = QString::fromLocal8Bit(qgetenv("QBS_INSTALL_DIR"));
    m_qbsExecutableFile = HostOsInfo::appendExecutableSuffix(!qbsInstallDir.isEmpty()
            ? qbsInstallDir + QLatin1String("/bin/qbs")
            : QCoreApplication::applicationDirPath() + QLatin1String("/qbs"));

    QBS_CHECK(m_qbsExecutableFile.isAbsolute() && m_qbsExecutableFile.exists());
}

void VisualStudioGenerator::generate(const InstallOptions &installOptions)
{
    setupGenerator();

    MsvsPreparedProject project;
    foreach (const Project &qbsProject, projects()) {
        const MsvsProjectConfiguration config(qbsProject,
                                              m_qbsExecutableFile.absoluteFilePath(),
                                              m_qbsProjectFile.absoluteFilePath(),
                                              m_baseBuildDirectory.absolutePath(),
                                              installOptions.installRoot(),
                                              !m_multipleProfiles);
        project.prepare(qbsProject, installOptions, qbsProject.projectData(), config);
    }

    QSharedPointer<VisualStudioXmlProjectWriter> writer;
    if (m_versionInfo.usesMsBuild())
        writer = QSharedPointer<MSBuildProjectWriter>::create(m_versionInfo);
    else if (m_versionInfo.usesVcBuild())
        writer = QSharedPointer<VCBuildProjectWriter>::create(m_versionInfo);
    else
        throw ErrorInfo(Tr::tr("Failed to generate project for unknown build engine"));

    for (QSharedPointer<MsvsPreparedProduct> product : project.allProducts())
        if (!writer->writeProjectFile(*product.data(), m_baseBuildDirectory.absolutePath()))
            throw ErrorInfo(Tr::tr("Failed to generate %1").arg(product->name + writer->projectFileExtension()));

    if (m_versionInfo.usesSolutions()) {
        VisualStudioSolutionWriter solutionWriter(*writer.data());
        const QString solutionFilePath = m_baseBuildDirectory.absoluteFilePath(m_projectName + solutionWriter.fileExtension());
        if (!solutionWriter.write(project, solutionFilePath))
            throw ErrorInfo(Tr::tr("Failed to generate %1").arg(QFileInfo(solutionFilePath).fileName()));

        qDebug() << "Generated" << qPrintable(QFileInfo(solutionFilePath).fileName());
    }
}

QList<QSharedPointer<ProjectGenerator> > VisualStudioGenerator::createGeneratorList()
{
    QList<QSharedPointer<ProjectGenerator> > result;
    for (const VisualStudioVersionInfo &info : VisualStudioVersionInfo::knownVersions())
        result << QSharedPointer<ProjectGenerator>(new VisualStudioGenerator(info));
    return result;
}
