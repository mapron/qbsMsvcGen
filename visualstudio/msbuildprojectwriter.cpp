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

#include "msbuildprojectwriter.h"
#include <tools/hostosinfo.h>
#include <QFile>
#include <QUuid>
#include <QXmlStreamWriter>

namespace qbs {

static const QString kMSBuildSchemaURI = QStringLiteral("http://schemas.microsoft.com/developer/msbuild/2003");

bool MSBuildProjectWriter::writeProjectFile(const MsvsPreparedProduct &product,
                                            const QString &baseBuildDirectory) const
{
    return VisualStudioXmlProjectWriter::writeProjectFile(product, baseBuildDirectory)
            && writeFiltersFile(product, baseBuildDirectory);
}

QString MSBuildProjectWriter::projectFileExtension() const
{
    return QStringLiteral(".vcxproj");
}

bool MSBuildProjectWriter::writeFiltersFile(const MsvsPreparedProduct &product,
                                            const QString &baseBuildDirectory) const
{
    QFile file(targetFilePath(product, baseBuildDirectory) + QStringLiteral(".filters"));
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);

    xmlWriter.writeStartDocument();
    xmlWriter.writeStartElement(QStringLiteral("Project"));
    xmlWriter.writeAttribute(QStringLiteral("ToolsVersion"), m_versionInfo.toolsVersion());
    xmlWriter.writeAttribute(QStringLiteral("xmlns"), kMSBuildSchemaURI);

    foreach (const VisualStudioItemGroupFilter &options, m_filterOptions) {
        xmlWriter.writeStartElement(QStringLiteral("ItemGroup"));
        xmlWriter.writeAttribute(QStringLiteral("Label"), QStringLiteral("ProjectConfigurations"));

            xmlWriter.writeStartElement(QStringLiteral("Filter"));
            xmlWriter.writeAttribute(QStringLiteral("Include"), options.title);

                xmlWriter.writeStartElement(QStringLiteral("UniqueIdentifier"));
                xmlWriter.writeCharacters(QUuid::createUuid().toString());
                xmlWriter.writeEndElement();

                xmlWriter.writeStartElement(QStringLiteral("Extensions"));
                xmlWriter.writeCharacters(options.extensions.toList().join(Internal::HostOsInfo::pathListSeparator(Internal::HostOsInfo::HostOsWindows)));
                xmlWriter.writeEndElement();

                if (!options.additionalOptions.isEmpty()) {
                    xmlWriter.writeStartElement(options.additionalOptions);
                    xmlWriter.writeCharacters(QStringLiteral("False")); // We write only "False" additional options. Could be changed later.
                    xmlWriter.writeEndElement();
                }

            xmlWriter.writeEndElement();

        xmlWriter.writeEndElement();
    }

    xmlWriter.writeStartElement(QStringLiteral("ItemGroup"));
    QSet<QString> allFiles;

    foreach (const MsvsProjectConfiguration &buildTask, product.configurations.keys())
        foreach (const GroupData &groupData, product.configurations[buildTask].groups())
            if (groupData.isEnabled())
                allFiles.unite(groupData.allFilePaths().toSet());

    foreach (const QString& fileName, allFiles) {
        xmlWriter.writeStartElement(QStringLiteral("ClCompile"));

            xmlWriter.writeAttribute(QStringLiteral("Include"), fileName);
            xmlWriter.writeStartElement(QStringLiteral("Filter"));
            // TODO: can we get file tags here from GroupData?
            foreach (const VisualStudioItemGroupFilter &options, m_filterOptions) // FIXME: non-optimal algorithm. Fix on bad performance.
                if (options.matchesFilter(fileName))
                    xmlWriter.writeCharacters(options.title);

            xmlWriter.writeEndElement();

        xmlWriter.writeEndElement();
    }

    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();

    return !xmlWriter.hasError() && file.flush();
}

void MSBuildProjectWriter::writeHeader(QXmlStreamWriter &xmlWriter,
                                       const MsvsPreparedProduct &product) const
{
    xmlWriter.writeStartDocument();

    xmlWriter.writeStartElement(QStringLiteral("Project"));
    xmlWriter.writeAttribute(QStringLiteral("DefaultTargets"), QStringLiteral("Build"));
    xmlWriter.writeAttribute(QStringLiteral("ToolsVersion"), m_versionInfo.toolsVersion());
    xmlWriter.writeAttribute(QStringLiteral("xmlns"), kMSBuildSchemaURI);

    // Project begin
    xmlWriter.writeStartElement(QStringLiteral("ItemGroup"));
    xmlWriter.writeAttribute(QStringLiteral("Label"), QStringLiteral("ProjectConfigurations"));
    foreach (const MsvsProjectConfiguration &buildTask, product.configurations.keys()) {
        xmlWriter.writeStartElement(QStringLiteral("ProjectConfiguration"));
        xmlWriter.writeAttribute(QStringLiteral("Include"), buildTask.fullName());
        xmlWriter.writeTextElement(QStringLiteral("Configuration"), buildTask.profileAndVariant());
        xmlWriter.writeTextElement(QStringLiteral("Platform"), buildTask.platform);
        xmlWriter.writeEndElement();
    }
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(QStringLiteral("PropertyGroup"));
    xmlWriter.writeAttribute(QStringLiteral("Label"), QStringLiteral("Globals"));
    xmlWriter.writeTextElement(QStringLiteral("ProjectGuid"), product.guid);
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(QStringLiteral("Import"));
    xmlWriter.writeAttribute(QStringLiteral("Project"),
                             QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.Default.props"));
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(QStringLiteral("Import"));
    xmlWriter.writeAttribute(QStringLiteral("Project"),
                             QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.props"));
    xmlWriter.writeEndElement();
}

void MSBuildProjectWriter::writeConfiguration(QXmlStreamWriter &xmlWriter,
                                                 const MsvsPreparedProduct &product,
                                                 const MsvsProjectConfiguration &buildTask,
                                                 const ProductData &productData) const
{
    const QString targetDir = product.targetPath;

    const PropertyMap properties = productData.moduleProperties();

    const bool debugBuild = properties.getModuleProperty(QStringLiteral("qbs"), QStringLiteral("debugInformation")).toBool();

    const QString buildTaskCondition = QStringLiteral("'$(Configuration)|$(Platform)'=='") + buildTask.fullName() + QStringLiteral("'");
    const QString optimizationLevel = properties.getModuleProperty(QStringLiteral("qbs"), QStringLiteral("optimization")).toString();
    const QString warningLevel = properties.getModuleProperty(QStringLiteral("qbs"), QStringLiteral("warningLevel")).toString();

    const QStringList includePaths = QStringList()
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("includePaths"))
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("systemIncludePaths"));
    const QStringList cppDefines = properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("defines"));

    const auto sep = Internal::HostOsInfo::pathListSeparator(Internal::HostOsInfo::HostOsWindows);

    // Setup VCTool compilation option if someone wants to change configuration type.
    xmlWriter.writeStartElement(QStringLiteral("PropertyGroup"));
    xmlWriter.writeAttribute(QStringLiteral("Condition"), buildTaskCondition);
    xmlWriter.writeAttribute(QStringLiteral("Label"), QStringLiteral("Configuration"));
    xmlWriter.writeTextElement(QStringLiteral("ConfigurationType"), QStringLiteral("Makefile"));
    xmlWriter.writeTextElement(QStringLiteral("UseDebugLibraries"), debugBuild ? QStringLiteral("true") : QStringLiteral("false"));
    xmlWriter.writeTextElement(QStringLiteral("CharacterSet"), // VS possible values: Unicode|MultiByte|NotSet
                               properties.getModuleProperty(QStringLiteral("cpp"), QStringLiteral("windowsApiCharacterSet")) == QStringLiteral("unicode") ? QStringLiteral("MultiByte") : QStringLiteral("NotSet"));
    xmlWriter.writeTextElement(QStringLiteral("PlatformToolset"), m_versionInfo.platformToolsetVersion());
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(QStringLiteral("PropertyGroup"));
    xmlWriter.writeAttribute(QStringLiteral("Condition"), buildTaskCondition);
    xmlWriter.writeAttribute(QStringLiteral("Label"), QStringLiteral("Configuration"));
    xmlWriter.writeTextElement(QStringLiteral("NMakeIncludeSearchPath"), includePaths.join(sep));
    xmlWriter.writeTextElement(QStringLiteral("NMakePreprocessorDefinitions"), cppDefines.join(sep));
    xmlWriter.writeTextElement(QStringLiteral("OutDir"), targetDir);
    xmlWriter.writeTextElement(QStringLiteral("TargetName"), productData.targetName());
    xmlWriter.writeTextElement(QStringLiteral("NMakeOutput"), QStringLiteral("$(OutDir)$(TargetName)$(TargetExt)"));
    xmlWriter.writeTextElement(QStringLiteral("LocalDebuggerCommand"), QStringLiteral("$(OutDir)$(TargetName)$(TargetExt)"));
    xmlWriter.writeTextElement(QStringLiteral("LocalDebuggerWorkingDirectory"), QStringLiteral("$(OutDir)"));
    xmlWriter.writeTextElement(QStringLiteral("DebuggerFlavor"), QStringLiteral("WindowsLocalDebugger"));
    xmlWriter.writeTextElement(QStringLiteral("NMakeBuildCommandLine"), qbsCommandLine(QStringLiteral("install"), product, buildTask));
    xmlWriter.writeTextElement(QStringLiteral("NMakeCleanCommandLine"), qbsCommandLine(QStringLiteral("clean"), product, buildTask));
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(QStringLiteral("ItemDefinitionGroup"));
    xmlWriter.writeAttribute(QStringLiteral("Condition"), buildTaskCondition);
        xmlWriter.writeStartElement(QStringLiteral("ClCompile"));
            xmlWriter.writeStartElement(QStringLiteral("WarningLevel"));
                if (warningLevel == QStringLiteral("none"))
                    xmlWriter.writeCharacters(QStringLiteral("TurnOffAllWarnings"));
                else if (warningLevel == QStringLiteral("all"))
                    xmlWriter.writeCharacters(QStringLiteral("EnableAllWarnings"));
                else
                    xmlWriter.writeCharacters(QStringLiteral("Level3")); // this is VS default.
            xmlWriter.writeEndElement();

            xmlWriter.writeTextElement(QStringLiteral("Optimization"), optimizationLevel == QStringLiteral("none") ? QStringLiteral("Disabled") : QStringLiteral("MaxSpeed"));
            xmlWriter.writeTextElement(QStringLiteral("RuntimeLibrary"),
                                       debugBuild ? QStringLiteral("MultiThreadedDebugDLL") : QStringLiteral("MultiThreadedDLL"));
            xmlWriter.writeTextElement(QStringLiteral("PreprocessorDefinitions"),
                                       cppDefines.join(sep) + sep + QStringLiteral("%(PreprocessorDefinitions)"));
            xmlWriter.writeTextElement(QStringLiteral("AdditionalIncludeDirectories"),
                                       includePaths.join(sep) + sep + QStringLiteral("%(AdditionalIncludeDirectories)"));
        xmlWriter.writeEndElement();

        xmlWriter.writeStartElement(QStringLiteral("Link"));
            xmlWriter.writeTextElement(QStringLiteral("GenerateDebugInformation"), debugBuild ? QStringLiteral("true") : QStringLiteral("false"));
            xmlWriter.writeTextElement(QStringLiteral("OptimizeReferences"), debugBuild ? QStringLiteral("false") : QStringLiteral("true"));
            xmlWriter.writeTextElement(QStringLiteral("AdditionalDependencies"),
                                       properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("staticLibraries")).join(sep) + sep + QStringLiteral("%(AdditionalDependencies)"));
            xmlWriter.writeTextElement(QStringLiteral("AdditionalLibraryDirectories"),
                                       properties.getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("libraryPaths")).join(sep));
        xmlWriter.writeEndElement();
        xmlWriter.writeEndElement();
}

void MSBuildProjectWriter::writeFiles(QXmlStreamWriter &xmlWriter,
                                             const QSet<MsvsProjectConfiguration> &allConfigurations,
                                             const ProjectConfigurations &allProjectFilesConfigurations) const
{
    xmlWriter.writeStartElement(QStringLiteral("ItemGroup"));

    foreach (const QString &filePath, allProjectFilesConfigurations.keys()) {
        xmlWriter.writeStartElement(QStringLiteral("ClCompile"));
        QSet<MsvsProjectConfiguration> disabledConfigurations = allConfigurations - allProjectFilesConfigurations[filePath];
        xmlWriter.writeAttribute(QStringLiteral("Include"), filePath);
        foreach (const MsvsProjectConfiguration &buildTask, disabledConfigurations) {
            xmlWriter.writeStartElement(QStringLiteral("ExcludedFromBuild"));
            xmlWriter.writeAttribute(QStringLiteral("Condition"), QStringLiteral("'$(Configuration)|$(Platform)'=='") + buildTask.fullName() + QStringLiteral("'"));
            xmlWriter.writeCharacters(QStringLiteral("true"));
            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(QStringLiteral("Import"));
    xmlWriter.writeAttribute(QStringLiteral("Project"), QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.targets"));
    xmlWriter.writeEndElement();
}

void MSBuildProjectWriter::writeFooter(QXmlStreamWriter &xmlWriter) const
{
    xmlWriter.writeEndElement(); // </Project>
    xmlWriter.writeEndDocument();
}

} // namespace qbs
