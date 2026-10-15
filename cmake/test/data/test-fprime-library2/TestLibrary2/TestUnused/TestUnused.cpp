// Library module that no project target depends on: must not be built by `all` (issue #4436)
namespace TestLibrary2 {
int unused() {
    return 42;
}
}  // namespace TestLibrary2
