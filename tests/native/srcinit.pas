program Initializer;
const data:array[1..2]of integer=(
{$I source-parts/values.inc}
);
{$I source-parts/open.inc}
22);
{$I source-parts/after.inc}
begin
 if(data[1]<>11)or(data[2]<>22)or(split[1]<>11)or(split[2]<>22)then halt;
 writeln('SOURCE-INITIALIZER-OK')
end.
