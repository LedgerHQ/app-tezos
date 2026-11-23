# ****************************************************************************
#    Ledger App Boilerplate
#    (c) 2023 Ledger SAS.
#    (c) 2023 Nomadic Labs <contact@nomadic.labs>
#    (c) 2023 Trilitech <contact@trili.tech>
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ****************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

include $(BOLOS_SDK)/Makefile.target

########################################
#        Mandatory configuration       #
########################################
# Application name
APPNAME = "Tezos Wallet"

# Application version
APPVERSION_M=3
APPVERSION_N=2
APPVERSION_P=6
APPVERSION=$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)

# COMMIT
GIT_DESCRIBE ?= $(shell git describe --tags --abbrev=8 --always --long --dirty 2>/dev/null)

VERSION_TAG ?= $(shell echo "$(GIT_DESCRIBE)" | cut -f1 -d-)

# Only warn about version tags if specified/inferred
ifeq ($(VERSION_TAG),)
  $(warning VERSION_TAG not checked)
else
  ifneq (v$(APPVERSION), $(VERSION_TAG))
    $(warning Version-Tag Mismatch: v$(APPVERSION) version and $(VERSION_TAG) tag)
  endif
endif

COMMIT ?= $(shell echo "$(GIT_DESCRIBE)" | awk -F'-g' '{print $2}' | sed 's/-dirty/*/')
ifeq ($(COMMIT),)
  $(warning COMMIT not specified and could not be determined with git from "$(GIT_DESCRIBE)")
else
  $(info COMMIT=$(COMMIT))
endif
DEFINES += COMMIT=\"$(COMMIT)\"

# Application source files
APP_SOURCE_PATH += src src/parser

# Application icons following guidelines:
# https://developers.ledger.com/docs/embedded-app/design-requirements/#device-icon
ICON_NANOS = icons/nano-s-tezos.gif
ICON_NANOX = icons/nano-x-tezos.gif
ICON_NANOSP = icons/nano-x-tezos.gif
ICON_STAX = icons/stax_tezos.gif
ICON_FLEX = icons/flex_tezos.gif
ICON_APEX_P = icons/apex_tezos.png

# Application allowed derivation curves.
# Possibles curves are: secp256k1, secp256r1, ed25519 and bls12381g1
# If your app needs it, you can specify multiple curves by using:
# `CURVE_APP_LOAD_PARAMS = <curve1> <curve2>`
CURVE_APP_LOAD_PARAMS = ed25519 secp256k1 secp256r1

# Application allowed derivation paths.
# You should request a specific path for your app.
# This serve as an isolation mechanism.
# Most application will have to request a path according to the BIP-0044
# and SLIP-0044 standards.
# If your app needs it, you can specify multiple path by using:
# `PATH_APP_LOAD_PARAMS = "44'/1'" "45'/1'"`
PATH_APP_LOAD_PARAMS = "44'/1729'"

# Setting to allow building variant applications
# - <VARIANT_PARAM> is the name of the parameter which should be set
#   to specify the variant that should be build.
# - <VARIANT_VALUES> a list of variant that can be build using this app code.
#   * It must at least contains one value.
#   * Values can be the app ticker or anything else but should be unique.
VARIANT_PARAM = APP_TYPE
VARIANT_VALUES = tezos_wallet

# Enabling DEBUG flag will enable PRINTF and disable optimizations
#DEBUG = 1
ifneq ($(DEBUG), 0)
  DEFINES += TEZOS_DEBUG
endif

########################################
#           Compiler warnings          #
########################################
# The optimization level and the baseline warnings come from the SDK
# (Makefile.defines): only add flags that are not already enabled there.
# Makefile.defines is included by Makefile.standard_app, so the SDK switches
# below must be set before it.
ENABLE_SDK_WERROR = 1
# The SDK sources are compiled with the app CFLAGS too, and are not clean for
# every warning, so the extra warnings are scoped to the app objects.
$(OBJ_DIR)/app/%.o: CFLAGS += -Wmissing-prototypes -Wpointer-arith
$(OBJ_DIR)/app/%.o: CFLAGS += -Wassign-enum -Wcomma -Wstring-conversion
$(OBJ_DIR)/app/%.o: CFLAGS += -Wcast-align -Wunreachable-code-aggressive
# -Wcast-qual is not enabled: it fires in the app code and in an SDK header.

########################################
#     Application custom permissions   #
########################################
# See SDK `include/appflags.h` for the purpose of each permission
#HAVE_APPLICATION_FLAG_DERIVE_MASTER = 1
#HAVE_APPLICATION_FLAG_GLOBAL_PIN = 1
#HAVE_APPLICATION_FLAG_BOLOS_SETTINGS = 1
# HAVE_APPLICATION_FLAG_LIBRARY is set by ENABLE_SWAP
#HAVE_APPLICATION_FLAG_LIBRARY = 1

########################################
# Application communication interfaces #
########################################
ENABLE_BLUETOOTH = 1
#ENABLE_NFC = 1
ENABLE_SWAP = 1
# U2F (used e.g. by Kukai wallet) is enabled by default by the SDK standard app,
# with its sources and USB_SEGMENT_SIZE. U2F_PROXY_MAGIC is not defined: the
# SDK no longer reads it (removed from lib_u2f in the IO revamp).

########################################
#         NBGL custom features         #
########################################
ENABLE_NBGL_QRCODE = 1
#ENABLE_NBGL_KEYBOARD = 1
#ENABLE_NBGL_KEYPAD = 1

########################################
#        Stack protection features     #
########################################
ENABLE_STACK_PROTECTOR = 1

########################################
#          Features disablers          #
########################################
# These advanced settings allow to disable some feature that are by
# default enabled in the SDK `Makefile.standard_app`.
#DISABLE_STANDARD_APP_FILES = 1
#DISABLE_DEFAULT_IO_SEPROXY_BUFFER_SIZE = 1 # To allow custom size declaration
#DISABLE_STANDARD_APP_DEFINES = 1 # Will set all the following disablers
#DISABLE_STANDARD_SNPRINTF = 1
#DISABLE_STANDARD_USB = 1
#DISABLE_STANDARD_WEBUSB = 1
#DISABLE_STANDARD_BAGL_UX_FLOW = 1
#DISABLE_DEBUG_LEDGER_ASSERT = 1
#DISABLE_DEBUG_THROW = 1

include $(BOLOS_SDK)/Makefile.standard_app

.PHONY: mrproper
mrproper: clean
	rm -f .target


.PHONY: app-size-report
app-size-report: $(BIN_TARGETS) $(DBG_TARGETS)
	@echo ""
	@echo "Finished Tezos Wallet app ($(TARGET_NAME)) → $(BIN_DIR)/app.elf"
	$(L)$(GCCPATH)arm-none-eabi-size $(BIN_DIR)/app.elf

default: app-size-report
