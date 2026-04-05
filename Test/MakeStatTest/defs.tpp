template render_test(defs: Defs)
#include <gtest/gtest.h>
#include "@defs.testName@_functions.h"

TEST(static_tests, @defs.testName@)
{
    @for input in defs.inputs | iteratorVar=idx sep="\n"@
    @input.type@ input@idx@ = nlohmann::json::parse(R"(@input.value@)");
    @end for@
    auto output = @defs.testName@::main(@for input in defs.inputs | iteratorVar=idx sep=", "@input.type@ input@idx@@end for@);
    EXPECT_EQ(output, @defs.expectedOutput@);
}

END