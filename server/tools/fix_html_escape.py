#!/usr/bin/env python3
"""Fix HTML entity escaping in SecurityManager.cpp"""
filepath = "src/security/SecurityManager.cpp"

with open(filepath, "r", encoding="utf-8") as f:
    content = f.read()

# Build the replacement strings without using HTML entities
# Target: result += """; break;
amp = "&"  # literal ampersand
quot = amp + "quot;"

old_amp = 'result += "&";  break;'
new_amp = 'result += "' + amp + "amp;" + '";  break;'

old_lt = 'result += "<";   break;'
new_lt = 'result += "' + amp + "lt;" + '";   break;'

old_gt = 'result += ">";   break;'
new_gt = 'result += "' + amp + "gt;" + '";   break;'

old_quot = 'case \'"\':  result += """; break;'
new_quot = 'case \'"\':  result += "' + quot + '"; break;'

old_apos = "result += \"'\";  break;"
new_apos = "result += \"" + amp + "apos;" + "\";  break;"

changes = 0
for old, new in [(old_amp, new_amp), (old_lt, new_lt), (old_gt, new_gt),
                  (old_quot, new_quot), (old_apos, new_apos)]:
    if old in content:
        content = content.replace(old, new)
        changes += 1
        print(f"Fixed: {old.strip()} -> {new.strip()}")
    else:
        print(f"NOT FOUND: {repr(old)}")

print(f"Total changes: {changes}")

with open(filepath, "w", encoding="utf-8") as f:
    f.write(content)
print("Done")
