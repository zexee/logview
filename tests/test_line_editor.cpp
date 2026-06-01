#include "ui/line_editor.h"

#include <cstdio>

namespace {

int passed = 0;
int failed = 0;

#define CHECK(expr)                                                                            \
    do {                                                                                       \
        if (expr) {                                                                            \
            ++passed;                                                                          \
        } else {                                                                               \
            ++failed;                                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                       \
        }                                                                                      \
    } while (false)

void test_insert_and_submit() {
    lv::ui::LineEditor editor;
    editor.start(":", "");
    CHECK(editor.active());
    editor.handle_key('q');
    CHECK(editor.text() == "q");
    CHECK(editor.handle_key('\n') == lv::ui::LineEditorEvent::Submitted);
    CHECK(!editor.active());
    CHECK(editor.text() == "q");
}

void test_cursor_and_backspace() {
    lv::ui::LineEditor editor;
    editor.start("", "abcd");
    editor.handle_key(KEY_LEFT);
    editor.handle_key(KEY_LEFT);
    editor.handle_key('X');
    CHECK(editor.text() == "abXcd");
    editor.handle_key(KEY_BACKSPACE);
    CHECK(editor.text() == "abcd");
}

void test_delete_home_end() {
    lv::ui::LineEditor editor;
    editor.start("", "abcd");
    editor.handle_key(KEY_HOME);
    editor.handle_key(KEY_DC);
    CHECK(editor.text() == "bcd");
    editor.handle_key(KEY_END);
    editor.handle_key('e');
    CHECK(editor.text() == "bcde");
}

void test_cancel() {
    lv::ui::LineEditor editor;
    editor.start(":", "quit");
    CHECK(editor.handle_key(27) == lv::ui::LineEditorEvent::Canceled);
    CHECK(!editor.active());
    CHECK(editor.text().empty());
}

} // namespace

int main() {
    test_insert_and_submit();
    test_cursor_and_backspace();
    test_delete_home_end();
    test_cancel();
    std::printf("passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
