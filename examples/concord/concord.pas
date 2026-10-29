program Concordance;
{$R+}
{$X-}
const limit=64;
type entry=record word:string[20];hits:integer end;
var table:array[1..limit]of entry;source:text;
    path:string[127];token:string[20];ch:char;
    used,i,j,best,status:integer;words,characters,lines:real;
    inword,overflow,longword,any,lastlf:boolean;
procedure store;
var k:integer;found:boolean;
begin
  words:=words+1.0;k:=1;found:=false;
  while(k<=used)and not found do begin
    if table[k].word=token then found:=true else k:=k+1
  end;
  if found then begin
    if table[k].hits<30000 then table[k].hits:=table[k].hits+1
    else overflow:=true
  end else if used<limit then begin
    used:=used+1;table[used].word:=token;table[used].hits:=1
  end else overflow:=true;
  token:='';inword:=false
end;
begin
  writeln('Concordance - count the words in a text file.');
  write('File [sample.txt]> ');if eof(Input)then halt;readln(Input,path);
  if path=''then path:='sample.txt';
  {$I-}assign(source,path);status:=ioresult;
  if status=0 then begin reset(source);status:=ioresult end;
  if status<>0 then begin writeln('Cannot open file: ',status);halt end;
  used:=0;words:=0.0;characters:=0.0;lines:=0.0;
  token:='';inword:=false;overflow:=false;longword:=false;any:=false;lastlf:=false;
  while not eof(source)do begin
    read(source,ch);status:=ioresult;
    if status<>0 then begin writeln('Read error: ',status);close(source);halt end;
    any:=true;characters:=characters+1.0;lastlf:=ch=#10;
    if lastlf then lines:=lines+1.0;
    ch:=upcase(ch);
    if(ch>='A')and(ch<='Z')then begin
      inword:=true;
      if length(token)<20 then token:=token+ch else longword:=true
    end else if inword then store
  end;
  status:=ioresult;
  if status<>0 then begin writeln('Read error: ',status);close(source);halt end;
  if inword then store;
  if any and not lastlf then lines:=lines+1.0;
  close(source);status:=ioresult;
  if status<>0 then begin writeln('Close error: ',status);halt end;
  writeln('Lines: ',lines:0:0,'  Words: ',words:0:0,'  Characters: ',characters:0:0);
  writeln('Most frequent words (alphabetical ties):');
  for j:=1 to 10 do begin
    best:=0;
    for i:=1 to used do
      if table[i].hits>0 then begin
        if best=0 then best:=i
        else if table[i].hits>table[best].hits then best:=i
        else if table[i].hits=table[best].hits then
          if table[i].word<table[best].word then best:=i
      end;
    if best<>0 then begin
      writeln(table[best].word:20,' ',table[best].hits:5);table[best].hits:=0
    end
  end;
  if overflow then writeln('Table/count limit reached: frequencies are partial.');
  if longword then writeln('Words longer than 20 letters share their first 20 letters.')
end.
