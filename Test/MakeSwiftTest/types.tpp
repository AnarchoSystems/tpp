/// Type definitions for the Swift test harness generator.

struct SwiftTestParam
{
    name : string;
    parseLine : string;
}

struct SwiftTestDef
{
    testName : string;
    params : list<SwiftTestParam>;
    expectedOutputLiteral : string;
    callArgs : string;
    hasPolicies : bool;
}
