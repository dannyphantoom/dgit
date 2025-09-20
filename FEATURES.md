# dgit Feature Status and Gaps

This document tracks what currently works, what's stubbed or incomplete, and priority issues found while attempting to run tests and review the codebase.

## Build & Test Status

- CMake is not installed on this machine; the test runner now falls back to Makefile.
- Makefile build currently fails due to missing public headers (e.g., `dgit/sha1.hpp`, `dgit/repository.hpp`). The sources include these headers, but they are not present under `include/dgit/`.
- Because of the above, unit tests and integration tests cannot be executed yet.

## Implemented (per code/README)

- Core object types: Blob, Tree, Commit, Tag (implementations in `src/objects/object.cpp`).
- SHA-1 implementation (`src/core/sha1.cpp`).
- Object database for loose objects with zlib compression (`src/objects/object_database.cpp`).
- Basic repository scaffolding: `.git` layout, `HEAD`, `config`, `refs` (`src/core/repository.cpp`, `src/refs/refs.cpp`, `src/core/config.cpp`, `src/core/index.cpp`).
- CLI command registrations and several command handlers (`src/commands`).

## Major Gaps and Issues

1) Public headers missing
- All sources and tests `#include "dgit/*.hpp"` but no headers exist in `include/dgit/`. This blocks compilation.

2) Duplicate/Conflicting implementations
- `MergeCommand::execute` is implemented in both `src/commands/commands.cpp` and `src/merge/merge.cpp`, which will cause ODR/link errors.

3) Object model correctness
- `Tree::add_entry` serializes by writing `id` with `oss.write(id.c_str(), 20)` where `id` is a 40-char hex string. Writing 20 raw chars is incorrect (should write binary 20-byte SHA). This will corrupt tree storage.
- `Object::deserialize` creates empty `Tree`, `Commit`, `Tag` instead of parsing `content` back into structured fields; returned objects may not match serialized data.

4) Object database caching API mismatch
- `ObjectDatabase` calls `object->clone()` but `Object` does not define `clone()` in the shown implementation. This will not compile without adding virtual `clone()` to `Object` and overrides in derived classes.

5) Index format is highly simplified
- The index read/write code (`src/core/index.cpp`) does not follow the Git index (DIRC) spec; field parsing/writing is ad-hoc and likely incompatible. It may be okay for toy use, but tests relying on exact behavior may fail.

6) Refs and HEAD handling
- Refs implementation is simplified; reflog writes ignore failures and format is non-standard. `Refs::create_ref` symbolic path validation may fail for fresh repos.

7) Network/pack/merge are placeholders
- Network (`src/network/network.cpp`), packfile (`src/packfile/packfile.cpp`), and merge (`src/merge/merge.cpp`) contain substantial placeholders and stubs; they are not production-ready and may not be link-safe.

8) Time/author fields
- Commit author/committer timestamps are seconds since epoch without timezone, not the full Git format. Fine for demo, but not interoperable.

## Recommended Fix Order (High → Low)

1. Add missing public headers under `include/dgit/` for: `sha1.hpp`, `object.hpp`, `object_database.hpp`, `repository.hpp`, `refs.hpp`, `config.hpp`, `index.hpp`, `commands.hpp`, plus minimal for `merge.hpp`, `network.hpp`, `packfile.hpp` (even if stubs) so build links.
2. Remove one duplicate `MergeCommand::execute` (keep it in one place; prefer `src/merge/merge.cpp`) and adjust headers accordingly.
3. Fix `Tree` serialization to write binary SHA bytes (20-byte binary) and update deserialization to parse correctly; ensure `Object::deserialize` reconstructs real objects.
4. Add `virtual std::unique_ptr<Object> clone() const` to `Object` and derived implementations; update `ObjectDatabase` cache accordingly.
5. Tighten `Index` read/write or mark the format as project-internal and keep consistent between save/load. Add tests to validate round-trips.
6. Add minimal `CMake` detection in CI or ensure Makefile builds a `tests` target (if GTest is present) to run unit tests without CMake.

## Testing Plan (once headers compile)

- Unit tests
  - SHA-1: Known vectors, streaming, file hashing (already present).
  - Object round-trip: serialize→store→load→deserialize should preserve fields.
  - Index: add/remove/list, save→load round-trip integrity.
  - Refs: create/update/delete, HEAD branch, symbolic refs.

- Integration tests
  - repo init → add files → status → commit → log.
  - branch create/list/checkout; HEAD content checks.

- Negative tests
  - Opening non-repo path throws.
  - Adding missing file errors.
  - Reading non-blob as blob throws.

## Quick Wins

- Ensure `test.sh` gracefully falls back to Makefile (done).
- Create skeleton headers to unblock compilation; iterate until Makefile builds `build/bin/dgit`.
- Gate unimplemented subsystems behind compile-time flags to avoid linking placeholders until ready.

---

If you want, I can generate the initial `include/dgit/*.hpp` headers to match the current sources and get the Makefile build green; then we can iterate on fixing the `Tree` and `ObjectDatabase` issues and re-enable unit tests.


