// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Updater.h"

#include <cstdlib>
#include <utility>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Common/Version.h"

#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/Settings.h"
#include <qdesktopservices.h>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

Updater::Updater(QWidget* parent, std::string update_track, std::string hash_override)
    : m_parent(parent), m_update_track(std::move(update_track)),
      m_hash_override(std::move(hash_override))
{
  connect(this, &QThread::finished, this, &QObject::deleteLater);
}

void Updater::run()
{
  AutoUpdateChecker::CheckForUpdate(m_update_track, m_hash_override,
                                    AutoUpdateChecker::CheckType::Automatic);
}

void Updater::CheckForUpdate()
{
  AutoUpdateChecker::CheckForUpdate(m_update_track, m_hash_override,
                                    AutoUpdateChecker::CheckType::Manual);
}

std::string Updater::MarkDownToRichText(std::string str)
{
  std::string markdownNewLine = "\r\n";
  std::string RichTextNewLine = "<br/>";

  size_t start_pos = 0;
  while ((start_pos = str.find(markdownNewLine, start_pos)) != std::string::npos)
  {
    str.replace(start_pos, markdownNewLine.length(), RichTextNewLine);
    start_pos += RichTextNewLine.length();
  }

  start_pos = 0;
  while ((start_pos = str.find("*", start_pos)) != std::string::npos)
  {
    str.erase(start_pos, 1);
  }
  return str;
}

void Updater::OnUpdateAvailable(const NewVersionInformation& info)
{
  std::string changes = MarkDownToRichText(info.changelog_html);
  std::optional<int> choice = RunOnObject(m_parent, [&] {
    QDialog* dialog = new QDialog(m_parent);
    dialog->setWindowTitle(tr("Update available"));
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    auto* label = new QLabel(tr("<h2>A new version of Rio is available!</h2>"
                                "<h4>Head to the Project Rio website to download the latest update!</h4>"
                                "<u>New Version:</u><strong> %1</strong><br/>"
                                "<u>Your Version:</u><strong> %2</strong><br/>"
                                "<h3>Changelog</h3>%3")
                                .arg(QString::fromStdString(info.new_shortrev))
                                .arg(QString::fromStdString(Common::GetRioRevStr()))
                                .arg(QString::fromStdString(changes)));
    label->setTextFormat(Qt::RichText);

    auto* buttons = new QDialogButtonBox;
    auto* projectrio = buttons->addButton(tr("Go to Project Rio website"), QDialogButtonBox::AcceptRole);

    connect(projectrio, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.projectrio.online/")));
    });

    auto* layout = new QVBoxLayout;
    layout->addWidget(label);
    dialog->setLayout(layout);
    layout->addWidget(buttons);

    return dialog->exec();
  });

  /*
  {
    if (std::getenv("DOLPHIN_UPDATE_SERVER_URL"))
    {
      TriggerUpdate(info, AutoUpdateChecker::RestartMode::RESTART_AFTER_UPDATE);
      RunOnObject(m_parent, [this] {
        m_parent->close();
        return 0;
      });
      return;
    }

    bool later = false;

    std::optional<int> choice = RunOnObject(m_parent, [&] {
      QDialog* dialog = new QDialog(m_parent);
      dialog->setWindowTitle(tr("Update available"));
      dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);

      auto* label = new QLabel(
          tr("<h2>A new version of Dolphin is available!</h2>Dolphin %1 is available for "
            "download. "
            "You are running %2.<br> Would you like to update?<br><h4>Release Notes:</h4>")
              .arg(QString::fromStdString(info.new_shortrev))
              .arg(QString::fromStdString(Common::GetScmDescStr())));
      label->setTextFormat(Qt::RichText);

      auto* changelog = new QTextBrowser;

      changelog->setHtml(QString::fromStdString(info.changelog_html));
      changelog->setOpenExternalLinks(true);
      changelog->setMinimumWidth(400);

      auto* update_later_check = new QCheckBox(tr("Update after closing Dolphin"));

      connect(update_later_check, &QCheckBox::toggled, [&](bool checked) { later = checked; });

      auto* buttons = new QDialogButtonBox;

      auto* never_btn =
          buttons->addButton(tr("Never Auto-Update"), QDialogButtonBox::DestructiveRole);
      buttons->addButton(tr("Remind Me Later"), QDialogButtonBox::RejectRole);
      buttons->addButton(tr("Install Update"), QDialogButtonBox::AcceptRole);

      auto* layout = new QVBoxLayout;
      dialog->setLayout(layout);

      layout->addWidget(label);
      layout->addWidget(changelog);
      layout->addWidget(update_later_check);
      layout->addWidget(buttons);

      connect(never_btn, &QPushButton::clicked, [dialog] {
        Settings::Instance().SetAutoUpdateTrack(QString{});
        dialog->reject();
      });

      connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
      connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

      return dialog->exec();
    });

    if (choice && *choice == QDialog::Accepted)
    {
      TriggerUpdate(info, later ? AutoUpdateChecker::RestartMode::NO_RESTART_AFTER_UPDATE :
                                  AutoUpdateChecker::RestartMode::RESTART_AFTER_UPDATE);

      if (!later)
      {
        RunOnObject(m_parent, [this] {
          m_parent->close();
          return 0;
        });
      }
    }
  }*/
}
