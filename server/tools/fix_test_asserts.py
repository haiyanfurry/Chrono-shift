#!/usr/bin/env python3
"""Fix test assertions in test_security.cpp - using chr() to avoid HTML entity decoding issues."""

import sys

# Build HTML entity strings at runtime using chr(38) = '&'
# This avoids write_to_file decoding HTML entities in our source code
AMP = chr(38) + "amp;"    # &
LT = chr(38) + "lt;"      # <
GT = chr(38) + "gt;"      # >
QUOT = chr(38) + "quot;"  # "
APOS = chr(38) + "apos;"  # '

def main():
    filepath = sys.argv[1] if len(sys.argv) > 1 else "server/tests/test_security.cpp"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = 0

    # Fix 1: Line 199 - result.find("<") -> result.find("<")
    # This also fixes line 206 (same pattern)
    old = '    TEST_ASSERT_TRUE(result.find("<") != std::string::npos);'
    new = '    TEST_ASSERT_TRUE(result.find("' + LT + '") != std::string::npos);'
    if old in content:
        content = content.replace(old, new)
        changes += 1
        print("Fix 1 applied: line 199 (and 206) - < -> <")

    # Fix 2a: Line 205 - result.find("&") -> result.find("&")
    old = '    TEST_ASSERT_TRUE(result.find("&") != std::string::npos);'
    new = '    TEST_ASSERT_TRUE(result.find("' + AMP + '") != std::string::npos);'
    if old in content:
        content = content.replace(old, new)
        changes += 1
        print("Fix 2a applied: line 205 - & -> &")

    # Fix 2c: Line 207 - result.find(">") -> result.find(">")
    old = '    TEST_ASSERT_TRUE(result.find(">") != std::string::npos);'
    new = '    TEST_ASSERT_TRUE(result.find("' + GT + '") != std::string::npos);'
    if old in content:
        content = content.replace(old, new)
        changes += 1
        print("Fix 2c applied: line 207 - > -> >")

    # Fix 2d: Line 208 - result.find("""") -> result.find(""")
    # The file has some encoding issue with triple quotes
    old_dq = '    TEST_ASSERT_TRUE(result.find("\u201c") != std::string::npos);'
    new_dq = '    TEST_ASSERT_TRUE(result.find("' + QUOT + '") != std::string::npos);'
    if old_dq in content:
        content = content.replace(old_dq, new_dq)
        changes += 1
        print("Fix 2d applied via unicode left-double-quote")

    old_dq2 = '    TEST_ASSERT_TRUE(result.find("\u201d") != std::string::npos);'
    if old_dq2 in content:
        content = content.replace(old_dq2, new_dq)
        changes += 1
        print("Fix 2d applied via unicode right-double-quote")

    # Try literal triple-quote pattern
    old_tq = '    TEST_ASSERT_TRUE(result.find("""") != std::string::npos);'
    if old_tq in content:
        content = content.replace(old_tq, new_dq)
        changes += 1
        print("Fix 2d applied via triple-quote")

    # Fix 3: Line 272 - escape_html expected value
    # The line has: TEST_ASSERT_EQUAL_STRING("<tag> & "quote'", result.c_str());
    # The actual expected value should be: <tag> & "quote'
    expected_new = '"' + LT + 'tag' + GT + ' ' + AMP + ' ' + QUOT + 'quote' + APOS + '"'
    new_line = '    TEST_ASSERT_EQUAL_STRING(' + expected_new + ', result.c_str());'

    # Try different possible patterns for line 272 (encoding issues may cause variations)
    patterns = [
        '    TEST_ASSERT_EQUAL_STRING("<tag> & "quote\'", result.c_str());',
        '    TEST_ASSERT_EQUAL_STRING("<tag> & \u201cquote\'", result.c_str());',
        '    TEST_ASSERT_EQUAL_STRING("<tag> & \u201cquote\u2019", result.c_str());',
        '    TEST_ASSERT_EQUAL_STRING("<tag> & "quote\'", result.c_str());',
    ]
    for pat in patterns:
        if pat in content:
            content = content.replace(pat, new_line)
            changes += 1
            print("Fix 3 applied: line 272 - escape_html expected value")
            break

    if changes == 0:
        print("WARNING: No changes were made! Patterns not found.")
        # Debug: show the actual content around line 208 and 272
        lines = content.split('\n')
        for i, line in enumerate(lines):
            if i + 1 in [208, 272]:
                print(f"  Line {i+1}: {repr(line)}")
    else:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"All {changes} fixes applied and saved to {filepath}")

if __name__ == '__main__':
    main()
