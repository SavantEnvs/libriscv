// Build-time LeakSanitizer off-switch (fleet convention, SPEC §6.2 item 15).
//
// -fsanitize=address always bundles LeakSanitizer; leaks are not the defect class this target is
// fuzzed for, and a leak flood would bury the emulator's real memory/UB findings. LSan consults this
// hook both at process exit and in libFuzzer's per-input leak check, so leak detection is skipped
// while ASan's memory-error checks and UBSan stay fully active. build.sh compiles it with $SANITIZER_FLAGS and links it into every fuzz target and every
// -standalone reproducer.
extern "C" int __lsan_is_turned_off() { return 1; }
