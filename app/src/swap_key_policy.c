/* Tezos Ledger application - Which keys a swap may be signed with

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#ifdef HAVE_SWAP

#include "swap_key_policy.h"

#define BIP32_HARDENED 0x80000000u
#define PATH_PURPOSE   (BIP32_HARDENED | 44u)
#define PATH_COIN_TYPE (BIP32_HARDENED | 1729u)

/* Exchange derives the refund address from 44'/1729'/account' and Ledger Live
   signs with 44'/1729'/account'/0'. Both are in use, so both are accepted. */
#define PATH_MIN_LENGTH 3u
#define PATH_MAX_LENGTH 4u

bool
swap_signing_key_allowed(bool is_ed25519, const uint32_t *components,
                         size_t length)
{
    size_t i;

    if (!is_ed25519 || (components == NULL)) {
        return false;
    }
    if ((length < PATH_MIN_LENGTH) || (length > PATH_MAX_LENGTH)) {
        return false;
    }
    if ((components[0] != PATH_PURPOSE)
        || (components[1] != PATH_COIN_TYPE)) {
        return false;
    }
    for (i = 2; i < length; i++) {
        if ((components[i] & BIP32_HARDENED) == 0) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_SWAP
