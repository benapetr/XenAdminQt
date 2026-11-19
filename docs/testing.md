# Testing

The automated tests are QtTest applications under `tests/`. They run offline;
nothing in them talks to a real XCP-ng or XenServer host.

| Project | Covers |
|---------|--------|
| `tests/xenlib/xenlib-tests.pro` | Library logic: cache and typed objects, search and grouping, alerts, OVF handling, JSON-RPC parsing, actions against a local test transport |
| `tests/xenadmin-ui/xenadmin-ui-tests.pro` | A few UI helpers (icon lookup) |

`tests/tests.pro` builds both.

## Building and running

The test projects link the library from the main qmake build in `release/`
(`LIBS += -L../../release/xenlib -lxenlib`), so build that first, then build the
tests from their own directories so the relative path resolves:

```bash
cd release && make -j12 && cd ..

cd tests/xenlib
qmake xenlib-tests.pro      # use the qmake of the Qt that built release/
make -j12
./xenlib-tests              # macOS: ./xenlib-tests.app/Contents/MacOS/xenlib-tests
```

The UI tests build the same way from `tests/xenadmin-ui/`.

If linking fails on missing zlib symbols, add `LIBS+=-lz` to the qmake call (or
build `release/` with `CONFIG+=no_zlib`).

QtTest options apply. Pass test function names to run only those, for example
`./xenlib-tests cacheBuildsEveryKnownType`, and `-v2` for verbose output. Set
`QT_QPA_PLATFORM=offscreen` when running UI tests on a machine where windows
must not appear or take focus.

## Writing tests

- Library model code can be tested by filling a cache: create a
  `XenConnection`, call `GetCache()->Update(type, ref, record)` with the fields
  the code reads, then resolve typed objects with `ResolveObject<T>()`. See
  `generalTabModelHelpersMatchCSharp` and `vtpmRulesMatchCSharp` in
  `tests/xenlib/test_main.cpp`.
- Version-dependent logic reads the coordinator: add a `Host` with
  `software_version` (`xapi`, `platform_version`) and a `Pool` whose `master`
  points at it.
- Tests that exercise the transport (`ConnectionWorker`, sessions, heartbeat)
  use the local TLS server in `test_transportserver.h` with the certificates in
  `transport-testdata/`. They open local sockets and are slower than the rest.

## Limitations

- CI (`.github/workflows/build.yml`) builds the application but does not run the
  tests; run them locally.
- The tests do not cover live-server behaviour. Workflows that change server
  state need checking against a test XCP-ng/XenServer pool.
- The test projects are qmake only.
