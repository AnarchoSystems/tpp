template main(feature: Feature)
@for scenario in feature.scenarios | sep="\n"@
TEST(@feature.name@, @scenario.name@)
{
    @for step in scenario.steps@
    @switch step.argument@
    @case None@
    @step.name@(@for arg in step.regexArgs | sep=", "@@arg@@@end for@);
    @end case@
    @case DocString(docStr)@
    @step.name@(@for arg in step.regexArgs | sep=", " followedBy=", "@@arg@@@end for@@docStr@);
    @end case@
    @case DataTable(rows)@
    {
        std::vector<TableRow> table = {
            @for row in rows@
            {@for cell in row | sep=", "@@cell@@@end for@},
            @end for@
        };
        @step.name@(@for arg in step.regexArgs | sep=", " followedBy=", "@@arg@@@end for@table);
    }
    @end case@
    @end switch@
    @end for@
}
@end for@
END