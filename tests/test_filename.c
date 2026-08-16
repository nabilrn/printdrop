#include "printdrop/filename.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define PD_TEST_ASSERT(condition)                                                                    \
    do {                                                                                             \
        if (!(condition)) {                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                    \
            failures += 1;                                                                           \
        }                                                                                            \
    } while (0)

static void assert_sanitized(const char *input, const char *expected)
{
    char output[PD_FILENAME_CAPACITY];
    size_t required = 0U;

    PD_TEST_ASSERT(pd_filename_sanitize(input, output, sizeof(output), &required) == PD_FILENAME_OK);
    PD_TEST_ASSERT(strcmp(output, expected) == 0);
    PD_TEST_ASSERT(required == strlen(expected) + 1U);
}

static void test_safe_names_are_preserved(void)
{
    assert_sanitized("skripsi-final.pdf", "skripsi-final.pdf");
    assert_sanitized("Foto KTP 01.jpg", "Foto KTP 01.jpg");
    assert_sanitized("laporan_(revisi).docx", "laporan_(revisi).docx");
}

static void test_windows_path_characters_are_neutralized(void)
{
    assert_sanitized("../../Windows/System32/cmd.exe", ".._.._Windows_System32_cmd.exe");
    assert_sanitized("folder\\file.pdf", "folder_file.pdf");
    assert_sanitized("a<b>c:d\"e|f?g*h.txt", "a_b_c_d_e_f_g_h.txt");
    assert_sanitized("trailing...   ", "trailing");
}

static void test_windows_device_names_are_neutralized(void)
{
    assert_sanitized("CON", "_CON");
    assert_sanitized("con.txt", "_con.txt");
    assert_sanitized("NUL.pdf", "_NUL.pdf");
    assert_sanitized("COM1", "_COM1");
    assert_sanitized("lpt9.log", "_lpt9.log");
    assert_sanitized("COM10.txt", "COM10.txt");
}

static void test_empty_and_dot_only_names_get_fallback(void)
{
    assert_sanitized("", "file");
    assert_sanitized(".", "file");
    assert_sanitized("..", "file");
    assert_sanitized("...", "file");
    assert_sanitized("   ", "file");
}

static void test_size_and_buffer_limits(void)
{
    char too_long[PD_FILENAME_MAX_BYTES + 2U];
    char output[8] = "stale";
    size_t required = 0U;

    memset(too_long, 'a', sizeof(too_long) - 1U);
    too_long[sizeof(too_long) - 1U] = '\0';

    PD_TEST_ASSERT(pd_filename_sanitize(too_long, output, sizeof(output), NULL) ==
                   PD_FILENAME_TOO_LONG);
    PD_TEST_ASSERT(pd_filename_sanitize("skripsi.pdf", output, sizeof(output), &required) ==
                   PD_FILENAME_BUFFER_TOO_SMALL);
    PD_TEST_ASSERT(output[0] == '\0');
    PD_TEST_ASSERT(required == strlen("skripsi.pdf") + 1U);
}

int main(void)
{
    test_safe_names_are_preserved();
    test_windows_path_characters_are_neutralized();
    test_windows_device_names_are_neutralized();
    test_empty_and_dot_only_names_get_fallback();
    test_size_and_buffer_limits();

    if (failures != 0) {
        fprintf(stderr, "%d filename test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop filename tests passed.");
    return 0;
}
