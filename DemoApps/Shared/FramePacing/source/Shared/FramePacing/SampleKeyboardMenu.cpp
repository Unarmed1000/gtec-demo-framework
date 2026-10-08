//****************************************************************************************************************************************************
//* BSD 3-Clause License
//*
//* Copyright (c) 2026, Mana Battery
//* All rights reserved.
//*
//* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
//*    documentation and/or other materials provided with the distribution.
//* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
//*    software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
//* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************************************************************************************

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/KeyEvent.hpp>
#include <FslGraphics/PackedColor32.hpp>
#include <FslGraphics/Sprite/BasicImageSprite.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <FslSimpleUI/Base/Control/ButtonBase.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/Control/ToggleButton.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <FslSimpleUI/Theme/Base/IThemeResources.hpp>
#include <Shared/FramePacing/SampleKeyboardMenu.hpp>
#include <Shared/FramePacing/SampleMenuRow.hpp>
#include <stdexcept>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! The bar of the cursor: white that the bar of the controls is seen through, so it reads on every control of the theme
      constexpr UI::UIColor CursorColor(PackedColor32(0x30FFFFFF));
    }
  }


  std::shared_ptr<UI::BaseWindow> SampleKeyboardMenu::AddButton(UI::Theme::IThemeControlFactory& rFactory,
                                                                const std::shared_ptr<UI::ButtonBase>& button, std::function<void()> action)
  {
    if (!button || !action)
    {
      throw std::invalid_argument("button and action can not be null");
    }
    Entry entry;
    entry.Type = EntryType::Button;
    entry.Row = CreateRow(rFactory, button);
    entry.Button = button;
    entry.ButtonAction = std::move(action);
    m_entries.push_back(std::move(entry));
    return m_entries.back().Row;
  }


  std::shared_ptr<UI::BaseWindow> SampleKeyboardMenu::AddToggle(UI::Theme::IThemeControlFactory& rFactory,
                                                                const std::shared_ptr<UI::ToggleButton>& toggleButton)
  {
    if (!toggleButton)
    {
      throw std::invalid_argument("toggleButton can not be null");
    }
    Entry entry;
    entry.Type = EntryType::Toggle;
    entry.Row = CreateRow(rFactory, toggleButton);
    entry.Toggle = toggleButton;
    entry.DefaultIsChecked = toggleButton->IsChecked();
    m_entries.push_back(std::move(entry));
    return m_entries.back().Row;
  }


  std::shared_ptr<UI::BaseWindow> SampleKeyboardMenu::AddSlider(UI::Theme::IThemeControlFactory& rFactory,
                                                                const std::shared_ptr<UI::BaseWindow>& caption,
                                                                const std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>>& slider)
  {
    if (!caption || !slider)
    {
      throw std::invalid_argument("caption and slider can not be null");
    }
    // The caption above the slider, as the two are stacked in a list
    const auto layout = std::make_shared<UI::StackLayout>(rFactory.GetContext());
    layout->SetOrientation(UI::LayoutOrientation::Vertical);
    layout->SetAlignmentX(UI::ItemAlignment::Stretch);
    layout->AddChild(caption);
    layout->AddChild(slider);

    Entry entry;
    entry.Type = EntryType::Slider;
    entry.Row = CreateRow(rFactory, layout);
    entry.Slider = slider;
    entry.DefaultValue = slider->GetValue();
    m_entries.push_back(std::move(entry));
    return m_entries.back().Row;
  }


  void SampleKeyboardMenu::SetScrollViewer(const std::shared_ptr<UI::ScrollViewer>& value)
  {
    m_scrollViewer = value;
  }


  void SampleKeyboardMenu::OnKeyEvent(const KeyEvent& event)
  {
    if (event.IsHandled() || m_entries.empty())
    {
      return;
    }
    const VirtualKey::Enum key = event.GetKey();
    if (!event.IsPressed())
    {
      // The release is left to whoever else looks at the key
      m_keyRepeat.Release(key);
      return;
    }

    switch (key)
    {
    case VirtualKey::UpArrow:
      event.Handled();
      MoveCursor(SampleMenuCursorMove::Previous);
      m_keyRepeat.Press(key, SampleMenuKeyRepeatMode::Steady);
      break;
    case VirtualKey::DownArrow:
      event.Handled();
      MoveCursor(SampleMenuCursorMove::Next);
      m_keyRepeat.Press(key, SampleMenuKeyRepeatMode::Steady);
      break;
    case VirtualKey::LeftArrow:
      if (m_isCursorShown)
      {
        event.Handled();
        ChangeEntry(-1);
        m_keyRepeat.Press(key, SampleMenuKeyRepeatMode::Accelerating);
      }
      break;
    case VirtualKey::RightArrow:
      if (m_isCursorShown)
      {
        event.Handled();
        ChangeEntry(1);
        m_keyRepeat.Press(key, SampleMenuKeyRepeatMode::Accelerating);
      }
      break;
    case VirtualKey::Return:
      if (m_isCursorShown)
      {
        event.Handled();
        UseEntry();
      }
      break;
    default:
      break;
    }
  }


  void SampleKeyboardMenu::Update(const TimeSpan elapsedTime)
  {
    const uint32_t repeats = m_keyRepeat.Update(elapsedTime);
    if (repeats == 0 || !m_isCursorShown)
    {
      return;
    }
    switch (m_keyRepeat.GetKey())
    {
    case VirtualKey::UpArrow:
      for (uint32_t i = 0; i < repeats; ++i)
      {
        MoveCursor(SampleMenuCursorMove::Previous);
      }
      break;
    case VirtualKey::DownArrow:
      for (uint32_t i = 0; i < repeats; ++i)
      {
        MoveCursor(SampleMenuCursorMove::Next);
      }
      break;
    case VirtualKey::LeftArrow:
      ChangeEntry(-static_cast<int32_t>(repeats));
      break;
    case VirtualKey::RightArrow:
      ChangeEntry(static_cast<int32_t>(repeats));
      break;
    default:
      break;
    }
  }


  void SampleKeyboardMenu::ResetToDefaults()
  {
    // Also the controls that are disabled at the moment: they can have been changed while they were enabled
    for (const Entry& entry : m_entries)
    {
      switch (entry.Type)
      {
      case EntryType::Button:
        break;
      case EntryType::Toggle:
        // A radio button that is not the one of its group is unchecked by the one that is
        entry.Toggle->SetIsChecked(entry.DefaultIsChecked);
        break;
      case EntryType::Slider:
        entry.Slider->SetValue(entry.DefaultValue);
        break;
      }
    }
    FSLLOG3_VERBOSE2("Keyboard menu: the controls are set to what they were added with");
  }


  void SampleKeyboardMenu::HideCursor()
  {
    m_keyRepeat.Clear();
    if (!m_isCursorShown)
    {
      return;
    }
    m_isCursorShown = false;
    if (m_cursorIndex < m_entries.size())
    {
      m_entries[m_cursorIndex].Row->SetHighlighted(false);
    }
    FSLLOG3_VERBOSE2("Keyboard menu: the cursor is hidden");
  }


  bool SampleKeyboardMenu::IsEnabled(const Entry& entry)
  {
    switch (entry.Type)
    {
    case EntryType::Button:
      return entry.Button->IsEnabled();
    case EntryType::Toggle:
      return entry.Toggle->IsEnabled();
    case EntryType::Slider:
      return entry.Slider->IsEnabled();
    }
    return false;
  }


  std::shared_ptr<SampleMenuRow> SampleKeyboardMenu::CreateRow(UI::Theme::IThemeControlFactory& rFactory,
                                                               const std::shared_ptr<UI::BaseWindow>& content)
  {
    auto row = std::make_shared<SampleMenuRow>(rFactory.GetContext());
    row->SetFillSprite(rFactory.GetResources().GetBasicFillSprite(false));
    row->SetHighlightColor(LocalConfig::CursorColor);
    // The bar of the cursor is as wide as the list
    row->SetAlignmentX(UI::ItemAlignment::Stretch);
    row->SetContent(content);
    return row;
  }


  void SampleKeyboardMenu::MoveCursor(const SampleMenuCursorMove move)
  {
    const auto isEnabled = [this](const std::size_t index) { return IsEnabled(m_entries[index]); };
    SetCursor(m_isCursorShown ? SampleMenuCursorUtil::Move(m_entries.size(), m_cursorIndex, move, isEnabled)
                              : SampleMenuCursorUtil::Nearest(m_entries.size(), m_cursorIndex, isEnabled));
  }


  void SampleKeyboardMenu::SetCursor(const std::size_t index)
  {
    if (index >= m_entries.size())
    {
      return;
    }
    if (index != m_cursorIndex || !m_isCursorShown)
    {
      if (m_cursorIndex < m_entries.size())
      {
        m_entries[m_cursorIndex].Row->SetHighlighted(false);
      }
      m_cursorIndex = index;
      m_isCursorShown = true;
      m_entries[index].Row->SetHighlighted(true);
      FSLLOG3_VERBOSE2("Keyboard menu: the cursor is at entry {} of {}", index + 1, m_entries.size());
    }
    if (m_scrollViewer)
    {
      // Also for a cursor that did not move: the list can have been scrolled away from it
      m_scrollViewer->MakeVisible(m_entries[index].Row);
    }
  }


  void SampleKeyboardMenu::UseEntry()
  {
    if (m_cursorIndex >= m_entries.size())
    {
      return;
    }
    const Entry& entry = m_entries[m_cursorIndex];
    // The controls check if they are enabled for a click only, so it is checked here
    if (!IsEnabled(entry))
    {
      return;
    }
    switch (entry.Type)
    {
    case EntryType::Button:
      entry.ButtonAction();
      break;
    case EntryType::Toggle:
      // A radio button that is checked stays checked
      entry.Toggle->Toggle();
      break;
    case EntryType::Slider:
      break;
    }
  }


  void SampleKeyboardMenu::ChangeEntry(const int32_t delta)
  {
    if (m_cursorIndex >= m_entries.size())
    {
      return;
    }
    const Entry& entry = m_entries[m_cursorIndex];
    if (delta == 0 || !IsEnabled(entry))
    {
      return;
    }
    switch (entry.Type)
    {
    case EntryType::Button:
      break;
    case EntryType::Toggle:
      // Left is off and right is on. A radio button can not be switched off, so left does nothing there.
      entry.Toggle->SetIsChecked(delta > 0);
      break;
    case EntryType::Slider:
      // Both stop at the end of the range of the slider
      if (delta > 0)
      {
        entry.Slider->AddValue(delta);
      }
      else
      {
        entry.Slider->SubValue(-delta);
      }
      break;
    }
  }
}
