program Sets;
type colors=(red,green,blue);palette=set of colors;
     full=set of byte;
var a,b:palette;s,t:full;n:integer;
begin
 a:=[red,blue];b:=[green..blue];
 if (red in a) and not (green in a) then writeln('SET-ENUM-MEMBER-OK');
 if a+b=[red..blue] then writeln('SET-UNION-OK');
 if a*b=[blue] then writeln('SET-INTERSECTION-OK');
 if a-b=[red] then writeln('SET-DIFFERENCE-OK');
 if ([red]<=a) and (a>=[blue]) and (a<>b) and (a=a) then writeln('SET-RELATIONS-OK');
 if ([]<=a) and (a>=[]) and ([]+a=a) and (a*[]=[]) then writeln('SET-EMPTY-POLYMORPHIC-OK');
 s:=[0..255];t:=[0,255];
 if (0 in s) and (255 in s) and (t<=s) and not (s<=t) then writeln('SET-FULL-UNIVERSE-OK');
 s:=[3,3,5..3];if s=[3] then writeln('SET-DUPLICATE-REVERSED-OK');
 {$R+}n:=256;s:=[n,-1];
 if (0 in s) and (255 in s) and not (-1 in s) and not (256 in s) then writeln('SET-CONSTRUCTOR-LOW-BYTE-OK');
 s:=[250..258];if s=[] then writeln('SET-WRAPPED-RANGE-OK');
end.
