#!/usr/bin/env python3
# Copyright 2023 Functori <contact@functori.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from utils.app import Screen

def test_basic(app):
    # Check main menu operation
    app.assert_screen(Screen.Home)
    app.backend.right_click()
    app.assert_screen(Screen.Version)
    app.backend.both_click()
    app.assert_screen(Screen.Version)
    app.backend.right_click()
    app.assert_screen(Screen.Settings)
    app.backend.right_click()
    app.assert_screen(Screen.Quit)
    app.backend.left_click()
    app.assert_screen(Screen.Settings)
    app.backend.left_click()
    app.assert_screen(Screen.Version)
    app.backend.left_click()
    app.assert_screen(Screen.Home)
    app.backend.left_click()
    app.assert_screen(Screen.Home)
    app.backend.both_click()
    app.assert_screen(Screen.Home)
    app.backend.right_click()
    app.assert_screen(Screen.Version)
    app.backend.right_click()
    app.assert_screen(Screen.Settings)
    app.backend.right_click()
    app.assert_screen(Screen.Quit)
    app.backend.right_click()
    app.assert_screen(Screen.Quit)

    # Check Settings menu operation
    app.backend.left_click()
    app.assert_screen(Screen.Settings)
    app.backend.both_click()
    app.assert_screen(Screen.Settings_expert_mode_disabled)
    app.backend.both_click()
    app.assert_screen(Screen.Settings_expert_mode_enabled)
    app.backend.right_click()
    app.assert_screen(Screen.Settings_back)
    app.backend.left_click()
    app.assert_screen(Screen.Settings_expert_mode_enabled)
    app.backend.both_click()
    app.assert_screen(Screen.Settings_expert_mode_disabled)
    app.backend.right_click()
    app.assert_screen(Screen.Settings_back)
    app.backend.both_click()
    app.assert_screen(Screen.Home)
    app.backend.right_click()
    app.backend.right_click()
    app.backend.right_click()
    app._quit()
