{
  CALCULATOR - Recursive-descent expression evaluator.

  Accepts arithmetic, parentheses and ANS. A failed expression leaves
  the previous answer intact. Input is limited to 72 characters.
}

program Calculator;

{$R+}
{$A-}
{ Preserve each recursive call's local operands. }

type
  line = string[72];
  message = string[48];

var
  source: line;
  at, depth, errorat, i: integer;
  problem: message;
  answer, value: real;
  quit: boolean;

function Current: char;
begin
  if at > Length(source) then
    Current := #0
  else
    Current := source[at]
end;

procedure Spaces;
begin
  while (Current = ' ') or (Current = #9) do
    at := at + 1
end;

{ Retain the first error so later parsing does not move its caret. }
procedure Fail(where: integer; why: message);
begin
  if errorat = 0 then
  begin
    errorat := where;
    problem := why
  end
end;

{ Bound literals and intermediate results before further arithmetic. }
function Bounded(number: real; where: integer): real;
begin
  if Abs(number) > 1000000000.0 then
  begin
    Fail(where, 'Magnitude exceeds 1000000000');
    Bounded := 0.0
  end
  else
    Bounded := number
end;

{ Grammar: expression = terms; term = factors; factor = number,
  unary sign, ANS or a parenthesized expression. }
function Expression: real; forward;

{ Limit simultaneous factor calls, including unary signs, to eight. }
function Factor: real;
var
  result: real;
  start, code, digits: integer;
  sign: char;
  name: string[8];

begin
  result := 0.0;
  Spaces;
  start := at;
  depth := depth + 1;
  if depth > 8 then
    Fail(at, 'Too deeply nested (limit 8)');
  if errorat = 0 then
  begin
    if (Current = '+') or (Current = '-') then
    begin
      sign := Current;
      at := at + 1;
      result := Factor;
      if sign = '-' then
        result := -result
    end
    else if Current = '(' then
    begin
      at := at + 1;
      result := Expression;
      Spaces;
      if Current = ')' then
        at := at + 1
      else
        Fail(at, 'Missing )')
    end
    else if (Current >= '0') and (Current <= '9') or (Current = '.') then
    begin
      digits := 0;
      while ((Current >= '0') and (Current <= '9')) or (Current = '.') do
      begin
        if Current <> '.' then
          digits := digits + 1;
        at := at + 1
      end;
      if digits = 0 then
        Fail(start, 'Expected a number')
      else
      begin
        Val(Copy(source, start, at - start), result, code);
        if code <> 0 then
          Fail(start + code - 1, 'Invalid number')
        else
          result := Bounded(result, start)
      end
    end
    else if (Current >= 'A') and (Current <= 'Z') then
    begin
      name := '';
      while (Current >= 'A') and (Current <= 'Z') do
      begin
        name := name + Current;
        at := at + 1
      end;
      if name = 'ANS' then
        result := answer
      else
        Fail(start, 'Unknown name; use ANS')
    end
    else
      Fail(at, 'Expected a number or (')
  end;
  depth := depth - 1;
  Factor := result
end;

{ Multiplication and division bind more tightly than addition. }
function Term: real;
var
  result, right: real;
  op: char;
  where: integer;

begin
  result := Factor;
  Spaces;
  while (errorat = 0) and ((Current = '*') or (Current = '/')) do
  begin
    op := Current;
    where := at;
    at := at + 1;
    right := Factor;
    if errorat = 0 then
    begin
      if op = '*' then
        result := Bounded(result * right, where)
      else if right = 0.0 then
        Fail(where, 'Division by zero')
      else if Abs(right) < 0.000000001 then
        Fail(where, 'Divisor too small (min 0.000000001)')
      else
        result := Bounded(result / right, where)
    end;
    Spaces
  end;
  Term := result
end;

{ Evaluate equal-precedence operators from left to right. }
function Expression;
var
  result, right: real;
  op: char;
  where: integer;

begin
  result := Term;
  Spaces;
  while (errorat = 0) and ((Current = '+') or (Current = '-')) do
  begin
    op := Current;
    where := at;
    at := at + 1;
    right := Term;
    if errorat = 0 then
    begin
      if op = '+' then
        result := Bounded(result + right, where)
      else
        result := Bounded(result - right, where)
    end;
    Spaces
  end;
  Expression := result
end;

{ Formatting is kept out of the recursive parser's workspace. }
procedure Display(number: real);
var
  text: string[40];
  done: boolean;

begin
  if (number <> 0.0) and (Abs(number) < 0.000001) then
    WriteLn('= ', number)
  else
  begin
    Str(number:0:6, text);
    done := False;
    while not done do
    begin
      if text[Length(text)] = '0' then
        Delete(text, Length(text), 1)
      else
        done := True
    end;
    if text[Length(text)] = '.' then
      Delete(text, Length(text), 1);
    if text = '-0' then
      text := '0';
    WriteLn('= ', text)
  end
end;

begin
  WriteLn('Calculator: + - * / ( ) and ANS. Blank line quits.');
  answer := 0.0;
  quit := False;
  repeat
    Write('> ');
    BufLen := 72;
    if Eof(Input) then
      quit := True
    else
    begin
      ReadLn(Input, source);
      if source = '' then
        quit := True
      else
      begin
        for i := 1 to Length(source) do
          source[i] := UpCase(source[i]);
        at := 1;
        depth := 0;
        errorat := 0;
        problem := '';
        value := Expression;
        Spaces;
        if Current <> #0 then
          Fail(at, 'Unexpected character');

        { Commit ANS only after the entire expression has succeeded. }
        if errorat = 0 then
        begin
          answer := value;
          Display(value)
        end
        else
        begin
          WriteLn('  ', source);
          for i := 1 to errorat + 1 do
            Write(' ');
          WriteLn('^ ', problem)
        end
      end
    end
  until quit
end.
