enum StepArgument
{
    None, DocString(string), DataTable(list<list<string>>)
};

struct Step
{
    name : string;
    regexArgs : list<string>;
    argument : StepArgument;
};

struct Scenario
{
    name : string;
    steps : list<Step>;
};

struct Feature
{
    name : string;
    scenarios : list<Scenario>;
};