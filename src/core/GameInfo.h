#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

struct GameInfo {
    QString name;
    QString nickname;
    QString appId;
    QString source = QStringLiteral("Steam");
    QString installPath;
    QString steamLibrary;
    QString protonPrefix;
    QString detectedProtonPrefix;
    bool prefixOverridden = false;
    QString exePath;
    QString detectedExePath;
    QString graphicsApi;
    QString detectedGraphicsApi;
    bool graphicsApiOverridden = false;
    QString architecture;
    QString engine;
    QString coverArtPath;
    QString bannerArtPath;
    QString originalCoverArtPath;
    QString originalBannerArtPath;
    bool artworkOverridden = false;
    bool customProgram = false;
    bool favorite = false;
    bool hidden = false;
    QString alternateGroupId;
    int duplicateCandidateCount = 0;
    int linkedAlternateCount = 0;
    bool exeOverridden = false;
    bool reshadeInstalled = false;
    bool reshadeManaged = false;
    bool reshadeExternal = false;
    QString reshadeVersion;
    QString reshadeChannel;
    QString reshadeProxy;
    bool reshade64Installed = false;
    bool reshade64Managed = false;
    bool reshade64External = false;
    QString reshade64Version;
    QString reshade64Channel;
    bool renodxInstalled = false;
    bool renodxManaged = false;
    bool renodxExternal = false;
    QString renodxFile;
    bool updateChecked = false;
    bool reshadeUpdateAvailable = false;
    bool renodxUpdateAvailable = false;
    bool reframeworkUpdateAvailable = false;
    bool reframeworkSupported = false;
    bool reframeworkInstalled = false;
    bool reframeworkManaged = false;
    bool reframeworkExternal = false;
    QString reframeworkVersion;
    bool optiScalerInstalled = false;
    bool integrityWarning = false;
    QString integritySummary;
    QStringList importDiagnostics;
    QVariantMap sourceMetadata;
    QStringList detectionHistory;
};
