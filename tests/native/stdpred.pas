program StandardPredicates;
type box=record tag:integer;c:char end;
var x:box;checks,saved,code:integer;s:string[20];
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('PRED-FAIL-',checks);halt end end;
function ended(var f:text):boolean;
begin ended:=eoln(f)end;
function defaultready:boolean;
var Input:integer;
begin Input:=7;defaultready:=(not eof)and(Input=7)end;
begin
  saved:=stackptr;x.tag:=123;
  with x do begin
    {$I-}close(Output);check(not eof);code:=ioresult;{$I+}
    check((code=$22)and(tag=123));
    check(not eof);check(not eof(Input));check(not eoln);check(not ended(Input));
    check(not seekeof);check(not seekeoln(Input));
    read(Input,c);check(c='A');check(not eoln(Input));
    check(not seekeoln);read(Input,c);check(c='b');
    check(seekeoln);check(eoln);check(not eof);read(Input,c);check(c=#13);
    readln(Input);
    check(eoln(Input));check(not eof(Input));check(seekeoln(Input));readln(Input);
    check(not seekeof(Input));read(Input,c);check(c='Z');
    check(seekeoln);readln(Input);
    check(defaultready);{$B-}readln(s);check(s='minus');{$B+}
    check(not eof);readln(s);check(s='fresh');
    check(not eof);read(Input,s);check(s='end');
    check(eof);check(eoln);check(seekeof);check(seekeoln);
    read(Input,c);check(c=#26);check(eof(Input));check(ended(Input));
    check(tag=123)
  end;
  check(stackptr=saved);writeln('STD-PREDICATES-OK');
end.
