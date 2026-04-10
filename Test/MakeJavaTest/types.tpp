/// Type definitions for the Java test harness generator.

struct JavaTestParam
{
    name : string;
    parseLine : string;
}

struct JavaTestDef
{
    testName : string;
    params : list<JavaTestParam>;
    expectedOutputLiteral : string;
    callArgs : string;
}
