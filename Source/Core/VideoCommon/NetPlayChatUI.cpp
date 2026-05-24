// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/NetPlayChatUI.h"

#include <algorithm>

#include <imgui.h>

#include "VideoCommon/Present.h"

constexpr float DEFAULT_WINDOW_WIDTH = 220.0f;
constexpr float DEFAULT_WINDOW_HEIGHT = 400.0f;

// Minimum dimensions the user can shrink the chat to. Kept small so the chat can
// fit inside a narrow pillar bar when aligned with the L/R shortcut buttons.
constexpr float MIN_WINDOW_WIDTH = 80.0f;
constexpr float MIN_WINDOW_HEIGHT = 100.0f;

// Gap between the chat box and the game image when snapped to a pillar.
constexpr float SNAP_FRAME_PADDING = 6.0f;

constexpr size_t MAX_BACKLOG_SIZE = 100;

std::unique_ptr<NetPlayChatUI> g_netplay_chat_ui;

NetPlayChatUI::NetPlayChatUI(std::function<void(const std::string&)> callback)
    : m_message_callback{std::move(callback)}
{
}

NetPlayChatUI::~NetPlayChatUI() = default;

void NetPlayChatUI::Display()
{
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;

  ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(MIN_WINDOW_WIDTH * scale, MIN_WINDOW_HEIGHT * scale), ImGui::GetIO().DisplaySize);
  ImGui::SetNextWindowSize(ImVec2(DEFAULT_WINDOW_WIDTH * scale, DEFAULT_WINDOW_HEIGHT * scale),
                           ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_None))
  {
    ImGui::End();
    return;
  }

  // L/R shortcuts: reposition + resize the chat to fill the left or right pillar's width.
  // Height is preserved so it doesn't take over the full screen. Acts once on click; the
  // window stays freely draggable + corner-resizable afterward. Returns true if the snap
  // was applied (i.e. the pillar was wide enough).
  const auto snap_to_pillar = [&](bool left) -> bool {
    if (!g_presenter)
      return false;
    const auto& target = g_presenter->GetTargetRectangle();
    const float display_x = ImGui::GetIO().DisplaySize.x;
    const float display_y = ImGui::GetIO().DisplaySize.y;
    const float current_height = ImGui::GetWindowSize().y;
    const float y_centered = std::max(0.0f, (display_y - current_height) * 0.5f);
    const float pad = SNAP_FRAME_PADDING * scale;
    if (left)
    {
      const float pillar_w = static_cast<float>(target.left) - pad;
      if (pillar_w < MIN_WINDOW_WIDTH * scale)
        return false;
      ImGui::SetWindowPos(ImVec2(0.0f, y_centered));
      ImGui::SetWindowSize(ImVec2(pillar_w, current_height));
    }
    else
    {
      const float pillar_w = display_x - static_cast<float>(target.right) - pad;
      if (pillar_w < MIN_WINDOW_WIDTH * scale)
        return false;
      ImGui::SetWindowPos(ImVec2(static_cast<float>(target.right) + pad, y_centered));
      ImGui::SetWindowSize(ImVec2(pillar_w, current_height));
    }
    return true;
  };

  // Fallback when the right pillar can't fit the chat: anchor to the right edge of the
  // screen, vertically centered, keeping the chat's current size.
  const auto align_right_edge_centered = [&] {
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const ImVec2 sz = ImGui::GetWindowSize();
    const float x = std::max(0.0f, ds.x - sz.x);
    const float y = std::max(0.0f, (ds.y - sz.y) * 0.5f);
    ImGui::SetWindowPos(ImVec2(x, y));
  };

  if (ImGui::ArrowButton("##snap_left", ImGuiDir_Left))
    snap_to_pillar(true);
  ImGui::SameLine();
  if (ImGui::ArrowButton("##snap_right", ImGuiDir_Right))
    snap_to_pillar(false);

  // Consume a hotkey-requested snap. Falls back to right-edge centered if the
  // requested pillar isn't wide enough — particularly relevant when the hotkey is
  // bound to the same key as Toggle Fullscreen and the pillar only just appeared.
  const PendingSnap pending = m_pending_snap.exchange(PendingSnap::None);
  if (pending == PendingSnap::Left)
  {
    if (!snap_to_pillar(true))
      align_right_edge_centered();
  }
  else if (pending == PendingSnap::Right)
  {
    if (!snap_to_pillar(false))
      align_right_edge_centered();
  }

  ImGui::Separator();

  ImGui::BeginChild("Scrolling", ImVec2(0, -30 * scale), true, ImGuiWindowFlags_None);
  for (const auto& msg : m_messages)
  {
    auto c = msg.second;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(c[0], c[1], c[2], 1.0f), "%s", msg.first.c_str());
    ImGui::PopTextWrapPos();
  }

  if (m_scroll_to_bottom)
  {
    ImGui::SetScrollHereY(1.0f);
    m_scroll_to_bottom = false;
  }

  m_is_scrolled_to_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();

  ImGui::EndChild();

  ImGui::Spacing();

  ImGui::PushItemWidth(-50.0f * scale);

  if (ImGui::InputText("##NetplayMessageBuffer", m_message_buf, IM_ARRAYSIZE(m_message_buf),
                       ImGuiInputTextFlags_EnterReturnsTrue))
  {
    SendMessage();
  }

  if (m_activate)
  {
    ImGui::SetKeyboardFocusHere(-1);
    m_activate = false;
  }

  ImGui::PopItemWidth();

  ImGui::SameLine();

  if (ImGui::Button("Send"))
    SendMessage();

  ImGui::End();
}

void NetPlayChatUI::AppendChat(std::string message, Color color)
{
  if (m_messages.size() > MAX_BACKLOG_SIZE)
    m_messages.pop_front();

  m_messages.emplace_back(std::move(message), color);

  // Only scroll to bottom, if we were at the bottom previously
  if (m_is_scrolled_to_bottom)
    m_scroll_to_bottom = true;
}

void NetPlayChatUI::SendMessage()
{
  // Check whether the input field is empty
  if (m_message_buf[0] != '\0')
  {
    if (m_message_callback)
      m_message_callback(m_message_buf);

    // 'Empty' the buffer
    m_message_buf[0] = '\0';
  }
}

void NetPlayChatUI::Activate()
{
  if (ImGui::IsItemFocused())
    ImGui::SetWindowFocus(nullptr);
  else
    m_activate = true;
}
