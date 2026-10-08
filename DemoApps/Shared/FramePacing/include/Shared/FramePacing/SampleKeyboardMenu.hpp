#ifndef SHARED_FRAMEPACING_SAMPLEKEYBOARDMENU_HPP
#define SHARED_FRAMEPACING_SAMPLEKEYBOARDMENU_HPP
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

#include <FslBase/Time/TimeSpan.hpp>
#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <Shared/FramePacing/SampleMenuCursor.hpp>
#include <Shared/FramePacing/SampleMenuKeyRepeat.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Fsl
{
  class KeyEvent;
  class SampleMenuRow;
  namespace UI
  {
    class BaseWindow;
    class ButtonBase;
    class ScrollViewer;
    class ToggleButton;
    namespace Theme
    {
      class IThemeControlFactory;
    }
  }

  //! The keyboard menu of the side bar of the FramePacing samples: a cursor that is moved over the controls of a list with the arrow
  //! keys, so the sample can be used without a mouse.
  //! - Up and down move the cursor to the control before and after it, around the ends of the list. A control that is disabled is
  //!   passed over. A key that is held down repeats.
  //! - Return uses the control: it toggles a switch, checks a radio button and does what a button does.
  //! - Left and right change a slider by one, and faster the longer the key is held down. They switch a switch off and on, and right
  //!   checks a radio button.
  //! The cursor is a bar behind the control (SampleMenuRow). It is not shown before the up or the down arrow is pressed, which shows it
  //! where it was last, and the other keys do nothing while it is not shown.
  //!
  //! FslSimpleUI has no input focus and no key input, so the menu is a list of the controls in the order they were added in, and the
  //! owner sends it the key events.
  class SampleKeyboardMenu final
  {
    enum class EntryType
    {
      Button,
      Toggle,
      Slider
    };

    struct Entry
    {
      EntryType Type{EntryType::Button};
      std::shared_ptr<SampleMenuRow> Row;
      std::shared_ptr<UI::ButtonBase> Button;
      //! What the button does. The select event of a button is sent by a click only, so the menu calls what the owner does for it.
      std::function<void()> ButtonAction;
      //! A switch, a check box or a radio button
      std::shared_ptr<UI::ToggleButton> Toggle;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> Slider;
      //! What the control was set to when it was added: if a toggle was checked, the value of a slider
      bool DefaultIsChecked{false};
      int32_t DefaultValue{0};
    };

    std::vector<Entry> m_entries;
    //! The viewer the list can be scrolled in (can be null): it scrolls to the row the cursor moves to
    std::shared_ptr<UI::ScrollViewer> m_scrollViewer;
    SampleMenuKeyRepeat m_keyRepeat;
    //! The entry the cursor is at, or was at when it was shown last
    std::size_t m_cursorIndex{0};
    bool m_isCursorShown{false};

  public:
    //! @brief Add a button to the end of the menu
    //! @param action what Return does when the cursor is at the button (what the owner does for the select event of the button)
    //! @return the row of the button: the window to add to the list in place of the button
    std::shared_ptr<UI::BaseWindow> AddButton(UI::Theme::IThemeControlFactory& rFactory, const std::shared_ptr<UI::ButtonBase>& button,
                                              std::function<void()> action);

    //! @brief Add a switch, a check box or a radio button to the end of the menu
    //! @return the row of the control: the window to add to the list in place of the control
    std::shared_ptr<UI::BaseWindow> AddToggle(UI::Theme::IThemeControlFactory& rFactory, const std::shared_ptr<UI::ToggleButton>& toggleButton);

    //! @brief Add a slider to the end of the menu
    //! @param caption the label that says what the slider is. It is shown above the slider and is part of its row, so the cursor is
    //!        behind both.
    //! @return the row of the slider: the window to add to the list in place of the caption and the slider
    std::shared_ptr<UI::BaseWindow> AddSlider(UI::Theme::IThemeControlFactory& rFactory, const std::shared_ptr<UI::BaseWindow>& caption,
                                              const std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>>& slider);

    //! @brief The viewer the rows are scrolled in, so the row the cursor moves to is scrolled into its view
    void SetScrollViewer(const std::shared_ptr<UI::ScrollViewer>& value);

    //! @brief A key went down or came up. The event is marked as handled when the menu used it.
    void OnKeyEvent(const KeyEvent& event);

    //! @brief A frame went by: a key that is held down repeats
    //! @param elapsedTime the time since the last update
    void Update(const TimeSpan elapsedTime);

    //! @brief Set every switch, radio button and slider back to what it was set to when it was added (what the sample started with)
    void ResetToDefaults();

    //! @brief Hide the cursor (the mouse was used). The up or the down arrow shows it again, where it was.
    void HideCursor();

    [[nodiscard]] bool IsCursorShown() const noexcept
    {
      return m_isCursorShown;
    }

  private:
    [[nodiscard]] static bool IsEnabled(const Entry& entry);
    std::shared_ptr<SampleMenuRow> CreateRow(UI::Theme::IThemeControlFactory& rFactory, const std::shared_ptr<UI::BaseWindow>& content);
    //! Shows the cursor when it is not shown, else moves it
    void MoveCursor(const SampleMenuCursorMove move);
    void SetCursor(const std::size_t index);
    void UseEntry();
    //! Left (less than zero) and right (more than zero): a slider is changed by delta, a switch is switched off and on
    void ChangeEntry(const int32_t delta);
  };
}

#endif
