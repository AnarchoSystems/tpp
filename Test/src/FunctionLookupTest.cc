#include <gtest/gtest.h>

#include <tpp/Compiler.h>
#include <tpp/Runtime.h>

namespace
{

tpp::IR compile_ir(const std::string &templateSrc)
{
    tpp::TppProject project;
    project.add_template_source(templateSrc, "memory://lookup/template.tpp");

    tpp::LexedProject lexed;
    tpp::ParsedProject parsed;
    tpp::IR ir;
    std::vector<tpp::DiagnosticLSPMessage> diagnostics;

    const bool ok = tpp::lex(project, lexed, diagnostics) &&
                    tpp::parse(lexed, parsed, diagnostics) &&
                    tpp::compile(parsed, ir, diagnostics);

    EXPECT_TRUE(ok) << nlohmann::json(diagnostics).dump(2);
    return ir;
}

const char *kOverloadedMain =
    "template main(name: string)\n"
    "string:@name@\n"
    "END\n"
    "\n"
    "template main(id: int)\n"
    "int:@id@\n"
    "END\n";

} // namespace

TEST(FunctionLookupTest, NameLookupAmbiguous)
{
    const tpp::IR ir = compile_ir(kOverloadedMain);

    const tpp::FunctionDef *fn = nullptr;
    std::string error;
    EXPECT_FALSE(tpp::get_function(ir, "main", fn, error));
    EXPECT_EQ(error, "function 'main' is ambiguous; matching overloads: main(string), main(int)");
}

TEST(FunctionLookupTest, SignatureLookupAndTypedLookupDisambiguate)
{
    const tpp::IR ir = compile_ir(kOverloadedMain);

    const tpp::FunctionDef *stringFn = nullptr;
    std::string error;
    EXPECT_TRUE(tpp::get_function(ir, "main", std::vector<std::string>{"string"}, stringFn, error))
        << error;

    std::string rendered;
    std::string renderError;
    EXPECT_TRUE(tpp::render_function(ir, *stringFn, nlohmann::json("Ada"), rendered, renderError))
        << renderError;
    EXPECT_EQ(rendered, "string:Ada");

    tpp::TypedRenderFunction<std::string> stringMain;
    EXPECT_TRUE(tpp::get_function<std::string>(ir, "main", stringMain, error)) << error;
    EXPECT_EQ(stringMain("Bob"), "string:Bob");

    tpp::TypedRenderFunction<int> intMain;
    EXPECT_TRUE(tpp::get_function<int>(ir, "main", intMain, error)) << error;
    EXPECT_EQ(intMain(7), "int:7");
}
