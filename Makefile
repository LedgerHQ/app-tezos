#
# Makefile
#

all: app_nanos.tgz \
	app_nanosp.tgz \
	app_nanox.tgz \
	app_stax.tgz \
	app_flex.tgz \
	app_apex_p.tgz
debug: app_nanos_dbg.tgz \
	app_nanosp_dbg.tgz \
	app_nanox_dbg.tgz \
	app_stax_dbg.tgz \
	app_flex_dbg.tgz \
	app_apex_p_dbg.tgz

.PHONY: clean all debug format integration_tests unit_tests scan-build%	\
	integration_tests_basic integration_tests_basic_% docker_%

DOCKER			= docker
DOCKER_RUN		= $(DOCKER) run --rm -i -v "$(realpath .):/app"
DOCKER_RUN_APP_BUILDER	= $(DOCKER_RUN) ledger-app-builder:latest
DOCKER_RUN_APP_OCAML	= $(DOCKER_RUN) ledger-app-tezos-ocaml:latest
CONCURRENCY = 2

ifdef GOLDEN_RUN
PYTEST_FLAGS += --golden-run
endif

ifdef FILTER_TESTS
PYTEST_FLAGS +=  -k '$(FILTER_TESTS)'
endif

#
# Fetch the docker images:

LEDGERHQ=ghcr.io/ledgerhq

CPU = $(shell uname -m)

docker_speculos:
	$(DOCKER) pull $(LEDGERHQ)/speculos
	$(DOCKER) image tag $(LEDGERHQ)/speculos speculos

docker_ledger_app_builder:
	$(DOCKER) pull $(LEDGERHQ)/ledger-app-builder/ledger-app-dev-tools:latest
	$(DOCKER) image tag $(LEDGERHQ)/ledger-app-builder/ledger-app-dev-tools:latest \
			ledger-app-builder

docker_ledger_app_ocaml:
	$(DOCKER) build -t ledger-app-tezos-ocaml \
			-f docker/Dockerfile.ocaml . --platform linux/$(CPU)

docker_ledger_app_integration_tests:
	$(DOCKER) pull $(LEDGERHQ)/ledger-app-builder/ledger-app-dev-tools:latest
	$(DOCKER) image tag $(LEDGERHQ)/ledger-app-builder/ledger-app-dev-tools:latest \
			ledger-app-tezos-integration-tests

docker_images: docker_speculos		\
	docker_ledger_app_builder	\
	docker_ledger_app_ocaml		\
	docker_ledger_app_integration_tests

scan-build-%:
	SDK=$(shell echo $* | tr "[:lower:]" "[:upper:]")_SDK;	\
	$(DOCKER_RUN_APP_BUILDER) bash -c			\
	  "BOLOS_SDK=\$$$$SDK make -C app scan-build"

scan-build:	scan-build-nanos scan-build-nanosp	\
	scan-build-nanox scan-build-stax scan-build-flex \
	scan-build-apex-p

app_%_dbg.tgz:	app/src/*.[ch]	\
	app/src/parser/*.[ch]	\
	app/Makefile
	SDK=$(shell echo $* | tr "[:lower:]" "[:upper:]")_SDK;	\
	$(DOCKER_RUN_APP_BUILDER) bash -c						\
		"BOLOS_SDK=\$$$$SDK make -C app DEBUG=1"
	$(DOCKER_RUN_APP_BUILDER) bash -c "cd app/bin/ && tar cz ." > $@


app_%.tgz:	app/src/*.[ch]		\
		app/src/parser/*.[ch]	\
		app/Makefile
	SDK=$(shell echo $* | tr "[:lower:]" "[:upper:]")_SDK;	\
	$(DOCKER_RUN_APP_BUILDER) bash -c						\
		"BOLOS_SDK=\$$$$SDK make -C app"
	$(DOCKER_RUN_APP_BUILDER) bash -c "cd app/bin/ && tar cz ." > $@

clean:
	rm -rf bin app_*.tgz
	make -C tests/unit/ctest clean
	$(DOCKER_RUN_APP_BUILDER) make -C app mrproper
	$(DOCKER_RUN_APP_OCAML) bash -c "make -C /app/tests/generate clean && cd /app && rm -rf **/_build"

unit_tests:	test/samples/micheline/nano/samples.hex	\
		test/samples/operations/nano/samples.hex	\
		tests/unit/parser/*.ml				\
		tests/unit/parser/*.[ch]			\
		tests/unit/parser/dune				\
		tests/unit/parser/Makefile			\
		tests/unit/Makefile				\
		tests/unit/ctest/Makefile			\
		tests/unit/ctest/*.[ch]
	@cp app/src/parser/[!g]*.[ch] tests/unit/parser/

	$(DOCKER_RUN_APP_OCAML) make -C /app/tests/unit

RUN_TEST_DOCKER = ./tests/integration/run_test_docker.sh

integration_tests_basic_%:	app_%.tgz   \
				app_%_dbg.tgz			\
				$(shell find tests/integration/python -type f)
	docker run --rm -i -v "$(realpath .):/app" \
	--entrypoint=/bin/bash ledger-app-tezos-integration-tests -c "  \
		TMP_DIR=\$$(mktemp -d /tmp/foo-XXXXXX);                   \
		cd /app;                                                  \
		tar xfz app_$*_dbg.tgz -C \$$TMP_DIR;                     \
		apt update && apt install -y libsodium-dev;     \
		python3 -m venv tezos_test_env --system-site-package;     \
		source ./tezos_test_env/bin/activate;                     \
		python3 -m pip install --upgrade pip -q;                  \
		python3 -m pip install -r tests/requirements.txt -q ;     \
		python3 -m pytest -n $(CONCURRENCY) tests/integration/python/ --tb=short \
			--device $* --app \$$TMP_DIR/app.elf              \
			--log-dir integration_tests_log $(PYTEST_FLAGS)"

integration_tests_basic:	integration_tests_basic_nanos	\
				integration_tests_basic_nanosp	\
				integration_tests_basic_nanox	\
				integration_tests_basic_stax	\
				integration_tests_basic_flex	\
				integration_tests_basic_apex_p

integration_tests_%:	integration_tests_basic_%		\
			test/samples/operations/nano/samples.hex\
			test/samples/micheline/nano/samples.hex	\
			tests/integration/*.sh
	$(RUN_TEST_DOCKER) $* 				\
			tests/samples/micheline/nano	\
			tests/samples/operations/nano

integration_tests: 	tests/integration/*.sh			\
			integration_tests_nanos 		\
			integration_tests_nanosp 		\
			integration_tests_nanox 		\
			integration_tests_basic_stax 	\
			integration_tests_basic_flex 	\
			integration_tests_basic_apex_p

test/samples/micheline/%/samples.hex:	tests/generate/*.ml*	\
					tests/generate/dune	\
					tests/generate/Makefile
	$(DOCKER_RUN_APP_OCAML) make -C /app/tests/generate	\
	    ../samples/micheline/$*/samples.hex

test/samples/operations/%/samples.hex:	tests/generate/*.ml*	\
					tests/generate/dune	\
					tests/generate/Makefile
	$(DOCKER_RUN_APP_OCAML) make -C /app/tests/generate	\
	    ../samples/operations/$*/samples.hex

load_%: app_%.tgz
	ledgerctl delete "Tezos Wallet"
	DIR=`mktemp -d` ; tar xf $< -C $$DIR && cd $$DIR && ledgerctl install app.toml ; rm -rf $$DIR

clang-format-linter-image:
	docker build -t clang-format-lint github.com/DoozyX/clang-format-lint-action

# command used by the pipeline to validate the code base format
format:
	@if ! docker image inspect clang-format-lint >/dev/null 2>&1; then \
		$(MAKE) clang-format-linter-image; \
	fi
	docker run --rm --user $(shell id -u):$(shell id -g) --workdir /app -v "$(CURDIR)":/app clang-format-lint "--clang-format-executable" "/clang-format/clang-format15" "-r" "--color" "always" "--style" "file" "--inplace" "true" "--extensions" "h,c" "--exclude" "none" "./app/src"

docker-images: docker_images
integration-tests: integration_tests
unit-tests: unit_tests
