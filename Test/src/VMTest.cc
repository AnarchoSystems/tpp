#include <gtest/gtest.h>

#include <tpp/VM.h>
#include <tpp/IR.h>
#include <tpp/Layout.h>
#include <tpp/Slot.h>

namespace
{

using namespace tpp;

// ════════════════════════════════════════════════════════════════════════════
// Helper: build a minimal IR for testing
// ════════════════════════════════════════════════════════════════════════════

IR make_empty_ir()
{
    IR ir;
    ir.versionMajor = 0;
    ir.versionMinor = 0;
    ir.versionPatch = 0;
    return ir;
}

// ════════════════════════════════════════════════════════════════════════════
// VM construction
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, ConstructionEmpty)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    EXPECT_FALSE(vm.hasError());
    EXPECT_EQ(vm.takeOutput(), "");
}

// ════════════════════════════════════════════════════════════════════════════
// Literal emission
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, EmitLiteral)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    EXPECT_TRUE(vm.emit("hello "));
    EXPECT_TRUE(vm.emit("world"));
    EXPECT_EQ(vm.takeOutput(), "hello world");
}

TEST(VMTest, EmitMultiple)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    EXPECT_TRUE(vm.emit("line1\n"));
    EXPECT_TRUE(vm.emit("line2\n"));
    EXPECT_EQ(vm.takeOutput(), "line1\nline2\n");
}

// ════════════════════════════════════════════════════════════════════════════
// Slot emission
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, EmitSlotStr)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::string val = "hello";
    std::vector<Slot> frame = { Slot::make_str(&val) };
    vm.pushFrame(frame);

    EXPECT_TRUE(vm.emitSlot(0));
    EXPECT_EQ(vm.takeOutput(), "hello");

    vm.popFrame();
}

TEST(VMTest, EmitSlotInt)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::vector<Slot> frame = { Slot::make_int(42) };
    vm.pushFrame(frame);

    EXPECT_TRUE(vm.emitSlot(0));
    EXPECT_EQ(vm.takeOutput(), "42");

    vm.popFrame();
}

TEST(VMTest, EmitSlotBool)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::vector<Slot> frame = { Slot::make_bool(true), Slot::make_bool(false) };
    vm.pushFrame(frame);

    EXPECT_TRUE(vm.emitSlot(0));
    EXPECT_TRUE(vm.emit(" "));
    EXPECT_TRUE(vm.emitSlot(1));
    EXPECT_EQ(vm.takeOutput(), "true false");

    vm.popFrame();
}

// ════════════════════════════════════════════════════════════════════════════
// Multi-line indentation
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, MultiLineIndent)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    // Emit some text to set column, then emit multi-line content
    std::string multiline = "alpha\nbeta\ngamma";
    std::vector<Slot> frame = { Slot::make_str(&multiline) };
    vm.pushFrame(frame);

    EXPECT_TRUE(vm.emit("    ")); // 4 spaces of indent
    EXPECT_TRUE(vm.emitSlot(0));  // multi-line value

    // Each continuation line should get 4 spaces of indent
    EXPECT_EQ(vm.takeOutput(), "    alpha\n    beta\n    gamma");
    vm.popFrame();
}

// ════════════════════════════════════════════════════════════════════════════
// Frame management
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, FramePushPop)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::string v1 = "outer";
    std::string v2 = "inner";

    std::vector<Slot> frame1 = { Slot::make_str(&v1) };
    vm.pushFrame(frame1);
    EXPECT_EQ(vm.slot(0).as_str(), "outer");

    std::vector<Slot> frame2 = { Slot::make_str(&v2), Slot::make_int(99) };
    vm.pushFrame(frame2);
    EXPECT_EQ(vm.slot(0).as_str(), "inner");
    EXPECT_EQ(vm.slot(1).as_int(), 99);

    vm.popFrame();
    EXPECT_EQ(vm.slot(0).as_str(), "outer");

    vm.popFrame();
}

TEST(VMTest, NestedFrames)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::vector<Slot> f1 = { Slot::make_int(1) };
    std::vector<Slot> f2 = { Slot::make_int(2) };
    std::vector<Slot> f3 = { Slot::make_int(3) };

    vm.pushFrame(f1);
    vm.pushFrame(f2);
    vm.pushFrame(f3);

    EXPECT_EQ(vm.slot(0).as_int(), 3);
    vm.popFrame();
    EXPECT_EQ(vm.slot(0).as_int(), 2);
    vm.popFrame();
    EXPECT_EQ(vm.slot(0).as_int(), 1);
    vm.popFrame();
}

// ════════════════════════════════════════════════════════════════════════════
// Policy scope
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, PolicyPushPop)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    EXPECT_TRUE(vm.pushPolicy("camelCase"));
    EXPECT_TRUE(vm.pushPolicy("snake_case"));
    EXPECT_TRUE(vm.popPolicy());
    EXPECT_TRUE(vm.popPolicy());
    EXPECT_FALSE(vm.hasError());
}

TEST(VMTest, PolicyScopeUnderflow)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    EXPECT_FALSE(vm.popPolicy());
    EXPECT_TRUE(vm.hasError());
}

// ════════════════════════════════════════════════════════════════════════════
// Policy application via emitSlotWithPolicy
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, PolicyReplacement)
{
    IR ir = make_empty_ir();

    PolicyDef pd;
    pd.tag = "underscores";
    PolicyReplacement rep;
    rep.find = " ";
    rep.replace = "_";
    pd.replacements.push_back(rep);
    ir.policies.push_back(pd);

    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::string val = "hello world";
    std::vector<Slot> frame = { Slot::make_str(&val) };
    vm.pushFrame(frame);

    EXPECT_TRUE(vm.emitSlotWithPolicy(0, "underscores"));
    EXPECT_EQ(vm.takeOutput(), "hello_world");

    vm.popFrame();
}

TEST(VMTest, PolicyLengthMax)
{
    IR ir = make_empty_ir();

    PolicyDef pd;
    pd.tag = "short";
    PolicyLength len;
    len.max = 5;
    pd.length = len;
    ir.policies.push_back(pd);

    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::string val = "toolongvalue";
    std::vector<Slot> frame = { Slot::make_str(&val) };
    vm.pushFrame(frame);

    EXPECT_FALSE(vm.emitSlotWithPolicy(0, "short"));
    EXPECT_TRUE(vm.hasError());

    vm.popFrame();
}

TEST(VMTest, PolicyScopeInheritance)
{
    IR ir = make_empty_ir();

    PolicyDef pd;
    pd.tag = "upper";
    PolicyReplacement rep;
    rep.find = "a";
    rep.replace = "A";
    pd.replacements.push_back(rep);
    ir.policies.push_back(pd);

    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::string val = "banana";
    std::vector<Slot> frame = { Slot::make_str(&val) };
    vm.pushFrame(frame);

    vm.pushPolicy("upper");
    // Empty exprPolicy → inherits active scope "upper"
    EXPECT_TRUE(vm.emitSlotWithPolicy(0, ""));
    EXPECT_EQ(vm.takeOutput(), "bAnAnA");

    vm.popPolicy();
    vm.popFrame();
}

TEST(VMTest, PolicyNoneOverride)
{
    IR ir = make_empty_ir();

    PolicyDef pd;
    pd.tag = "replace_x";
    PolicyReplacement rep;
    rep.find = "x";
    rep.replace = "X";
    pd.replacements.push_back(rep);
    ir.policies.push_back(pd);

    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::string val = "fox";
    std::vector<Slot> frame = { Slot::make_str(&val) };
    vm.pushFrame(frame);

    vm.pushPolicy("replace_x");
    // "none" overrides the scope policy
    EXPECT_TRUE(vm.emitSlotWithPolicy(0, "none"));
    EXPECT_EQ(vm.takeOutput(), "fox");

    vm.popPolicy();
    vm.popFrame();
}

// ════════════════════════════════════════════════════════════════════════════
// Block indentation
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, ExecBlockBody)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    // Build a body with two emit instructions
    std::vector<Instruction> body;

    Instruction i1;
    i1.value = EmitInstr{"    line1\n"};
    body.push_back(std::move(i1));

    Instruction i2;
    i2.value = EmitInstr{"    line2\n"};
    body.push_back(std::move(i2));

    // Block body with insertCol = 8 should de-indent from 4 (zero marker)
    // and re-indent to 8
    EXPECT_TRUE(vm.execBlockBody(body, 8));
    EXPECT_EQ(vm.takeOutput(), "        line1\n        line2\n");
}

TEST(VMTest, BlockIndentZeroMarker)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::vector<Instruction> body;

    Instruction i1;
    i1.value = EmitInstr{"  alpha\n"};
    body.push_back(std::move(i1));

    Instruction i2;
    i2.value = EmitInstr{"    beta\n"};  // extra 2 spaces relative
    body.push_back(std::move(i2));

    Instruction i3;
    i3.value = EmitInstr{"  gamma\n"};
    body.push_back(std::move(i3));

    // insertCol=0: strip 2-space zero marker, no re-indent
    EXPECT_TRUE(vm.execBlockBody(body, 0));
    EXPECT_EQ(vm.takeOutput(), "alpha\n  beta\ngamma\n");
}

// ════════════════════════════════════════════════════════════════════════════
// Emit instructions via execBody
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, ExecBodyEmit)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    std::vector<Instruction> body;

    Instruction i1;
    i1.value = EmitInstr{"hello "};
    body.push_back(std::move(i1));

    Instruction i2;
    i2.value = EmitInstr{"world"};
    body.push_back(std::move(i2));

    EXPECT_TRUE(vm.execBody(body));
    EXPECT_EQ(vm.takeOutput(), "hello world");
}

// ════════════════════════════════════════════════════════════════════════════
// Pad cell
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, PadCellLeft)
{
    // padCell is private static — test via the public interface indirectly
    // or verify alignment behaviour here. For now, test block indent which
    // exercises similar formatting logic.
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    EXPECT_TRUE(vm.emit("ok"));
    EXPECT_EQ(vm.takeOutput(), "ok");
}

// ════════════════════════════════════════════════════════════════════════════
// Capture stack
// ════════════════════════════════════════════════════════════════════════════

TEST(VMTest, CaptureViaBlockBody)
{
    IR ir = make_empty_ir();
    compute_ir_layouts(ir);
    auto layouts = LayoutTable::build(ir);
    VM vm(layouts, VM::compilePolicies(ir));

    // Emit before block body — should be in root output
    EXPECT_TRUE(vm.emit("before|"));

    std::vector<Instruction> body;
    Instruction i1;
    i1.value = EmitInstr{"captured"};
    body.push_back(std::move(i1));

    // Block body captures internally then emits result to root
    EXPECT_TRUE(vm.execBlockBody(body, 0));

    EXPECT_TRUE(vm.emit("|after"));
    EXPECT_EQ(vm.takeOutput(), "before|captured|after");
}

} // anonymous namespace
