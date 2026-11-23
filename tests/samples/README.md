# Parser samples

Reference samples for the C parsers of `app/src/parser`. They are generated
from Octez, committed, and replayed by the unit test
[`test_parser_samples`](../../unit-tests/test_parser_samples.c). Running that
test needs neither Octez nor OCaml: it is part of `make unit_tests` and of the
CI unit tests job.

| Directory         | One line of `samples.hex` is                       |
| ----------------- | -------------------------------------------------- |
| `micheline/nano/` | a Micheline expression (code or data)              |
| `operations/nano/`| an unsigned operation batch                        |

Each directory holds two files with one line per sample, in the same order:

- `samples.hex`: the binary encoding of the sample, in hex.
- `samples.expected`: the hex of the text the parser must render for it, as
  rendered by the Octez-based printers of `tests/generate/expected_*.ml`. The
  rendering is hex encoded to keep quotes and escapes out of the line format.

The expected rendering is Octez plus what the app adds on top of it. Anything
the app shows that Octez does not, like the note of a `finalize_unstake` paid
by another account, has to be mirrored in `expected_operations.ml`, or the test
fails on every sample that has it.

The test feeds each sample to the parser in pseudo-random chunks of input and
output, with a fixed seed, and compares what the parser wrote with the expected
rendering. Samples that the parser refuses by design, because they are too deep
or too large for a device, are counted and reported, not failed.

`test_*.sh` files are generated next to the samples for the legacy
`integration_tests_*` targets. They are not committed.

## Regenerating

Regenerate the samples only when the generator, the printers or the Octez
protocol they are built from change, or when the app starts to show something
new. The generator uses the default OCaml random state unless `RAND=1` is set,
so a regeneration without such a change does not modify `samples.hex`: if it
does, find out why before committing.

```
:; git submodule update --init --checkout deps/tezos
:; make docker_ledger_app_ocaml     # approx 1 hour, needs the Octez sources
:; make regen_samples
```

Review the diff of both files, then run `make unit_tests`: a changed rendering
is either a parser regression or an intended change of the expected output, and
only a human can tell which.
