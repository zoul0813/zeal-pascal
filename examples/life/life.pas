program Life;
{$R+}
const width=20;height=10;
type grid=array[1..height,1..width]of boolean;
var world,next:grid;x,y,dx,dy,nx,ny,neighbors,population:integer;
    generation,steps,step,code,i:integer;cmd:string[40];quit:boolean;
procedure clear;
begin
  for y:=1 to height do for x:=1 to width do world[y,x]:=false;
  generation:=0
end;
procedure pattern(glider:boolean);
begin
  clear;
  if glider then begin
    world[2,3]:=true;world[3,4]:=true;
    world[4,2]:=true;world[4,3]:=true;world[4,4]:=true
  end else begin world[5,9]:=true;world[5,10]:=true;world[5,11]:=true end
end;
procedure show;
begin
  population:=0;
  for y:=1 to height do for x:=1 to width do
    if world[y,x]then population:=population+1;
  writeln('Generation ',generation,'  Population ',population);
  for y:=1 to height do begin
    for x:=1 to width do if world[y,x]then write('O')else write('.');
    writeln
  end
end;
procedure evolve;
begin
  for y:=1 to height do for x:=1 to width do begin
    neighbors:=0;
    for dy:=-1 to 1 do for dx:=-1 to 1 do
      if(dx<>0)or(dy<>0)then begin
        nx:=x+dx;ny:=y+dy;
        if nx=0 then nx:=width;if nx>width then nx:=1;
        if ny=0 then ny:=height;if ny>height then ny:=1;
        if world[ny,nx]then neighbors:=neighbors+1
      end;
    next[y,x]:=(neighbors=3)or(world[y,x]and(neighbors=2))
  end;
  world:=next;generation:=generation+1
end;
begin
  writeln('Life - a 20 by 10 wrapping world. ? for help.');
  pattern(true);show;quit:=false;
  repeat
    write('> ');if eof(Input)then cmd:='Q'else readln(Input,cmd);
    for i:=1 to length(cmd)do cmd[i]:=upcase(cmd[i]);
    if cmd='Q'then quit:=true
    else if(cmd='?')then writeln('Enter: step, 1..100: advance, G: glider, B: blinker, Q: quit')
    else if(cmd='G')or(cmd='B')then begin pattern(cmd='G');show end
    else begin
      steps:=1;code:=0;if cmd<>''then val(cmd,steps,code);
      if(code<>0)or(steps<1)or(steps>100)then writeln('Enter 1..100, G, B, ? or Q.')
      else if generation>30000-steps then writeln('Generation limit. Reset with G or B.')
      else begin for step:=1 to steps do evolve;show end
    end
  until quit
end.
