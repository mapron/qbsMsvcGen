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

#include "msvspreparedproject.h"

#include <QDebug>
#include <QFileInfo>
#include <QTextBoundaryFinder>
#include <QUuid>

using namespace qbs;

static const QString visualStudioArchitectureName(const QString &qbsArch)
{
    static const QMap<QString, QString> map {
        {QStringLiteral("x86"), QStringLiteral("Win32")},
        {QStringLiteral("x86_64"), QStringLiteral("Win64")},
        {QStringLiteral("ia64"), QStringLiteral("Itanium")},
        {QStringLiteral("arm"), QStringLiteral("Arm")},
        {QStringLiteral("arm64"), QStringLiteral("Arm64")}
    };
    return map[qbsArch];
}

QList<QSharedPointer<MsvsPreparedProduct> > MsvsPreparedProject::allProducts() const
{
    QList<QSharedPointer<MsvsPreparedProduct> > result = products.values();
    foreach (const MsvsPreparedProject &child, subProjects)
        result << child.allProducts();
    return result;
}

void MsvsPreparedProject::prepare(const Project& qbsProject,
                                  const InstallOptions& installOptions,
                                  const ProjectData& projectData,
                                  const MsvsProjectConfiguration& config)
{
    foreach (const ProjectData &subData, projectData.subProjects()) {
        if (!subProjects.contains(subData.name())) {
            MsvsPreparedProject subPrepared;
            subPrepared.guid = QUuid::createUuid().toString();
            subPrepared.name = subData.name();
            subProjects[subData.name()] = subPrepared;
        }
        subProjects[subData.name()].prepare(qbsProject, installOptions, subData, config);
    }

    if (!projectData.isEnabled() || !projectData.isValid() || projectData.products().isEmpty())
        return;

    foreach (const ProductData &productData, projectData.products()) {
        if (!products.contains(productData.name())) {
            QSharedPointer<MsvsPreparedProduct> product(new MsvsPreparedProduct());
            product->guid = QUuid::createUuid().toString();
            product->name = productData.name();
            QString buildDirectory = productData.properties().value(QStringLiteral("buildDirectory")).toString();
            product->isApplication = productData.properties().value(QStringLiteral("type")).toStringList().contains(QStringLiteral("application"));
            QString fullPath = qbsProject.targetExecutable(productData, installOptions);
            if (!fullPath.isEmpty()) {
                product->targetName = QFileInfo(fullPath).fileName();
                product->targetPath = QFileInfo(fullPath).absolutePath();
            } else {
                product->targetName = productData.targetName();
                product->targetPath = installOptions.installRoot();
            }
            if (product->targetPath.isEmpty())
                product->targetPath = buildDirectory;
            product->targetPath += QLatin1Char('/');
            products.insert(product->name, product);
        }
        products[productData.name()]->configurations[config] = productData;
    }

    enabledConfigurations << config;
}

MsvsProjectConfiguration::MsvsProjectConfiguration()
{
}

MsvsProjectConfiguration::MsvsProjectConfiguration(const Project &project,
                                                   const QString &qbsExecutablePath,
                                                   const QString &qbsProjectFile,
                                                   const QString &buildDirectory,
                                                   const QString &installRoot,
                                                   bool useSimplifiedConfigurationNames)
    : profile(project.profile())
    , qbsExecutablePath(qbsExecutablePath)
    , qbsProjectFile(qbsProjectFile)
    , buildDirectory(buildDirectory)
    , installRoot(installRoot)
    , useSimplifiedConfigurationNames(useSimplifiedConfigurationNames)
{
    const QVariantMap qbsSettings = project.projectConfiguration()[QStringLiteral("qbs")].toMap();
    variant = qbsSettings[QStringLiteral("buildVariant")].toString();

    const QStringList toolchain = qbsSettings[QStringLiteral("toolchain")].toStringList();
    if (toolchain != QStringList() << QStringLiteral("msvc"))
        qWarning() << "WARNING: Generating Visual Studio project for toolchain:" << toolchain.join(QLatin1Char(','));

    // Select VS platform display name. It doesn't interfere with compilation settings.
    const QString architecture = qbsSettings[QStringLiteral("architecture")].toString();
    platform = visualStudioArchitectureName(architecture);
    if (platform.isEmpty()) {
        qWarning() << "Naming qbs platform \"" << architecture << "\" as \"Win32\" for VS project.";
        platform = QStringLiteral("Win32");
    }

    // TODO: Get this from the *acutal* command line, not the resolved configuration
    const QVariantMap projectSettings = project.projectConfiguration().value(QStringLiteral("project")).toMap();
    foreach (const QString& key, projectSettings.keys())
        commandLineParameters += QStringLiteral("project.%1:%2").arg(key).arg(projectSettings[key].toString());
}

QString MsvsProjectConfiguration::cleanProfileName() const
{
    QString result = profile;
    return result.replace(QRegExp(QStringLiteral("\\W+")), QString());
}

QString MsvsProjectConfiguration::fullName() const
{
    return QStringLiteral("%1|%2").arg(profileAndVariant()).arg(platform);
}

QString MsvsProjectConfiguration::profileAndVariant() const
{
    if (useSimplifiedConfigurationNames) {
        QString variantName = variant;
        int len = QTextBoundaryFinder(QTextBoundaryFinder::Grapheme, variantName).toNextBoundary();
        variantName.replace(0, len, variantName.left(len).toUpper());
        return variantName;
    }

    return QStringLiteral("%1-%2").arg(cleanProfileName()).arg(variant);
}

bool MsvsProjectConfiguration::operator<(const MsvsProjectConfiguration &right) const
{
    return platform < right.platform
            || (platform == right.platform && variant < right.variant)
            || (platform == right.platform && variant == right.variant && profile < right.profile);
}

bool MsvsProjectConfiguration::operator==(const MsvsProjectConfiguration &right) const
{
    return profile == right.profile && variant == right.variant && platform == right.platform;
}

quint32 qbs::qHash(const MsvsProjectConfiguration &config)
{
    return qHash(QStringLiteral("%1-%2|%3").arg(config.profile).arg(config.variant).arg(config.platform));
}

QStringList MsvsPreparedProduct::uniquePlatforms() const
{
    QSet<QString> result;
    foreach (const MsvsProjectConfiguration &configuration, configurations.keys())
        result << configuration.platform;
    return result.toList();
}
