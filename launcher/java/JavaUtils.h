/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QProcess>
#include <QStringList>
#include "java/JavaInstall.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString stripVariableEntries(QString name, QString target, QString remove);
QProcessEnvironment CleanEnviroment();
QStringList getMinecraftJavaBundle();
QStringList getPrismJavaBundle();

class JavaUtils : public QObject {
    Q_OBJECT
   public:
    JavaUtils();

    JavaInstallPtr MakeJavaPtr(QString path, QString id = "unknown", QString arch = "unknown");
    QList<QString> FindJavaPaths();
    JavaInstallPtr GetDefaultJava();

#ifdef Q_OS_WIN
    QList<JavaInstallPtr> FindJavaFromRegistryKey(DWORD keyType, QString keyName, QString keyJavaDir, QString subkeySuffix = "");
#endif

    static QString getJavaCheckPath();
    static const QString javaExecutable;

    /**
     * True when the java binary at this path can actually start.
     *
     * A managed runtime that was only half downloaded still has bin/java.exe,
     * but its lib folder has no jvm.cfg. Starting such a binary makes the JVM
     * pop its own "could not open ...\lib\jvm.cfg" box, which looks like a
     * launcher crash, so such paths are rejected before anything is spawned.
     */
    static bool isRunnableJava(const QString& path);
};
