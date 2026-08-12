template render_test(defs: Defs)
#include <gtest/gtest.h>
#include <tpp/Compiler.h>
#include <tpp/IR.h>
#include <tpp/Runtime.h>
#include "@defs.testName@_functions.h"

TEST(static_tests, @defs.testName@_compiled)
{
    @for input in defs.inputs | enumerator=idx sep="\n"@
    @input.type@ input@idx@ = nlohmann::json::parse(R"_(@input.value@)_").get<@input.type@>();
    @end for@
    auto output = @defs.testName@::main(@for input in defs.inputs | enumerator=idx sep=", "@input@idx@@end for@);
    EXPECT_EQ(output, @defs.expectedOutput@);
}

TEST(static_tests, @defs.testName@_dynamic_binding)
{
    tpp::IR iRep = nlohmann::json::parse(@defs.iRepJson@).get<tpp::IR>();
    const tpp::FunctionDef *fn = nullptr;
    std::string lookupError;
    @if defs.hasPreviewSignature@
    ASSERT_TRUE(tpp::get_function(iRep, "@defs.previewFunctionName@", {@for sig in defs.previewSignature | sep=", "@"@sig@"@end for@}, fn, lookupError)) << lookupError;
    @else@
    ASSERT_TRUE(tpp::get_function(iRep, "@defs.previewFunctionName@", fn, lookupError)) << lookupError;
    @end if@
    nlohmann::json inputArray = nlohmann::json::array({@for input in defs.inputs | sep=", "@nlohmann::json::parse(R"_(@input.value@)_")@end for@});
    std::string dynOutput, renderError;
    ASSERT_TRUE(tpp::render_function(iRep, *fn, inputArray, dynOutput, renderError)) << renderError;
    EXPECT_EQ(dynOutput, @defs.expectedOutput@);
}

END