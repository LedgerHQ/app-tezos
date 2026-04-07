# Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
# Copyright 2023 Trilitech <contact@trili.tech>
# Copyright 2023 Functori <contact@functori.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
export COMMIT=$(git describe --tags --abbrev=8 --always --long --dirty 2>/dev/null | sed 's/-dirty/*/')
export COMMIT_BYTES=$(printf '%s' "$COMMIT" | xxd -p -c 256)

export VERSION_WALLET_TAG="00"
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
APP_MAKEFILE="$SCRIPT_DIR/../../app/Makefile"

export APPVERSION_M=$(grep -m1 '^APPVERSION_M=' "$APP_MAKEFILE" | cut -d= -f2 | tr -d '[:space:]')
export APPVERSION_N=$(grep -m1 '^APPVERSION_N=' "$APP_MAKEFILE" | cut -d= -f2 | tr -d '[:space:]')
export APPVERSION_P=$(grep -m1 '^APPVERSION_P=' "$APP_MAKEFILE" | cut -d= -f2 | tr -d '[:space:]')
export APPVERSION=$APPVERSION_M.$APPVERSION_N.$APPVERSION_P
export VERSION_BYTES=$(printf "%02x%02x%02x%02x" "$VERSION_WALLET_TAG" "$APPVERSION_M" "$APPVERSION_N" "$APPVERSION_P")
