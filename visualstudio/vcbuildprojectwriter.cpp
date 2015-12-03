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

#include "vcbuildprojectwriter.h"
#include <tools/hostosinfo.h>
#include <QXmlStreamWriter>

namespace qbs {

static const QString _VcprojNmakeConfig = QStringLiteral("0");
static const QString _VcprojListsSeparator = QStringLiteral(";");

QString VCBuildProjectWriter::projectFileExtension() const
{
    return QStringLiteral(".vcproj");
}

void VCBuildProjectWriter::writeHeader(QXmlStreamWriter &xmlWriter, const MsvsPreparedProduct &product) const
{
    xmlWriter.writeStartDocument();

    xmlWriter.writeStartElement(QStringLiteral("VisualStudioProject"));
    xmlWriter.writeAttribute(QStringLiteral("ProjectType"), QStringLiteral("Visual C++"));
    xmlWriter.writeAttribute(QStringLiteral("Version"), m_versionInfo.toolsVersion());
    xmlWriter.writeAttribute(QStringLiteral("Name"), product.name);
    xmlWriter.writeAttribute(QStringLiteral("ProjectGUID"), product.guid);

    // Project begin
    xmlWriter.writeStartElement(QStringLiteral("Platforms"));
    foreach (const QString &platformName, product.uniquePlatforms()) {
        xmlWriter.writeStartElement(QStringLiteral("Platform"));
        xmlWriter.writeAttribute(QStringLiteral("Name"), platformName);
        xmlWriter.writeEndElement();
    }
    xmlWriter.writeEndElement();
}

void VCBuildProjectWriter::writeConfigurations(QXmlStreamWriter &xmlWriter,
                                                      const MsvsPreparedProduct &product,
                                                      const QSet<MsvsProjectConfiguration> &allConfigurations) const
{
    xmlWriter.writeStartElement(QStringLiteral("Configurations"));
    VisualStudioXmlProjectWriter::writeConfigurations(xmlWriter, product, allConfigurations);
    xmlWriter.writeEndElement();
}

void VCBuildProjectWriter::writeConfiguration(QXmlStreamWriter &xmlWriter,
                                                 const MsvsPreparedProduct &product,
                                                 const MsvsProjectConfiguration &buildTask,
                                                 const ProductData &productData) const
{
    const QString targetDir = product.targetPath;
    const PropertyMap properties = productData.moduleProperties();
    const QString executableSuffix = properties.getModuleProperty(QStringLiteral("qbs"), QStringLiteral("executableSuffix")).toString();
    const QString fullTargetName =  product.targetName + (product.isApplication ? executableSuffix : QString());

    const QStringList includePaths = QStringList()
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("includePaths"))
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("systemIncludePaths"));
    const QStringList cppDefines = properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("defines"));

    // For VCBuild we set only NMake options,
    // as it ignores VCCompiler options for configuration "Makefile".
    xmlWriter.writeStartElement(QStringLiteral("Configuration"));

    xmlWriter.writeAttribute(QStringLiteral("Name"), buildTask.fullName());
    xmlWriter.writeAttribute(QStringLiteral("OutputDirectory"), targetDir);
    xmlWriter.writeAttribute(QStringLiteral("ConfigurationType"), _VcprojNmakeConfig);

    xmlWriter.writeStartElement(QStringLiteral("Tool"));
    xmlWriter.writeAttribute(QStringLiteral("Name"), QStringLiteral("VCNMakeTool"));
    xmlWriter.writeAttribute(QStringLiteral("BuildCommandLine"), qbsCommandLine(QStringLiteral("install"), product, buildTask));
    xmlWriter.writeAttribute(QStringLiteral("ReBuildCommandLine"), qbsCommandLine(QStringLiteral("install"), product, buildTask));  // using build command.
    xmlWriter.writeAttribute(QStringLiteral("CleanCommandLine"), qbsCommandLine(QStringLiteral("clean"), product, buildTask));
    xmlWriter.writeAttribute(QStringLiteral("Output"), QStringLiteral("$(OutDir)%1").arg(fullTargetName));
    const auto sep = Internal::HostOsInfo::pathListSeparator(Internal::HostOsInfo::HostOsWindows);
    xmlWriter.writeAttribute(QStringLiteral("PreprocessorDefinitions"), cppDefines.join(sep));
    xmlWriter.writeAttribute(QStringLiteral("IncludeSearchPath"), includePaths.join(sep));
    xmlWriter.writeEndElement();

    xmlWriter.writeEndElement();
}

void VCBuildProjectWriter::writeFiles(QXmlStreamWriter &xmlWriter,
                                             const QSet<MsvsProjectConfiguration> &allConfigurations,
                                             const VisualStudioXmlProjectWriter::ProjectConfigurations &allProjectFilesConfigurations) const
{
    xmlWriter.writeStartElement(QStringLiteral("Files"));
    foreach (const VisualStudioItemGroupFilter &options, m_filterOptions) {
        QList<FilePathWithConfigurations> filterFilesWithDisabledConfigurations;
        foreach (const QString &filePath, allProjectFilesConfigurations.keys()) {
            if (options.matchesFilter(filePath)) {
                QStringList disabledFileConfigurations;
                QSet<MsvsProjectConfiguration> disabledConfigurations = allConfigurations - allProjectFilesConfigurations[filePath];
                foreach (const MsvsProjectConfiguration &buildTask, disabledConfigurations)
                    disabledFileConfigurations << buildTask.fullName();

                filterFilesWithDisabledConfigurations << FilePathWithConfigurations(filePath, disabledFileConfigurations);
            }
        }

        if (filterFilesWithDisabledConfigurations.isEmpty())
            continue;

        xmlWriter.writeStartElement(QStringLiteral("Filter"));
        xmlWriter.writeAttribute(QStringLiteral("Name"), options.title);

        foreach (const FilePathWithConfigurations &filePathAndConfig, filterFilesWithDisabledConfigurations) {
            xmlWriter.writeStartElement(QStringLiteral("File"));
            xmlWriter.writeAttribute(QStringLiteral("RelativePath"), filePathAndConfig.first); // No error! In VS absolute paths stored such way.

            foreach (const QString &disabledConfiguration, filePathAndConfig.second) {
                xmlWriter.writeStartElement(QStringLiteral("FileConfiguration"));
                xmlWriter.writeAttribute(QStringLiteral("Name"), disabledConfiguration);
                xmlWriter.writeAttribute(QStringLiteral("ExcludedFromBuild"), QStringLiteral("true"));
                xmlWriter.writeEndElement();
            }

            xmlWriter.writeEndElement();
        }

        xmlWriter.writeEndElement();
    }
}

void VCBuildProjectWriter::writeFooter(QXmlStreamWriter &xmlWriter) const
{
    xmlWriter.writeEndElement(); // </VisualStudioProject>
    xmlWriter.writeEndDocument();
}

} // namespace qbs
