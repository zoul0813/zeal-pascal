program PartsBin;
{$R+}
{$I-}
type line=string[40];
     part=record code:string[6];name:string[20];qty:integer end;
var stock:file of part;p:part;s,cmd:line;wanted:string[6];
    count,slot,i,amount,error:integer;found,quit:boolean;
procedure checked;
var status:integer;
begin
  status:=ioresult;
  if status<>0 then begin writeln('I/O ',status,'. Inspect parts.dat.');halt end
end;
procedure inputline;
begin {$I+}if eof(Input)then halt;readln(Input,s);{$I-}end;
procedure fetch;
begin
  seek(stock,slot);checked;read(stock,p);checked;
  if(p.code='')or(p.qty<0)or(p.qty>30000)then begin writeln('Bad record. Restore backup.');halt end
end;
procedure put;
begin seek(stock,slot);checked;write(stock,p);checked;flush(stock);checked end;
procedure locate;
begin
  write('Code> ');inputline;wanted:=s;
  for i:=1 to length(wanted)do wanted[i]:=upcase(wanted[i]);
  slot:=0;found:=false;
  while(slot<count)and not found do begin
    fetch;if p.code=wanted then found:=true else slot:=slot+1
  end
end;
procedure row;
begin writeln(p.code:6,' ',p.name:20,' ',p.qty:5)end;
begin
  writeln('Parts Bin - A add, L list, U update, Q quit.');
  assign(stock,'parts.dat');checked;reset(stock);error:=ioresult;
  if error=1 then begin
    write('Create parts.dat? Y/N> ');inputline;
    if(s<>'Y')and(s<>'y')then halt;
    rewrite(stock);checked
  end else if error<>0 then begin writeln('Open error ',error);halt end;
  count:=filesize(stock);checked;
  if(count<0)or(count>100)then begin writeln('Record limit: 100.');halt end;
  quit:=false;
  repeat
    write('> ');inputline;cmd:=s;
    for i:=1 to length(cmd)do cmd[i]:=upcase(cmd[i]);
    if cmd='L'then begin
      writeln('  Code                 Name   Qty');
      for slot:=0 to count-1 do begin fetch;row end
    end
    else if cmd='A'then begin
      if count=100 then writeln('Full (100 records).')
      else begin
        locate;
        if found then writeln('Duplicate code.')
        else if wanted=''then writeln('Code required.')
        else begin
          p.code:=wanted;write('Name> ');inputline;p.name:=s;p.qty:=0;
          slot:=count;put;count:=count+1;writeln('Added ',p.code,'.')
        end
      end
    end
    else if cmd='U'then begin
      locate;
      if not found then writeln('Code not found.')
      else begin
        write('Change (-30000..30000)> ');inputline;amount:=0;val(s,amount,error);
        if(s='')or(error<>0)or(amount< -30000)or(amount>30000)then writeln('Invalid change.')
        else if(amount<0)and(p.qty< -amount)then writeln('Not enough stock.')
        else if(amount>0)and(p.qty>30000-amount)then writeln('Limit: 30000.')
        else begin p.qty:=p.qty+amount;put;row end
      end
    end
    else if cmd='Q'then quit:=true
    else writeln('A add, L list, U update, Q quit.')
  until quit;
  close(stock);checked
end.
