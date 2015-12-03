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

#ifndef MSVS_PREPARED_PROJECT_H
#define MSVS_PREPARED_PROJECT_H

#include <qbs.h>

namespace qbs
{
    struct MsvsProjectConfiguration
    {
        QString profile;
        QString variant;
        QString platform;

        QString qbsExecutablePath;
        QString qbsProjectFile;
        QString buildDirectory;
        QString installRoot;
        QStringList commandLineParameters;
        bool useSimplifiedConfigurationNames = false;

        MsvsProjectConfiguration();
        MsvsProjectConfiguration(const Project &project,
                                 const QString &qbsExecutablePath,
                                 const QString &qbsProjectFile,
                                 const QString &buildDirectory,
                                 const QString &installRoot,
                                 bool useSimplifiedConfigurationNames);
        QString cleanProfileName() const;
        QString fullName() const;
        QString profileAndVariant() const;
        bool operator<(const MsvsProjectConfiguration &right) const;
        bool operator==(const MsvsProjectConfiguration &right) const;
    };

    quint32 qHash(const MsvsProjectConfiguration &config);

    struct MsvsPreparedProduct
    {
        QMap<MsvsProjectConfiguration, ProductData> configurations;
        QString name;
        QString targetName;
        QString targetPath;
        QString guid;
        bool isApplication;
        QStringList uniquePlatforms() const;
    };

    struct MsvsPreparedProject
    {
        QList<MsvsProjectConfiguration> enabledConfigurations;
        QString name;
        QString guid;
        QMap<QString, MsvsPreparedProject> subProjects;
        QMap<QString, QSharedPointer<MsvsPreparedProduct>> products;

        QList<QSharedPointer<MsvsPreparedProduct>> allProducts() const;
        void prepare(const Project &qbsProject,
                     const InstallOptions &installOptions,
                     const ProjectData &projectData,
                     const MsvsProjectConfiguration &config);
    };
}

#endif // MSVS_PREPARED_PROJECT_H
