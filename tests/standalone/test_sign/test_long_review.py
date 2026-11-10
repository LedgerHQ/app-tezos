# Copyright 2026 Ledger SAS

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Reviews that take many more pages than the application keeps.

The application keeps only the last pages of a review and gives up the oldest one when a page is
added, because its history is full or its text buffer is. What is given up must neither show again
in the pages that follow nor take a field with it.

The pages are read from the screen, so nothing is compared with a snapshot. With text this long
the buffer is full well before the history is, so these checks do not reach a history of eight
pages held at once: that case is covered by unit-tests/test_ui_stream_nbgl.c.
"""

import re

import pytest
from utils.account import Account
from utils.backend import TezosBackend
from utils.message import MichelineExpr, OperationGroup, Transaction
from utils.navigator import TezosNavigator

# Pages of a review the application keeps
HISTORY_PAGES = 8


@pytest.mark.use_on_device("touch")
def test_long_value(backend: TezosBackend, tezos_navigator: TezosNavigator, account: Account):
    """A value that takes many more pages than the application keeps is shown whole, once and in order.

    Every page holds one tall pair, with the same title: the old pages that are given up must not
    be added to the pages that follow.
    """
    # Four hexadecimal digits for each of 1000 numbers: a page that is missing, shown twice or cut
    # short changes the text
    value = "".join(f"{i:04x}" for i in range(1000))
    message = MichelineExpr({"string": value})

    with backend.sign(account, message, with_hash=False) as result:
        pages = tezos_navigator.read_review_pages()
        tezos_navigator.accept_sign(screen_change_before_first_instruction=False)

    account.check_signature(message=message, with_hash=False, data=result.value)

    # The first page opens the review and the last one is to sign
    assert len(pages) - 2 > HISTORY_PAGES, pages
    # Only the lines of the value are made of these characters
    shown = "".join(re.sub(r"\s", "", text) for page in pages for text in page if re.fullmatch(r'[0-9a-f"\s]+', text))
    assert shown == f'"{value}"', pages


@pytest.mark.use_on_device("touch")
def test_many_operations(backend: TezosBackend, tezos_navigator: TezosNavigator, account: Account):
    """Every field of every operation of a long batch is shown once.

    The pages of a batch hold as many pairs as they can, and the batch takes more pages than the
    application keeps.
    """
    # The last digit is not a 0: the amount is shown without the zeros at its end
    amounts = [11_000_001 + 10 * i for i in range(12)]
    message = OperationGroup(operations=[Transaction(amount=amount) for amount in amounts])

    with backend.sign(account, message, with_hash=True) as result:
        pages = tezos_navigator.read_review_pages()
        tezos_navigator.accept_sign(screen_change_before_first_instruction=False)

    account.check_signature(message=message, with_hash=True, data=result.value)

    assert len(pages) - 2 > HISTORY_PAGES, pages
    texts = [text.strip() for page in pages for text in page]
    labels = ("Source", "Fee", "Storage limit", "Amount", "Destination")
    titles = [text for text in texts if text in labels or text.startswith("Operation (")]
    # A value that does not fit a page goes on the next one under the same label
    fields = [title for i, title in enumerate(titles) if i == 0 or title != titles[i - 1]]
    for label in labels:
        assert fields.count(label) == len(amounts), (label, pages)
    for amount in amounts:
        shown = f"{amount // 10**6}.{amount % 10**6:06d} XTZ"
        assert texts.count(shown) == 1, (shown, pages)
