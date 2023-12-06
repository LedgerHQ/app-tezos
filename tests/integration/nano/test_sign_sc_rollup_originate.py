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

from pathlib import Path

from utils.app import nano_app, Screen, DEFAULT_ACCOUNT
from utils.message import Message

# Operation (0): SR: originate
# Fee: 0.01 XTZ
# Storage limit: 4
# Kind: arith
# Kernel: 396630396632393532643334353238633733336639343631356366633339626335353536313966633535306464346136376261323230386365386538363761613364313361366566393964666265333263363937346161396132313530643231656361323963333334396535396331336239303831663163313162343430616334643334353564656462653465653064653135613861663632306434633836323437643964313332646531626236646132336435666639643864666664613232626139613834
# Proof: 030002104135165622d08b0c6eac951c9d4fd65109585907bc30ef0617f6c26853c6ba724af04dd3e4b5861efae3166ebc12ef5781df9715c20943e8d0b7bc06068a6f8106737461747573c87a31b1c8e3af61756b336bcfc3b0c292c89b40cc8a5080ba99c45463d110ce8b
# Parameters: Pair "1" 2

if __name__ == "__main__":
    test_name = Path(__file__).stem
    with nano_app() as app:

        app.assert_screen(Screen.Home)

        message = Message.from_bytes("030000000000000000000000000000000000000000000000000000000000000000c800ffdd6102321bc251e4a5190ad5b12b251069d9b4904e02030400000000c63966303966323935326433343532386337333366393436313563666333396263353535363139666335353064643461363762613232303863653865383637616133643133613665663939646662653332633639373461613961323135306432316563613239633333343965353963313362393038316631633131623434306163346433343535646564626534656530646531356138616636323064346338363234376439643133326465316262366461323364356666396438646666646132326261396138340000006c030002104135165622d08b0c6eac951c9d4fd65109585907bc30ef0617f6c26853c6ba724af04dd3e4b5861efae3166ebc12ef5781df9715c20943e8d0b7bc06068a6f8106737461747573c87a31b1c8e3af61756b336bcfc3b0c292c89b40cc8a5080ba99c45463d110ce8b0000000a07070100000001310002")

        data = app.sign(DEFAULT_ACCOUNT,
                        message,
                        with_hash=True,
                        path=test_name)

        app.checker.check_signature(
            account=DEFAULT_ACCOUNT,
            message=message,
            with_hash=True,
            data=data)

        app.quit()
