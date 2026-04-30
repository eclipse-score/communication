// Canary binary: triggers heap-buffer-overflow detectable by ASan.
// Expected exit code with ASan: non-zero (sanitizer abort).
// Expected exit code without ASan: undefined behavior, may not crash.
int main()
{
    int* p = new int[1];
    p[1] = 42;  // out-of-bounds write
    delete[] p;
    return 0;
}
