program Lighthouse;
{$R+}
type room=(shore,cottage,cellar,lamp);
     item=(key,oil,wick,matches);
     belongings=set of item;
var place:room; bag:belongings; opened,won,quit:boolean;
    command:string[40];i:integer;
procedure look;
begin
  case place of
    shore:writeln('A storm batters the shore. N: cottage. U: lamp.');
    cottage:writeln('A cottage. Search here for a key and matches. S: shore. D: cellar.');
    cellar:writeln('The cellar stores oil and spare wicks. U: cottage.');
    lamp:writeln('The lighthouse lamp is cold. D: shore. Try LIGHT.')
  end
end;
procedure take(thing:item;where:room);
begin
  if place<>where then writeln('You cannot find that here.')
  else if thing in bag then writeln('You already have it.')
  else begin bag:=bag+[thing];writeln('Taken.')end
end;
begin
  place:=shore;bag:=[];opened:=false;won:=false;quit:=false;
  writeln('Lighthouse - bring the ships home. ? for help.');look;
  repeat
    write('> ');
    if eof(Input)then command:='Q'else readln(Input,command);
    for i:=1 to length(command)do command[i]:=upcase(command[i]);
    if(command='?')or(command='HELP')then
      writeln('N S U D, LOOK, TAKE KEY/OIL/WICK/MATCHES, OPEN, INV, LIGHT, Q')
    else if(command='LOOK')or(command='')then look
    else if command='INV'then begin
      write('Carrying: ');
      if key in bag then write('key ');
      if oil in bag then write('oil ');
      if wick in bag then write('wick ');
      if matches in bag then write('matches ');
      writeln
    end
    else if command='TAKE KEY'then take(key,cottage)
    else if command='TAKE MATCHES'then take(matches,cottage)
    else if command='TAKE OIL'then take(oil,cellar)
    else if command='TAKE WICK'then take(wick,cellar)
    else if command='OPEN'then begin
      if place<>cottage then writeln('No locked door here.')
      else if key in bag then begin opened:=true;writeln('The cellar door opens.')end
      else writeln('The cellar door needs a key.')
    end
    else if command='LIGHT'then begin
      if place<>lamp then writeln('You must reach the lamp first.')
      else if [oil,wick,matches]<=bag then begin
        won:=true;writeln('The beam sweeps the water. A ship turns toward harbour.')
      end else writeln('You need oil, a wick and matches.')
    end
    else if command='N'then begin
      if place=shore then begin place:=cottage;look end
      else writeln('No way north.')
    end
    else if command='S'then begin
      if place=cottage then begin place:=shore;look end
      else writeln('No way south.')
    end
    else if command='D'then begin
      if place=lamp then begin place:=shore;look end
      else if place=cottage then begin
        if opened then begin place:=cellar;look end
        else writeln('The cellar door is locked.')
      end else writeln('No way down.')
    end
    else if command='U'then begin
      if place=shore then begin place:=lamp;look end
      else if place=cellar then begin place:=cottage;look end
      else writeln('No way up.')
    end
    else if(command='Q')or(command='QUIT')then quit:=true
    else writeln('Unknown command. ? for help.')
  until won or quit
end.
