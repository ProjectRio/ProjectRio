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
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
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
  std::istringstream stream(str);
  std::ostringstream result;
  std::string line;
  bool inList = false;
  int currentLevel = 1;

  auto closeList = [&]() {
    if (inList)
    {
      if (currentLevel == 2)
        result << "</ul>";
      result << "</ul>";
      inList = false;
      currentLevel = 1;
    }
  };

  while (std::getline(stream, line))
  {
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    // Detect indent before stripping for bullet handling
    size_t firstChar = line.find_first_not_of(" \t");

    // Headings
    if (firstChar == 0 && line.substr(0, 4) == "### ")
    {
      closeList();
      result << "<h3>" << line.substr(4) << "</h3>";
    }
    else if (firstChar == 0 && line.substr(0, 3) == "## ")
    {
      closeList();
      result << "<h2>" << line.substr(3) << "</h2>";
    }
    else if (firstChar == 0 && line.substr(0, 2) == "# ")
    {
      closeList();
      result << "<h1>" << line.substr(2) << "</h1>";
    }
    // Bullet points
    else if (firstChar != std::string::npos && (line[firstChar] == '-' || line[firstChar] == '*') &&
             line.size() > firstChar + 1 && line[firstChar + 1] == ' ')
    {
      int level = (firstChar == 0) ? 1 : 2;
      std::string content = line.substr(firstChar + 2);

      if (!inList)
      {
        result << "<ul>";
        inList = true;
        currentLevel = 1;
      }

      if (level > currentLevel)
      {
        result << "<ul>";
        currentLevel = level;
      }
      else if (level < currentLevel)
      {
        result << "</ul>";
        currentLevel = level;
      }

      result << "<li>" << content << "</li>";
    }
    // Empty line
    else if (line.empty() || firstChar == std::string::npos)
    {
      if (!inList)
        result << "<br/>";
    }
    // Normal line
    else
    {
      closeList();
      result << line << "<br/>";
    }
  }

  closeList();

  // Inline formatting
  std::string out = result.str();
  auto replaceInline = [](std::string& s, const std::string& mdOpen, const std::string& mdClose,
                          const std::string& htmlOpen, const std::string& htmlClose) {
    size_t pos = 0;
    while ((pos = s.find(mdOpen, pos)) != std::string::npos)
    {
      s.replace(pos, mdOpen.length(), htmlOpen);
      size_t end = s.find(mdClose, pos + htmlOpen.length());
      if (end == std::string::npos)
        break;
      s.replace(end, mdClose.length(), htmlClose);
      pos = end + htmlClose.length();
    }
  };

  replaceInline(out, "**", "**", "<strong>", "</strong>");
  replaceInline(out, "*", "*", "<em>", "</em>");
  replaceInline(out, "`", "`", "<code>", "</code>");

  return out;
}

void Updater::OnUpdateAvailable(const NewVersionInformation& info)
{
  std::string changes = MarkDownToRichText(info.changelog_html);
  std::optional<int> choice = RunOnObject(m_parent, [&] {
    QDialog* dialog = new QDialog(m_parent);
    dialog->setWindowTitle(tr("Update available"));
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* label =
        new QLabel(tr("<h2>A new version of Rio is available!</h2>"
                      "<h4>Head to the Project Rio website to download the latest update!</h4>"
                      "<u>New Version:</u><strong> %1</strong><br/>"
                      "<u>Your Version:</u><strong> %2</strong><br/>"
                      "<h3>Changelog</h3>")
                       .arg(QString::fromStdString(info.new_shortrev))
                       .arg(QString::fromStdString(Common::GetRioRevStr())));
    label->setTextFormat(Qt::RichText);

    auto* changelog = new QTextEdit;
    changelog->setHtml(QString::fromStdString(changes));
    changelog->setReadOnly(true);

    auto* buttons = new QDialogButtonBox;
    auto* projectrio =
        buttons->addButton(tr("Go to Project Rio website"), QDialogButtonBox::AcceptRole);
    connect(projectrio, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.projectrio.online/")));
    });

    auto* layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(changelog);
    layout->addWidget(buttons);
    dialog->setLayout(layout);
    SetQWidgetWindowDecorations(dialog);
    dialog->resize(500, 600);
    changelog->setMinimumHeight(350);
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
