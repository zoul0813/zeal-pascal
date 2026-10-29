program Errands;
{$R+}
type link=^task;
     task=record title:string[40];priority:integer;next:link end;
var head,p,previous,fresh:link;cmd,s:string[40];
    i,n,priority,code,count,baseline,available,usedbytes:integer;quit,measured:boolean;
procedure enqueue(node:link);
var current,before:link;done:boolean;
begin
  current:=head;before:=nil;done:=false;
  while(current<>nil)and not done do begin
    if current^.priority>node^.priority then done:=true
    else begin before:=current;current:=current^.next end
  end;
  node^.next:=current;
  if before=nil then head:=node else before^.next:=node
end;
procedure listing;
begin
  p:=head;n:=0;
  while p<>nil do begin
    n:=n+1;writeln(n:3,' [',p^.priority,'] ',p^.title);p:=p^.next
  end;
  writeln(n,' errands. Heap bytes used: ',usedbytes)
end;
begin
  writeln('Errands - a list that grows and shrinks. ? for help.');
  head:=nil;count:=0;baseline:=0;measured:=false;quit:=false;
  repeat
    write('> ');if eof(Input)then cmd:='Q'else readln(Input,cmd);
    { First input establishes the runtime conservative stack boundary. }
    if not measured then begin baseline:=memavail;measured:=true end;
    for i:=1 to length(cmd)do cmd[i]:=upcase(cmd[i]);
    if(cmd='?')or(cmd='')then writeln('A add, L list, D done, Q quit (list is in memory only)')
    else if cmd='L'then begin available:=memavail;usedbytes:=baseline-available;listing end
    else if cmd='A'then begin
      if(maxavail<sizeof(task))or(count=100)then writeln('List full.')
      else begin
        write('Priority 1..9 (1 first)> ');
        if eof(Input)then quit:=true else begin
          readln(Input,s);priority:=0;val(s,priority,code);
          if(s='')or(code<>0)or(priority<1)or(priority>9)then writeln('Enter 1..9. Add again.')
          else begin
            write('Task (40 chars)> ');
            if eof(Input)then quit:=true else begin
              readln(Input,s);
              if s=''then writeln('A task needs some text.')
              else begin
                new(fresh);fresh^.title:=s;fresh^.priority:=priority;
                enqueue(fresh);count:=count+1;writeln('Added.')
              end
            end
          end
        end
      end
    end
    else if cmd='D'then begin
      write('Task number from L> ');
      if eof(Input)then quit:=true else begin
        readln(Input,s);n:=0;val(s,n,code);
        if(s='')or(code<>0)or(n<1)or(n>count)then writeln('No such task.')
        else begin
          p:=head;previous:=nil;
          for i:=1 to n-1 do begin previous:=p;p:=p^.next end;
          if previous=nil then head:=p^.next else previous^.next:=p^.next;
          dispose(p);count:=count-1;writeln('Done.');available:=memavail;usedbytes:=baseline-available;listing
        end
      end
    end
    else if cmd='Q'then quit:=true
    else writeln('Unknown command. ? for help.')
  until quit;
  while head<>nil do begin p:=head;head:=head^.next;dispose(p)end;
  available:=memavail;writeln('Heap bytes after cleanup: ',baseline-available)
end.
