# Fix test assertions in test_security.cpp
$filePath = "server/tests/test_security.cpp"
$content = [System.IO.File]::ReadAllText((Resolve-Path $filePath), [System.Text.Encoding]::UTF8)

# Fix 1: Line 199 - change result.find("<") to result.find("<")
$old1 = '    TEST_ASSERT_TRUE(result.find("<") != std::string::npos);'
$new1 = '    TEST_ASSERT_TRUE(result.find("<") != std::string::npos);'
$content = $content.Replace($old1, $new1)
Write-Host "Fix 1 applied"

# Fix 2a: Line 205 - result.find("&") -> result.find("&")
$old2a = '    TEST_ASSERT_TRUE(result.find("&") != std::string::npos);'
$new2a = '    TEST_ASSERT_TRUE(result.find("&") != std::string::npos);'
$content = $content.Replace($old2a, $new2a)
Write-Host "Fix 2a applied"

# Fix 2b: Line 206 - result.find("<") -> result.find("<")
$old2b = '    TEST_ASSERT_TRUE(result.find("<") != std::string::npos);'
$new2b = '    TEST_ASSERT_TRUE(result.find("<") != std.string::npos);'

# Fix 2c: Line 207 - result.find(">") -> result.find(">")
$old2c = '    TEST_ASSERT_TRUE(result.find(">") != std::string::npos);'
$new2c = '    TEST_ASSERT_TRUE(result.find(">") != std::string::npos);'
$content = $content.Replace($old2c, $new2c)
Write-Host "Fix 2c applied"

# Fix 2d: Line 208 - result.find("""") -> result.find(""")
# The triple-quote pattern
$old2d = '    TEST_ASSERT_TRUE(result.find(""") != std::string::npos);'
$new2d = '    TEST_ASSERT_TRUE(result.find(""") != std::string::npos);'
$content = $content.Replace($old2d, $new2d)
Write-Host "Fix 2d applied"

# Fix 3: Line 272 - escape_html expected value
$old3 = '    TEST_ASSERT_EQUAL_STRING("<tag> & "quote' + "'" + '", result.c_str());'
$new3 = '    TEST_ASSERT_EQUAL_STRING("<tag> & "quote'", result.c_str());'
$content = $content.Replace($old3, $new3)
Write-Host "Fix 3 applied"

[System.IO.File]::WriteAllText((Resolve-Path $filePath), $content, [System.Text.Encoding]::UTF8)
Write-Host "All fixes applied and saved!"
