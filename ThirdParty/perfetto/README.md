# perfetto

The C++ tracing SDK of [Perfetto](https://perfetto.dev). The framework uses it to write the trace of the trace service
(`--Trace <file>`), which can be opened in [ui.perfetto.dev](https://ui.perfetto.dev) and queried with the Perfetto trace processor.

Only `FslDemoService.Trace.Perfetto` includes the SDK. Apps and hosts use the trace service, which has no Perfetto types in its
interface (see [Doc/Trace.md](../../Doc/Trace.md)).

The SDK is released as two amalgamated files without a build script, so the recipe builds it with a CMake script of its own.

License: Apache 2.0. For more information see the [official repository](https://github.com/google/perfetto).
