template render_test(defs: Defs)
#include <gtest/gtest.h>
#include <tpp/Compiler.h>
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
    tpp::CompilerOutput compilerOutput = nlohmann::json::parse(@defs.compilerOutputJson@).get<tpp::CompilerOutput>();
    auto fun = compilerOutput.get_function<@for input in defs.inputs | sep=", "@@input.type@@end for@>("main");
    
    @for input in defs.inputs | enumerator=idx sep="\n"@
    @input.type@ input@idx@ = nlohmann::json::parse(R"_(@input.value@)_").get<@input.type@>();
    @end for@
    auto output = fun(@for input in defs.inputs | enumerator=idx sep=", "@input@idx@@end for@);
    EXPECT_EQ(output, @defs.expectedOutput@);
}

END