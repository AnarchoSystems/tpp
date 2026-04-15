/// Type definitions for the Java test harness generator.

struct JavaTestParam
{
    name : string;
    parseLine : string;
}

struct JavaTestDef
{
    testName : string;
    namespaceName : string;
    runnerClassName : string;
    params : list<JavaTestParam>;
    expectedOutputLiteral : string;
    callArgs : string;
}
