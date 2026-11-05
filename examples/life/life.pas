{
  LIFE - Conway's Life on a 20 by 10 toroidal grid.

  Two grids keep each generation independent of its update order.
  G and B select a glider and blinker; a number advances generations.
}

program Life;

{$R+}

const
  width = 20;
  height = 10;

type
  grid = array[1..height, 1..width] of boolean;

var
  world, next: grid;
  x, y, dx, dy, nx, ny, neighbors, population: integer;
  generation, steps, step, code, i: integer;
  cmd: string[40];
  quit: boolean;

procedure Clear;
begin
  for y := 1 to height do
    for x := 1 to width do
      world[y, x] := False;
  generation := 0
end;

procedure Pattern(glider: boolean);
begin
  Clear;
  if glider then
  begin
    world[2, 3] := True;
    world[3, 4] := True;
    world[4, 2] := True;
    world[4, 3] := True;
    world[4, 4] := True
  end
  else
  begin
    world[5, 9] := True;
    world[5, 10] := True;
    world[5, 11] := True
  end
end;

procedure Show;
begin
  population := 0;
  for y := 1 to height do
    for x := 1 to width do
      if world[y, x] then
        population := population + 1;
  WriteLn('Generation ', generation, '  Population ', population);
  for y := 1 to height do
  begin
    for x := 1 to width do
      if world[y, x] then
        Write('O')
      else
        Write('.');
    WriteLn
  end
end;

{ Read only world while writing next, then copy the completed generation.
  Both edges wrap, so there are no special dead boundary cells. }
procedure Evolve;
begin
  for y := 1 to height do
    for x := 1 to width do
    begin
      neighbors := 0;
      for dy := -1 to 1 do
        for dx := -1 to 1 do
          if (dx <> 0) or (dy <> 0) then
          begin
            nx := x + dx;
            ny := y + dy;
            if nx = 0 then
              nx := width;
            if nx > width then
              nx := 1;
            if ny = 0 then
              ny := height;
            if ny > height then
              ny := 1;
            if world[ny, nx] then
              neighbors := neighbors + 1
          end;

      { A dead cell needs three neighbours; a live cell keeps two or three. }
      next[y, x] := (neighbors = 3) or (world[y, x] and (neighbors = 2))
    end;
  world := next;
  generation := generation + 1
end;

begin
  WriteLn('Life - a 20 by 10 wrapping world. ? for help.');
  Pattern(True);
  Show;
  quit := False;
  repeat
    Write('> ');
    if Eof(Input) then
      cmd := 'Q'
    else
      ReadLn(Input, cmd);
    for i := 1 to Length(cmd) do
      cmd[i] := UpCase(cmd[i]);
    if cmd = 'Q' then
      quit := True
    else if (cmd = '?') then
      WriteLn('Enter: step, 1..100: advance, G: glider, B: blinker, Q: quit')
    else if (cmd = 'G') or (cmd = 'B') then
    begin
      Pattern(cmd = 'G');
      Show
    end
    else
    begin
      steps := 1;
      code := 0;
      if cmd <> '' then
        Val(cmd, steps, code);
      if (code <> 0) or (steps < 1) or (steps > 100) then
        WriteLn('Enter 1..100, G, B, ? or Q.')
      else if generation > 30000 - steps then
        WriteLn('Generation limit. Reset with G or B.')
      else
      begin
        for step := 1 to steps do
          Evolve;
        Show
      end
    end
  until quit
end.
