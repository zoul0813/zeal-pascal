#include "compiler/messages.h"

/* Recovered diagnostic meanings, with native source/capacity wording. The
 * catalog is optional presentation: unknown entries retain numeric errors.
 * Emit while its code bank is selected; no bank-local pointer escapes. */
static const char *const explanations[101]={
    [1]="';' expected",
    [2]="':' expected",
    [3]="',' expected",
    [4]="'(' expected",
    [5]="')' expected",
    [6]="'=' expected",
    [7]="':=' expected",
    [8]="'[' expected",
    [9]="']' expected",
    [10]="'.' expected",
    [11]="'..' expected",
    [12]="BEGIN expected",
    [13]="DO expected",
    [14]="END expected",
    [15]="OF expected",
    [17]="THEN expected",
    [18]="TO or DOWNTO expected",
    [20]="Boolean expression expected",
    [21]="File variable expected",
    [22]="Integer constant expected",
    [23]="Integer expression expected",
    [24]="Integer variable expected",
    [25]="Integer or real constant expected",
    [26]="Integer or real expression expected",
    [27]="Integer or real variable expected",
    [28]="Pointer variable expected",
    [29]="Record variable expected",
    [30]="Simple type expected",
    [31]="Simple expression expected",
    [32]="String constant expected",
    [33]="String expression expected",
    [34]="String variable expected",
    [35]="Textfile expected",
    [36]="Type identifier expected",
    [37]="Untyped file expected",
    [40]="Undefined label",
    [41]="Unknown identifier or syntax error",
    [42]="Undefined pointer type in preceding type definitions",
    [43]="Duplicate identifier or label",
    [44]="Type mismatch",
    [45]="Constant out of range",
    [46]="Case constant and selector types differ",
    [47]="Operand type(s) does not match operator",
    [48]="Invalid result type",
    [49]="Invalid String length",
    [50]="String constant length does not match type",
    [51]="Invalid subrange base type",
    [52]="Lower bound > upper bound",
    [53]="Reserved word",
    [54]="Illegal assignment",
    [55]="String constant exceeds line",
    [56]="Error in integer constant",
    [57]="Error in real constant",
    [58]="Illegal character in identifier",
    [60]="Constants are not allowed here",
    [61]="Files and pointers are not allowed here",
    [62]="Structured variables are not allowed here",
    [63]="Textfiles are not allowed here",
    [64]="Textfiles and untyped files are not allowed here",
    [65]="Untyped files are not allowed here",
    [66]="I/O are not allowed",
    [67]="Files must be variable parameters",
    [68]="File components can not be files",
    [69]="Invalid ordering of fields",
    [70]="Set base type out of range",
    [71]="Invalid GOTO",
    [72]="Label not within current block",
    [73]="Undefined FORWARD procedure(s)",
    [74]="INLINE error",
    [75]="Illegal use of ABSOLUTE",
    [77]="Overlays unavailable in Memory mode",
    [90]="Source file unavailable",
    [91]="Unexpected end of source",
    [93]="Invalid compiler directive",
    [97]="Too many nested WITH's",
    [98]="Memory overflow",
    [99]="Compiler overflow",
    [100]="Source line exceeds 127 bytes",
};

void compiler_message(uint8_t code) SOURCE_BLOCK_CALL {
    const char *text;
#ifdef ZP_UNITS_EXPERIMENT
    if(code==247){output(": Cyclic unit dependency");return;}
    {static const char *const unit_messages[]={
        "Invalid compiled unit", "Unsupported unit feature",
        "Compiled unit file unavailable", "Unit runtime profile changed; recompile unit",
        "Ambiguous unit export; qualify its name", "Unit interface/implementation mismatch"};
    if(code>=248&&code<=253){output(": ");output(unit_messages[code-248]);return;}}
#endif
    if(code>=sizeof(explanations)/sizeof(explanations[0]))return;
    text=explanations[code];
    if(text) {output(": ");output(text);}
}
