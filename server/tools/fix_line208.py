#!/usr/bin/env python3
# Fix line 208 in test_security.cpp - replace triple-quote with html entity

fp = "server/tests/test_security.cpp"
with open(fp, 'r', encoding='utf-8') as f:
    lines = f.readlines()

old_line = lines[207]
print("Line 208 before: " + repr(old_line))

amp = chr(38)  # this builds the character &
new_line = "    TEST_ASSERT_TRUE(result.find(\"" + amp + "quot;\") != std::string::npos);\n"
lines[207] = new_line

with open(fp, 'w', encoding='utf-8') as f:
    f.writelines(lines)

print("Line 208 after: " + repr(lines[207]))
print("Fix applied!")
