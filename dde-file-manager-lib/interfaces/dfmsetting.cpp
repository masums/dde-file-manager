#include "dfmsetting.h"
#include <QJsonDocument>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTemporaryFile>
#include "dfmstandardpaths.h"
#include <qsettingbackend.h>
#include <settings.h>

DFMSetting::DFMSetting(QObject *parent) : QObject(parent)
{
    paths << "Current Path"
          << DFMStandardPaths::standardLocation(DFMStandardPaths::HomePath)
          << DFMStandardPaths::standardLocation(DFMStandardPaths::DesktopPath)
          << DFMStandardPaths::standardLocation(DFMStandardPaths::DocumentsPath)
          << DFMStandardPaths::standardLocation(DFMStandardPaths::PicturesPath)
          << DFMStandardPaths::standardLocation(DFMStandardPaths::VideosPath)
          << DFMStandardPaths::standardLocation(DFMStandardPaths::MusicPath)
          << DFMStandardPaths::standardLocation(DFMStandardPaths::DownloadsPath);

    //load temlate
    m_settings = Settings::fromJsonFile(":/configure/global-setting-template.json").data();

    //load conf value
    auto backen = new QSettingBackend(getConfigFilePath());
    m_settings->setBackend(backen);
}

QVariant DFMSetting::getValueByKey(const QString &key)
{
    return m_settings->value(key);
}

bool DFMSetting::isAllwayOpenOnNewWindow()
{
    return m_settings->value("base.open_action.allways_open_on_new_window").toBool();
}

int DFMSetting::iconSizeIndex()
{
    int index = m_settings->value("base.default_view.icon_size").toInt();
    if(index == 3)
        return index+1;
    return index;
}

int DFMSetting::openFileAction()
{
    const int& index = m_settings->value("base.open_action.open_file_action").toInt();
    return index;
}

QString DFMSetting::newWindowPath()
{
    const int& index = m_settings->value("base.new_tab_windows.new_window_path").toInt();
    if(index < paths.count() && index >= 0)
        return paths[index];
    return "Current Path";
}

QString DFMSetting::newTabPath()
{
    const int& index = m_settings->value("base.new_tab_windows.new_tab_path").toInt();
    if(index < paths.count() && index >= 0)
        return paths[index];
    return "Current Path";
}

QString DFMSetting::getConfigFilePath()
{
    return QString("%1/%2").arg(DFMStandardPaths::getConfigPath(), "dde-file-manager.conf");
}

void DFMSetting::setSettings(Settings *settings)
{
    m_settings = settings;
}

QPointer<Settings> DFMSetting::settings()
{
    return m_settings;
}

bool DFMSetting::isQuickSearch()
{
    return m_settings->value("advance.search.quick_search").toBool();
}

bool DFMSetting::isCompressFilePreview()
{
    return m_settings->value("advance.preview.compress_file_preview").toBool();
}

bool DFMSetting::isTextFilePreview()
{
    return m_settings->value("advance.preview.text_file_preview").toBool();
}

bool DFMSetting::isDocumentFilePreview()
{
    return m_settings->value("advance.preview.document_file_preview").toBool();
}

bool DFMSetting::isImageFilePreview()
{
    return m_settings->value("advance.preview.image_file_preview").toBool();
}

bool DFMSetting::isVideoFilePreview()
{
    return m_settings->value("advance.preview.video_file_preview").toBool();
}

bool DFMSetting::isAutoMount()
{
    return m_settings->value("advance.mount.auto_mount").toBool();
}

bool DFMSetting::isAutoMountAndOpen()
{
    return m_settings->value("advance.mount.auto_mount_and_open").toBool();
}

bool DFMSetting::isDefaultChooserDialog()
{
    return m_settings->value("advance.dialog.default_chooser_dialog").toBool();
}

bool DFMSetting::isShowedHiddenOnSearch()
{
    return m_settings->value("base.search.show_hidden").toBool();
}

bool DFMSetting::isShowedHiddenOnView()
{
    return m_settings->value("base.hidden_files.show_hidden").toBool();
}