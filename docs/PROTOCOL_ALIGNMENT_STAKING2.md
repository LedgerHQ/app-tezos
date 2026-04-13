# Spike: Protocol alignment — Staking 2.0 (pseudo-operations)

**Status:** checklist for engineering follow-up  
**Sources:** [Paris staking mechanism](https://tezos.gitlab.io/paris/staking.html), [Seoul staking mechanism](https://tezos.gitlab.io/seoul/staking.html) (same pseudo-operation model; Seoul extends `finalize_unstake`).

---

## 1. Executive summary

Tezos **Staking 2.0** (adaptive issuance) exposes four **pseudo-operations**:

`stake`, `unstake`, `finalize_unstake`, `set_delegate_parameters`.

Per protocol documentation, these are **not new top-level manager operation tags**. They are implemented as **transfer (manager) operations** — in the typical case, **self-transfers** where `source` and `destination` are the same implicit account, with a **named entrypoint** matching the pseudo-operation. The binary encoding is therefore the usual **transaction** layout (manager header + amount + destination + entrypoint + parameters), not a new tag in `tz_operation_tag` alongside `108`/`110`/…

**Implication for this app:** alignment work is primarily **transaction + entrypoint + parameter decoding**, and **UX** (labeling and edge cases), not adding four new values to `TZ_OPERATION_TAG_*` unless the product explicitly wants distinct operation types in the UI state machine.

---

## 2. On-wire shape — what actually gets signed

This section answers: **if I inspect a staking-related operation as raw bytes, what pattern do I see?**  
It stays at **skeleton** level (no full Michelson grammar).

### 2.1 Same for all four pseudo-operations

Each pseudo-operation is a **manager operation** whose **kind** is **Transaction**. On the wire that is tag **`108`** — in this app, `TZ_OPERATION_TAG_TRANSACTION`. There is **no** extra protocol tag like “stake = 116”.

The **name** `stake` / `unstake` / … does **not** appear as its own top-level tag. It shows up **inside** the `108` payload as the **entrypoint** (and sometimes inside the **parameter**).

At a high level the stream looks like:

1. **Shell** (e.g. branch) — as for any operation.  
2. **Contents** — one or more operations; each has a **class tag** (`107` Reveal, **`108` Transaction**, `110` Delegation, …).  
3. For **`108`**, you get: **source**, **fee**, **counter**, **gas limit**, **storage limit**, then **amount**, **destination**, **entrypoint**, **parameter**.

Staking pseudo-ops are just **`108`** with specific **entrypoint** + **parameter** choices (see §3.2 for entrypoint byte **0–9** / **255**).

### 2.2 Typical wallet shape: self-transfer

In the **usual** flows (user sends the operation from their own wallet):

- **Source** and **destination** are the **same implicit account** (you transfer to yourself).
- The protocol uses the **entrypoint** to decide behavior (`stake`, `unstake`, …).

So the signer still sees **Source** and **Destination** fields; for self-transfer they **match**.

### 2.3 Per pseudo-operation — what to expect

**Stake** — move liquid tez into **staked** balance (you must already **delegate** to a baker).

| Field | Typical meaning |
|-------|------------------|
| Tag | **`108`** (Transaction) |
| Amount | **Greater than zero** (how much to stake) |
| Source / Destination | **Same** address |
| Entrypoint | **`stake`** (builtin index **6** — §3.2) |
| Parameter | **`Unit`** in normal flows |

**Unstake** — request to reduce staked balance (funds follow protocol delay before they are **finalizable**).

| Field | Typical meaning |
|-------|------------------|
| Tag | **`108`** |
| Amount | **Positive** amount, or “everything” depending on wallet encoding |
| Source / Destination | **Same** address |
| Entrypoint | **`unstake`** (index **7**) |
| Parameter | Wallet-specific for “all” vs fixed amount |

**Finalize unstake** — turn **finalizable** frozen tez back into **spendable** balance.

| Field | Typical meaning |
|-------|------------------|
| Tag | **`108`** |
| Amount | **Zero** |
| Source / Destination | **Same** in the **classic** user flow |
| Entrypoint | **`finalize_unstake`** (index **8**) |
| Parameter | **`Unit`** in typical flows |

**Set delegate parameters** — **baker** configures staking policy (limits and reward edge). This is still a **`108`** self-call, **not** the **`110` Delegation** operation.

| Field | Typical meaning |
|-------|------------------|
| Tag | **`108`** |
| Amount | **Zero** |
| Source / Destination | **Same** (delegate’s own account) |
| Entrypoint | **`set_delegate_parameters`** (index **9**) |
| Parameter | **Required** Micheline (e.g. **`Pair`** of ratio values — see Paris doc / `octez-client`); this is what **P2** focuses on for clear-signing |

### 2.4 Seoul+ exception: someone else pays for `finalize_unstake`

From **Seoul** onward, a **third party** may broadcast **`finalize_unstake`** for the staker (fee paid by bot or sponsor).

- **Source** = whoever pays the fee (often **≠** staker).  
- **Destination** = **staker’s** implicit account (the contract that receives the zero-tez transfer and runs **`finalize_unstake`**).

Treat **Source ≠ Destination** as **normal** for this entrypoint when reviewing bytes or UI.

---

## 3. Mapping to this codebase

### 3.1 Manager operation tag

- **Transaction:** `TZ_OPERATION_TAG_TRANSACTION` = `108` in `app/src/parser/operation_state.h`.
- **Delegation:** `TZ_OPERATION_TAG_DELEGATION` = `110` — still an optional delegate only in upstream `operation_repr` (no separate staking tag in the protocol’s `Delegation` constructor).

### 3.2 Entrypoint names (binary → display string)

In Octez, the **smart contract / “smart” entrypoint** encoding is `Entrypoint_repr.smart_encoding` in each protocol’s `lib_protocol/entrypoint_repr.ml` (a tagged union: `builtin_case n …` plus tag `255` for arbitrary names).

**Verification (done):** `tz_step_read_smart_entrypoint` in `app/src/parser/operation_parser.c` matches **`smart_encoding`** on **Octez `master`** for:

- `proto_020_PsParisC` (Paris)
- `proto_023_PtSeouLo` (Seoul)
- `proto_024_PtTALLiN` (Tallinn)
- `proto_alpha`

All four copies list the **same** builtin tags `0`–`9` and `255` (0xFF). No drift was found between those protocols.

| Byte (tag) | Entrypoint name | Ledger `case` |
|------------|-----------------|---------------|
| `0` | `default` | `0` |
| `1` | `root` | `1` |
| `2` | `do` | `2` |
| `3` | `set_delegate` | `3` |
| `4` | `remove_delegate` | `4` |
| `5` | `deposit` | `5` |
| `6` | `stake` | `6` |
| `7` | `unstake` | `7` |
| `8` | `finalize_unstake` | `8` |
| `9` | `set_delegate_parameters` | `9` |
| `255` (`0xFF`) | arbitrary name (length-prefixed string, max 31) | `0xFF` |

**Source of truth:** `builtin_case 0 …` through `builtin_case 9 …` and `Tag 255` in `entrypoint_repr.ml` (e.g. [Octez `entrypoint_repr.ml` on GitLab](https://gitlab.com/tezos/tezos/-/blob/master/src/proto_alpha/lib_protocol/entrypoint_repr.ml) — search for `let smart_encoding`).

**Ongoing:** when you **bump** the Octez/protocol version this app tracks, diff `entrypoint_repr.ml` again; if Tezos ever adds `builtin_case 10 …`, the Ledger parser must gain a matching `case` or reject until updated.

### 3.3 What is *not* required for “wire compatibility”

- New rows in `tz_operation_descriptors[]` **solely** for staking pseudo-ops — they use the existing **Transaction** descriptor path unless you add a **parallel** presentation layer.

### 3.4 Parsing change scheme (what actually moves)

Two different “numbers” must not be mixed:

| Concept | Where it lives | Role |
|--------|----------------|------|
| Protocol manager tag **`108`** | `tz_operation_tag` in `operation_state.h` | Tezos wire format: “this content is a **Transaction**.” |
| Parser control / errors **`TZ_CONTINUE`, `TZ_ERR_*`, …** | `tz_parser_result` in `parser_state.h` | **Internal** return codes from the embedded parser (`tz_return`, `tz_must`, …). **No relation to 108.** |

Staking pseudo-operations **stay on tag `108`**. Work is not “add 108 to `tz_parser_result`”; it is “keep using the **Transaction** descriptor, then improve **downstream** steps (entrypoint label, parameter display, signing UI).”

#### End-to-end pipeline (wire → app)

The diagram below merges the logical **Tezos encoding** (left) with **this repository’s** parser/signing layers (right). Read top to bottom.

```
  TEZOS WIRE (binary)                              LEDGER APP (C)
  ─────────────────────                            ─────────────────────────────────────────

  ┌──────────────────────────┐
  │ Operation shell          │
  │  • branch, …             │
  └────────────┬─────────────┘
               │
               ▼
  ┌──────────────────────────┐
  │ Contents                 │                    (parser: batch / cons, magic byte, …)
  │  single op or batch of     │
  │  manager operations        │
  └────────────┬─────────────┘
               │
               │          one item in the batch
               ▼
  ┌──────────────────────────┐
  │ Manager operation tag    │                    tz_operation_tag  (operation_state.h)
  │  (1 byte)                │                    e.g. 107 Reveal, 108 Transaction, 110 Delegation
  │                          │                    ─────────────────
  │  Staking pseudo-ops use  │                    Staking 2.0 pseudo-ops → still 108, not a new enum.
  │  ======= 108 ==========  │                         │
  └────────────┬─────────────┘                         │
               │ if tag == 108                         │
               ▼                                       ▼
  ┌──────────────────────────┐              ┌─────────────────────────────┐
  │ Manager header           │              │ Descriptor row for 108      │
  │  • source (implicit acct)│              │ tz_operation_descriptors[]  │
  │  • fee, counter          │   ─────────► │   → transaction_fields      │
  │  • gas_limit             │              │   (TZ_OPERATION_MANAGER_…   │
  │  • storage_limit         │              │    + Amount, Destination,   │
  └────────────┬─────────────┘              │    Entrypoint, Parameters)  │
               │                            └──────────────┬──────────────┘
               ▼                                           │
  ┌──────────────────────────┐                            │
  │ Transaction body         │                            │
  │  (same order as proto)   │                            ▼
  │                          │              ┌─────────────────────────────┐
  │  Amount                  │              │ Field walk: amount, dest, … │
  │  Destination             │   ◄────────┤ operation_parser.c          │
  │  Entrypoint              │              │                             │
  │    • tag 0–9 = builtin   │              │ Entrypoint byte → string:   │
  │      (see §3.2 table)    │   ─────────► │ tz_step_read_smart_entrypoint│
  │    • tag 255 = string    │              │   ("stake", "unstake", …)   │
  │  Parameters (Micheline)  │              │                             │
  │    • Unit or Pair …      │              │ Micheline / expr steps      │
  └────────────┬─────────────┘              │   (complexity, display)     │
               │                            └──────────────┬──────────────┘
               │                                           │
               │   Staking “novelty” is here:              ▼
               │   same 108 + same field order,     ┌─────────────────────────────┐
               │   but entrypoint 6–9 and params    │ Clear-sign / handler        │
               │   identify pseudo-op.              │ sign.c (titles, warnings)   │
               │                                    │ e.g. show Source vs Dest  │
               │                                    │ for sponsored finalize    │
               │                                    └─────────────────────────────┘
               │
               └──────────────────────────────────────────────────────────────────┐
                                                                                  │
  INDEPENDENT AXIS (do not confuse with 108):                                   │
                                                                                  │
      tz_parser_result  —  TZ_CONTINUE, TZ_BLO_DONE, TZ_ERR_INVALID_TAG, …      │
      parser_state.h                                                              │
      These values control the parser state machine and errors only.              │
      They are NOT Tezos protocol tags on the wire.                             │
                                                                                  │
```

**Where staking appears:** after the tag is already **`108`**, in the **entrypoint** byte (e.g. `6` = `stake`) and in the **parameter** expression where the protocol requires one (e.g. `Pair …` for `set_delegate_parameters`; `Unit` or other shapes for other entrypoints). No extra `tz_operation_tag` value is sent by the chain for those pseudo-operations.

#### What changes vs what does not

| Stage | Staking 2.0 change? | Notes |
|-------|---------------------|--------|
| Read shell / branch | **No** | Unchanged. |
| Read operation **tag** (5, 6, 17, 107, **108**, 110, …) | **No** for pseudo-ops | Staking uses **`TZ_OPERATION_TAG_TRANSACTION` (108)** like any transfer. |
| `tz_operation_descriptors[]` new row for “Stake” | **No** (not required) | Still one row for tag **108** → `transaction_fields`. |
| Parse transaction fields (amount, destination, entrypoint, params) | **Minor** | Same descriptor; watch **source ≠ destination** for Seoul+ `finalize_unstake`. |
| Map entrypoint index → name (`tz_step_read_smart_entrypoint`) | **No** (verified) | Matches Octez `Entrypoint_repr.smart_encoding` — see **§3.2**; re-check on protocol bump. |
| Micheline / parameter complexity | **Yes** (selective) | **`set_delegate_parameters`**: nicer decoding or labels for `Pair (limit, …)` (**P2**). |
| User-visible operation title in signing flow | **Yes** | Map **(tag 108 + entrypoint string)** → **Stake / Unstake / …** (**P1**); likely **`sign.c`** or shared helper, not a new `tz_operation_tag`. |

---

## 4. Gaps and recommendations (product / engineering)

1. **User-facing operation title**  
   Today the user may see a generic **Transaction** with entrypoint `stake` / … — consider surfacing **Stake** / **Unstake** / … as the primary title when tag is `108` and entrypoint matches (optional).

2. **`set_delegate_parameters` parameters**  
   Parameters are Micheline (`Pair …`). Ensure clear-signing shows **human-readable** limits/edges (or structured parsing), not only raw expression complexity.

3. **`finalize_unstake` after Seoul**  
   Support **non–self-transfer** transactions: show **source** (payer of fees) and **destination** (staker) distinctly so third-party finalization is obvious.

4. **Tests**  
   Add fixtures from real `octez-client` or wallet blobs: stake, unstake, finalize (self), finalize (third party), set_delegate_parameters.

5. **If the epic assumed new `tz_operation_tag` values**  
   Re-scope: either close “new tags” as **not applicable** per protocol docs, or narrow the ticket to **optional** UI/state tags that mirror entrypoint detection (internal only, same binary encoding).

---

## 5. Protocol versions to pin

Record the exact **protocol hash(es)** (e.g. Paris family, Quebec, Seoul, …) your release targets, and re-run the **entrypoint index** check for each. Constants such as `DELEGATE_PARAMETERS_ACTIVATION_DELAY` and `UNSTAKE_FINALIZATION_DELAY` affect **semantics**, not the Ledger parser’s byte layout, but are useful for QA matrices.

**Note:** §3.2 did not diff **Quebec** explicitly; if your release targets Quebec, add `proto_021_*` (or the exact folder name) to the `entrypoint_repr.ml` diff list for parity with §3.2.

---

## 6. References

- [Staking mechanism (Paris)](https://tezos.gitlab.io/paris/staking.html) — pseudo-operations as self-transfers + entrypoints  
- [Staking mechanism (Seoul)](https://tezos.gitlab.io/seoul/staking.html) — third-party `finalize_unstake`  
- Octez: `Operation_repr` / `Entrypoint_repr` in `src/proto_<HASH>/lib_protocol/` for binary truth

---

## 7. Re-scoped epic (Staking 2.0)

**Epic title (revised):**  
**Staking 2.0 — clear signing and UX for staking pseudo-operations (transaction + entrypoints)**

**Epic description (short):**  
Staking 2.0 pseudo-operations are **transfer operations** (tag `108`) with named entrypoints, not new manager operation kinds. This epic delivers **verified encoding alignment**, **better user-visible labeling**, **parameter clarity** where needed, and **tests** — not a batch of new `TZ_OPERATION_TAG_*` values (unless product later adds internal-only aliases).

**Explicitly out of scope for this epic**

- **Misleading new `tz_operation_tag` values for pseudo-ops** — Adding entries such as `TZ_OPERATION_TAG_STAKE` **as if they were the next on-chain manager operation tags** (alongside `107`…`115`). **Why:** Paris+ does **not** put stake/unstake/finalize/`set_delegate_parameters` on the wire as new tag bytes; it uses **`TZ_OPERATION_TAG_TRANSACTION` (`108`)** plus **entrypoint** + parameters. A new enum value in `tz_operation_tag` would **not** match any byte the parser reads first, and would confuse future readers about the protocol.  
  - **Allowed elsewhere (optional, not required for correctness):** a **separate** type or UI layer (e.g. “display kind” / internal classification) **after** parsing `108` and the entrypoint — that is **not** a protocol tag and is **in scope** if product wants it (see **P1**).

- Node/protocol semantics (cycles, delays, slashing) beyond what users must see when signing.

---

### Child issues (suggested) — product / UX

| # | Title | Type | Summary |
|---|--------|------|---------|
| **P1** | Clear-sign titles for staking pseudo-ops | Task | When operation is **Transaction** (`108`) and entrypoint matches one of the four pseudo-ops, show primary title **Stake** / **Unstake** / **Finalize unstake** / **Set delegate parameters** (exact copy per product) instead of or above generic “Transaction”. |
| **P2** | `set_delegate_parameters` — readable parameter display | Task | Present Micheline so **limit_of_staking_over_baking** and **edge_of_baking_over_staking** are human-readable (not only raw complex expression), within stack/UI limits. |
| **P3** | `finalize_unstake` — third-party payer (Seoul+) | Task | When **Source** ≠ **Destination**, show both distinctly (sponsor/bot vs staker); do not assume self-transfer in copy. |
| **P4** | Test vectors from wallets / `octez-client` | Task | Binary fixtures: stake, unstake, finalize (self), finalize (third party), `set_delegate_parameters`; **regression** on ordinary transactions; cover **batch (`Cons`)** if feasible. |
| **P5** | QA matrix & release note | Task | Table: app version × protocol hash × pseudo-ops smoke-tested; short user-facing paragraph (“staking ops shown as …”). |

**Story vs Task:** **Story** is often used when the team wants a **user story** (“as a signer, I see …”) that may hold **several sub-tasks**. Nothing here *requires* that: **P1–P3** are all **Tasks** if you prefer a flat backlog. Use **Story** only if your process expects epics → stories → sub-tasks.

**Splitting:** **P1** and **P3** are usually **one Task each** (small UI/copy changes). **P2** is the best candidate to **split** if scope grows — e.g. separate tasks for (a) extracting numeric limits from the Micheline `Pair` / validation, and (b) device strings and layout. Alternatively keep **one Task** with **sub-tasks** in your tracker.

**Dependency sketch:** **P1–P3** can run in parallel once product copy is agreed; **P4** tracks implementation and fixtures; **P5** last.

---

### What to **close or rewrite** from the old epic

- Issues titled “Add new operation **kind/tag** for Stake/Unstake/…” → **supersede** by **§1 / P1** or **close as obsolete** with pointer to §1 and §7 here.
- “Parser infrastructure — new `tz_operation_tag` rows” → **narrow** to optional **internal** classification only if still desired; **not** required for wire compatibility.

---

### Epic acceptance criteria (rollup)

1. **P1:** Users recognize staking pseudo-ops **by name** on device, not only “Transaction” + entrypoint.  
2. **P2:** `set_delegate_parameters` limits/edges are **understandable** at signing time.  
3. **P3:** Third-party `finalize_unstake` shows **Source** and **Destination** clearly when they differ.  
4. **P4:** Representative **test vectors** (wallets / `octez-client`).  
5. **P5:** QA matrix and release note complete.
