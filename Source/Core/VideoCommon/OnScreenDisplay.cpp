// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/OnScreenDisplay.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Timer.h"

#include "Core/Config/MainSettings.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/TextureConfig.h"

namespace OSD
{
constexpr float LEFT_MARGIN = 10.0f;         // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;          // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;       // Pixels between subsequent OSD messages.
constexpr float SNAP_PADDING = 12.0f;        // Pixels between OSD box edge and inside game frame.
constexpr float MESSAGE_FADE_TIME = 1000.f;  // Ms to fade OSD messages at the end of their life.
constexpr float MESSAGE_DROP_TIME = 5000.f;  // Ms to drop OSD messages that has yet to ever render.

// Groups of message types that share a single expand toggle. Clicking the grip on
// any box in a group toggles every box in that group together. Use ExpandGroup::None
// to mean "not expandable" (no grip drawn, no toggle).
enum class ExpandGroup
{
  None,
  TrainingMode,  // batting + fielder/ball coords + golfing all toggle as one
  PlayerBatter,
  PlayerFielder,
};

static ExpandGroup GroupForType(MessageType type)
{
  switch (type)
  {
  case MessageType::TrainingModeBatting:
  case MessageType::TrainingModeFielderCoordinates:
  case MessageType::TrainingModeBallCoordinates:
  case MessageType::TrainingModeGolfing:
    return ExpandGroup::TrainingMode;
  case MessageType::CurrentBatter:
    return ExpandGroup::PlayerBatter;
  case MessageType::CurrentFielder:
    return ExpandGroup::PlayerFielder;
  default:
    return ExpandGroup::None;
  }
}

// Stable per-type window name. ImGui needs a stable name to carry window state —
// in our case the per-box expand toggle — across the replacement messages that
// AddTypedMessage produces every frame. Non-expandable types return nullptr and
// fall back to a per-index name.
static const char* WindowNameForType(MessageType type)
{
  switch (type)
  {
  case MessageType::TrainingModeBatting:            return "osd_training_batting";
  case MessageType::TrainingModeFielderCoordinates: return "osd_training_fielder_coords";
  case MessageType::TrainingModeBallCoordinates:    return "osd_training_ball_coords";
  case MessageType::TrainingModeGolfing:            return "osd_training_golfing";
  case MessageType::CurrentBatter:                  return "osd_player_batter";
  case MessageType::CurrentFielder:                 return "osd_player_fielder";
  default:                                          return nullptr;
  }
}

// Per-group "collapsed" state — when true, the box shrinks to fit the left pillar;
// when false (default), the box renders at full font size. Persists across launches
// via Rio's Config system.
static const Config::Info<bool>* ConfigForGroup(ExpandGroup group)
{
  switch (group)
  {
  case ExpandGroup::TrainingMode:  return &Config::MAIN_OSD_TRAINING_COLLAPSED;
  case ExpandGroup::PlayerBatter:  return &Config::MAIN_OSD_PLAYER_BATTER_COLLAPSED;
  case ExpandGroup::PlayerFielder: return &Config::MAIN_OSD_PLAYER_FIELDER_COLLAPSED;
  default:                         return nullptr;
  }
}

static std::atomic<int> s_obscured_pixels_left = 0;
static std::atomic<int> s_obscured_pixels_top = 0;

struct Message
{
  Message() = default;
  Message(std::string text_, u32 duration_, u32 color_, std::unique_ptr<Icon> icon_ = nullptr)
      : text(std::move(text_)), duration(duration_), color(color_), icon(std::move(icon_))
  {
    timer.Start();
  }
  s64 TimeRemaining() const { return duration - timer.ElapsedMs(); }
  std::string text;
  Common::Timer timer;
  u32 duration = 0;
  bool ever_drawn = false;
  bool should_discard = false;
  u32 color = 0;
  std::unique_ptr<Icon> icon;
  std::unique_ptr<AbstractTexture> texture;
};
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

static ImVec4 ARGBToImVec4(const u32 argb)
{
  return ImVec4(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 0) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

// Smallest pixel font size we'll shrink to in order to fit a message inside a narrow
// black bar. Below this glyphs are unreadable so we let the text spill instead.
constexpr float MIN_FIT_PX = 10.0f;

// Parameters passed to DrawMessage. Bundled into a struct to keep the call site readable
// — there are a lot of inputs that vary per box.
struct DrawParams
{
  int index;
  MessageType type;
  const std::string* text;
  u32 color;
  u32 duration;
  s64 time_left;
  bool ever_drawn;
  // Icon carrier — for individual messages, points to the owning Message so we can lazily
  // create its GPU texture. nullptr for combined boxes (no icon).
  Message* icon_carrier;
  ImVec2 position;
  ImVec2 pivot;
  float max_content_width;  // 0 to disable shrink-to-fit
};

static float DrawMessage(const DrawParams& p)
{
  const ExpandGroup group = GroupForType(p.type);
  const bool expandable = group != ExpandGroup::None;
  // "Collapsed" means snap-to-pillar (shrink). Default is expanded/full-size.
  const Config::Info<bool>* config_info = expandable ? ConfigForGroup(group) : nullptr;
  const bool collapsed = config_info && Config::Get(*config_info);

  // Window name: stable per-type when we have one (preserves expand state across
  // replacement messages), per-index otherwise.
  std::string window_name_buf;
  const char* window_name = WindowNameForType(p.type);
  if (window_name == nullptr)
  {
    window_name_buf = fmt::format("osd_{}", p.index);
    window_name = window_name_buf.c_str();
  }

  // Shrink-to-fit only when the user has collapsed this group.
  bool font_pushed = false;
  if (collapsed && p.max_content_width > 0.0f)
  {
    const ImVec2 text_size = ImGui::CalcTextSize(p.text->c_str());
    if (text_size.x > p.max_content_width)
    {
      const float fit_scale = p.max_content_width / text_size.x;
      const float new_base = std::max(ImGui::GetStyle().FontSizeBase * fit_scale, MIN_FIT_PX);
      ImGui::PushFont(nullptr, new_base);
      font_pushed = true;
    }
  }

  // Pinned every frame at the top-left of the OSD area. Auto-resize handles size.
  ImGui::SetNextWindowPos(p.position, ImGuiCond_Always, p.pivot);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  // Gradually fade old messages away (except in their first frame).
  const float fade_time = std::max(std::min(MESSAGE_FADE_TIME, (float)p.duration), 1.f);
  const float alpha = std::clamp(static_cast<float>(p.time_left) / fade_time, 0.f, 1.f);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, p.ever_drawn ? alpha : 1.0f);

  // Expandable boxes accept mouse input on the toggle grip; non-expandable boxes have
  // NoInputs so clicks pass through to the game.
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
                           ImGuiWindowFlags_NoFocusOnAppearing;
  if (!expandable)
    flags |= ImGuiWindowFlags_NoInputs;

  float window_height = 0.0f;
  if (ImGui::Begin(window_name, nullptr, flags))
  {
    // Lazily create the icon's GPU texture (only for individual messages with an icon).
    Message* carrier = p.icon_carrier;
    if (carrier && carrier->icon)
    {
      if (!carrier->texture)
      {
        const u32 width = carrier->icon->width;
        const u32 height = carrier->icon->height;
        TextureConfig tex_config(width, height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                                 AbstractTextureType::Texture_2DArray);
        carrier->texture = g_gfx->CreateTexture(tex_config);
        if (carrier->texture)
        {
          carrier->texture->Load(0, width, height, width, carrier->icon->rgba_data.data(),
                                 sizeof(u32) * width * height);
        }
        else
        {
          // Don't try again next time.
          carrier->icon.reset();
        }
      }

      if (carrier->texture)
      {
        ImGui::Image(*carrier->texture.get(),
                     ImVec2(static_cast<float>(carrier->icon->width),
                            static_cast<float>(carrier->icon->height)));
      }
    }

    // Use %s in case the message contains %.
    ImGui::TextColored(ARGBToImVec4(p.color), "%s", p.text->c_str());

    // Capture the content size BEFORE we draw the grip — the grip is rendered with
    // ImDrawList (not a widget), so it must not influence auto-resize.
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);

    // Click-toggle "expand" grip in the bottom-right corner. Visually mimics ImGui's native
    // resize grip; toggles s_expanded[logical_type] between shrink-to-fit and full size.
    // Implemented with ImDrawList + manual hit-test so it does NOT add a widget to layout
    // (a widget here would chase its own tail with AlwaysAutoResize, growing the window).
    if (expandable)
    {
      const float font_size = ImGui::GetFontSize();
      const float grip_size = std::max(font_size * 0.65f, 10.0f);
      const ImVec2 win_pos = ImGui::GetWindowPos();
      const ImVec2 win_size = ImGui::GetWindowSize();
      const ImVec2 corner(win_pos.x + win_size.x, win_pos.y + win_size.y);
      const ImVec2 grip_min(corner.x - grip_size, corner.y - grip_size);
      const ImVec2 grip_max(corner.x, corner.y);

      const bool hovered =
          ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(grip_min, grip_max);
      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && config_info)
        Config::SetBaseOrCurrent(*config_info, !collapsed);
      if (hovered)
      {
        ImGui::SetTooltip(collapsed ? "Click to expand to full size"
                                    : "Click to fit to pillar");
      }

      ImDrawList* dl = ImGui::GetWindowDrawList();
      const ImU32 col = ImGui::GetColorU32(hovered ? ImGuiCol_ResizeGripHovered
                                                   : ImGuiCol_ResizeGrip);
      // Triangle pointing into the corner — same shape ImGui uses for its resize grip.
      dl->AddTriangleFilled(ImVec2(corner.x - 1.0f, corner.y - 1.0f),
                            ImVec2(corner.x - grip_size, corner.y - 1.0f),
                            ImVec2(corner.x - 1.0f, corner.y - grip_size), col);
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  if (font_pushed)
    ImGui::PopFont();

  return window_height;
}

void AddTypedMessage(MessageType type, std::string message, u32 ms, u32 argb,
                     std::unique_ptr<Icon> icon)
{
  std::lock_guard lock{s_messages_mutex};

  // A message may hold a reference to a texture that can only be destroyed on the video thread, so
  // only mark the old typed message (if any) for removal. It will be discarded on the next call to
  // DrawMessages().
  auto range = s_messages.equal_range(type);
  for (auto it = range.first; it != range.second; ++it)
    it->second.should_discard = true;

  s_messages.emplace(type, Message(std::move(message), ms, argb, std::move(icon)));
}

void AddMessage(std::string message, u32 ms, u32 argb, std::unique_ptr<Icon> icon)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.emplace(MessageType::Typeless, Message(std::move(message), ms, argb, std::move(icon)));
}

void DrawMessages()
{
  const bool draw_messages = Config::Get(Config::MAIN_OSD_MESSAGES);
  const ImVec2 fb_scale = ImGui::GetIO().DisplayFramebufferScale;

  // Default Dolphin OSD positioning: top-left of the window, stacking downward.
  // We give each message a max text width derived from the left pillar (the black
  // bar between the window edge and the game image). DrawMessage uses this to
  // shrink-to-fit, clamped at MIN_FIT_PX so text never becomes unreadable.
  // - Wide pillar: text fits at full size, no shrink.
  // - Tight pillar: text shrinks to fit exactly.
  // - Narrower than MIN_FIT_PX allows: text shrinks to MIN_FIT_PX and spills
  //   leftward off-screen (better than not shrinking at all, since at least the
  //   right portion of each line is visible inside the pillar).
  // - No pillar (target.left == 0): max_content_width = 0, no shrink, box overlays
  //   the game (clicking the expand grip is a no-op since there's no fit to compute).
  const auto& target = g_presenter->GetTargetRectangle();
  const ImVec2 anchor(LEFT_MARGIN * fb_scale.x + s_obscured_pixels_left,
                      TOP_MARGIN * fb_scale.y + s_obscured_pixels_top);
  const ImVec2 pivot(0.0f, 0.0f);

  // Subtract ImGui's WindowPadding so we constrain the inner text area, not the outer box,
  // and account for the left-margin offset of the anchor.
  float max_content_width = 0.0f;
  if (target.left > 0)
  {
    const float max_window_width = static_cast<float>(target.left) -
                                   LEFT_MARGIN * fb_scale.x - SNAP_PADDING * fb_scale.x -
                                   static_cast<float>(s_obscured_pixels_left);
    max_content_width =
        std::max(0.0f, max_window_width - ImGui::GetStyle().WindowPadding.x * 2.0f);
  }

  ImVec2 cursor = anchor;
  int index = 0;

  std::lock_guard lock{s_messages_mutex};

  for (auto it = s_messages.begin(); it != s_messages.end();)
  {
    const MessageType msg_type = it->first;
    Message& msg = it->second;

    if (msg.should_discard)
    {
      it = s_messages.erase(it);
      continue;
    }

    const s64 time_left = msg.TimeRemaining();
    if (time_left <= 0 && (msg.ever_drawn || -time_left >= MESSAGE_DROP_TIME))
    {
      it = s_messages.erase(it);
      continue;
    }
    ++it;

    if (!draw_messages)
      continue;

    DrawParams p{};
    p.index = index++;
    p.type = msg_type;
    p.text = &msg.text;
    p.color = msg.color;
    p.duration = msg.duration;
    p.time_left = time_left;
    p.ever_drawn = msg.ever_drawn;
    p.icon_carrier = &msg;
    p.position = cursor;
    p.pivot = pivot;
    p.max_content_width = max_content_width;

    const float msg_height = DrawMessage(p);
    msg.ever_drawn = true;

    // Always stack downward — default Dolphin OSD layout.
    cursor.y += msg_height;
  }
}

void ClearMessages()
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.clear();
}

void SetObscuredPixelsLeft(int width)
{
  s_obscured_pixels_left = width;
}

void SetObscuredPixelsTop(int height)
{
  s_obscured_pixels_top = height;
}
}  // namespace OSD
