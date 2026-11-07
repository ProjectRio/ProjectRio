// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QElapsedTimer>

#include "Common/CommonTypes.h"

class QCheckBox;
class QLabel;
class QDialogButtonBox;
class QTimer;
class QVBoxLayout;

class GCPadWiiUConfigDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit GCPadWiiUConfigDialog(int port, QWidget* parent = nullptr);
  ~GCPadWiiUConfigDialog();

private:
  void LoadSettings();
  void SaveSettings();

  void UpdatePollRate();

  void CreateLayout();
  void ConnectWidgets();

private:
  void UpdateAdapterStatus();

  int m_port;

  QVBoxLayout* m_layout;
  QLabel* m_status_label;
  QDialogButtonBox* m_button_box;
  QLabel* m_poll_rate_label;
  QTimer* m_poll_rate_timer;
  QElapsedTimer m_poll_rate_elapsed;
  u64 m_last_read_count = 0;

  // Checkboxes
  QCheckBox* m_rumble;
  QCheckBox* m_simulate_bongos;
};
