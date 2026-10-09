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

#pragma once

#ifdef HAVE_SWAP

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Whether a key is one a swap may be signed with.
 *
 * A swap is signed without any review on the device, and the signing key is
 * chosen by the host. So it is limited to what the swap flow itself uses: an
 * ed25519 (tz1) key on a Tezos account path, 44'/1729'/account' or
 * 44'/1729'/account'/change', every component hardened.
 *
 * @param is_ed25519: the derivation type is ed25519
 * @param components: path components
 * @param length: number of path components
 */
bool swap_signing_key_allowed(bool is_ed25519, const uint32_t *components,
                              size_t length);

#endif  // HAVE_SWAP
