// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "InputCommon/GCAdapter.h"

GCPadWiiUConfigDialog::GCPadWiiUConfigDialog(int port, QWidget* parent)
    : QDialog(parent), m_port{port}
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateLayout();

  LoadSettings();
  ConnectWidgets();
}

GCPadWiiUConfigDialog::~GCPadWiiUConfigDialog()
{
  GCAdapter::SetAdapterCallback(nullptr);
}

void GCPadWiiUConfigDialog::CreateLayout()
{
  setWindowTitle(tr("GameCube Adapter for Wii U at Port %1").arg(m_port + 1));

  m_layout = new QVBoxLayout();
  m_status_label = new QLabel();
  m_poll_rate_label = new QLabel(tr("Poll Rate: N/A"));
  m_poll_rate_timer = new QTimer(this);
  m_rumble = new QCheckBox(tr("Enable Rumble"));
  m_simulate_bongos = new QCheckBox(tr("Simulate DK Bongos"));
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  UpdateAdapterStatus();

  m_last_read_count = GCAdapter::ReadCount();
  m_poll_rate_elapsed.start();
  m_poll_rate_timer->start(1000);

  auto callback = [this] { QueueOnObject(this, &GCPadWiiUConfigDialog::UpdateAdapterStatus); };
  GCAdapter::SetAdapterCallback(callback);

  m_layout->addWidget(m_status_label);
  m_layout->addWidget(m_poll_rate_label);
  m_layout->addWidget(m_rumble);
  m_layout->addWidget(m_simulate_bongos);
  m_layout->addWidget(m_button_box);

  setLayout(m_layout);
}

void GCPadWiiUConfigDialog::ConnectWidgets()
{
  connect(m_rumble, &QCheckBox::toggled, this, &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_simulate_bongos, &QCheckBox::toggled, this, &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_button_box, &QDialogButtonBox::accepted, this, &GCPadWiiUConfigDialog::accept);
  connect(m_poll_rate_timer, &QTimer::timeout, this, &GCPadWiiUConfigDialog::UpdatePollRate);
}

void GCPadWiiUConfigDialog::UpdateAdapterStatus()
{
  const char* error_message = nullptr;
  const bool detected = GCAdapter::IsDetected(&error_message);
  QString status_text;

  if (detected)
  {
    status_text = tr("Adapter Detected");
  }
  else if (error_message)
  {
    status_text = tr("Error Opening Adapter: %1").arg(QString::fromUtf8(error_message));
  }
  else
  {
    status_text = tr("No Adapter Detected");
  }

  m_status_label->setText(status_text);

  m_rumble->setEnabled(detected);
  m_simulate_bongos->setEnabled(detected);
  m_poll_rate_label->setEnabled(detected);
}

void GCPadWiiUConfigDialog::UpdatePollRate()
{
  const u64 read_count = GCAdapter::ReadCount();
  const u64 reads = read_count - m_last_read_count;
  const qint64 elapsed_ms = m_poll_rate_elapsed.restart();
  m_last_read_count = read_count;

  // no reads means there's no adapter, or it isn't in use
  if (reads == 0 || elapsed_ms <= 0)
  {
    m_poll_rate_label->setText(tr("Poll Rate: N/A"));
    return;
  }

  m_poll_rate_label->setText(tr("Poll Rate: %1 Hz").arg(qRound(reads * 1000.0 / elapsed_ms)));
}

void GCPadWiiUConfigDialog::LoadSettings()
{
  m_rumble->setChecked(Config::Get(Config::GetInfoForAdapterRumble(m_port)));
  m_simulate_bongos->setChecked(Config::Get(Config::GetInfoForSimulateKonga(m_port)));
}

void GCPadWiiUConfigDialog::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::GetInfoForAdapterRumble(m_port), m_rumble->isChecked());
  Config::SetBaseOrCurrent(Config::GetInfoForSimulateKonga(m_port), m_simulate_bongos->isChecked());
}
