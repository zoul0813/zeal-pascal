{ Shared types, room descriptions, screen tables and game state. }

type
  screenline = string[77];
  room = (shore, cottage, cellar, lamp);
  item = (key, oil, wick, matches);
  belongings = set of item;
  stroke = record
    x, y, fg, bg: byte;
    text: string[12]
  end;
  action = (HelpCommand, LookCommand, InventoryCommand, KeyCommand,
            OilCommand, WickCommand, MatchesCommand, OpenCommand,
            LightCommand, NorthCommand, SouthCommand, DownCommand,
            UpCommand, QuitCommand, UnknownCommand);
  border = record
    y: byte;
    left, right: char
  end;
  commandentry = record
    name: string[12];
    code: action
  end;
  caption = record
    x, y, fg, bg: byte;
    text: string[66]
  end;

const
  padding = '                                        ';
  titles: array[room] of string[13] = (
    'THE SHORE',
    'THE COTTAGE',
    'THE CELLAR',
    'THE LAMP ROOM'
  );
  first: array[room] of string[44] = (
    'Rain lashes the rocks. Beyond the breakers,',
    'Salt-stained windows rattle in their frames.',
    'Under the stairs: oil and spare wicks.',
    'The great lens waits above the black water.'
  );
  second: array[room] of string[41] = (
    'a ship searches for the harbour light.',
    'Search here for a key and matches.',
    'The storm is a distant rumble overhead.',
    'Bring oil, a wick and matches. Try LIGHT.'
  );
  exits: array[room] of string[27] = (
    'N: cottage    U: lamp room',
    'S: shore      D: cellar',
    'U: cottage',
    'D: shore'
  );
  itemnames: array[item] of string[8] = ('Key', 'Oil', 'Wick', 'Matches');
  itemrows: array[item] of integer = (8, 10, 11, 9);

  ArtCount = 34;
  LabelCount = 10;
  scenery: array[1..ArtCount] of stroke = (
    { Weather }
    (x: 5; y: 13; fg: DarkGray; bg: Black;
     text: #176#177#178#178#177#176),
    (x: 37; y: 13; fg: DarkGray; bg: Black;
     text: #176#177#178#178#177#176),
    (x: 9; y: 14; fg: DarkGray; bg: Black;
     text: #176#177#176),
    (x: 7; y: 15; fg: LightBlue; bg: Black;
     text: '/  /'),
    (x: 41; y: 14; fg: LightBlue; bg: Black;
     text: '/  /'),
    (x: 14; y: 14; fg: LightBlue; bg: Black;
     text: '/'),
    { Tower }
    (x: 26; y: 13; fg: LightRed; bg: Black;
     text: #220#220),
    (x: 24; y: 14; fg: LightRed; bg: Black;
     text: #220#219#219#219#219#220),
    (x: 24; y: 15; fg: White; bg: Black;
     text: #218#196#196#196#196#191),
    (x: 24; y: 16; fg: White; bg: Black;
     text: #179'    '#179),
    (x: 25; y: 16; fg: DarkGray; bg: Blue;
     text: #177#176#176#177),
    (x: 23; y: 17; fg: White; bg: Black;
     text: #196#193#196#196#196#196#193#196),
    (x: 25; y: 18; fg: DarkGray; bg: LightGray;
     text: ' '#254' '#177),
    (x: 25; y: 19; fg: DarkGray; bg: LightGray;
     text: '  '#176#177),
    (x: 25; y: 20; fg: DarkGray; bg: Red;
     text: '  '#176#177),
    (x: 25; y: 21; fg: DarkGray; bg: Red;
     text: '  '#176#177),
    (x: 24; y: 22; fg: DarkGray; bg: LightGray;
     text: '   '#176#177#177),
    (x: 24; y: 23; fg: DarkGray; bg: LightGray;
     text: '   '#176#177#177),
    (x: 23; y: 24; fg: LightGray; bg: Brown;
     text: #220#219#219#219#219#219#219#220),
    { Rock }
    (x: 19; y: 24; fg: Brown; bg: Blue;
     text: #176#220#219#219),
    (x: 31; y: 24; fg: Brown; bg: Blue;
     text: #219#219#220#176),
    (x: 17; y: 25; fg: Brown; bg: Blue;
     text: #176#177#178#219#219#219#219#219#219#219#219#219),
    (x: 29; y: 25; fg: Brown; bg: Blue;
     text: #219#219#219#219#219#178#177#176),
    (x: 13; y: 26; fg: Brown; bg: Blue;
     text: #176#177#178#219#219#219#219#219#219#219#219#219),
    (x: 25; y: 26; fg: Brown; bg: Blue;
     text: #219#219#219#219#219#219#219#219#219#219#219#219),
    (x: 37; y: 26; fg: Brown; bg: Blue;
     text: #219#178#177#176),
    { Water }
    (x: 4; y: 18; fg: Cyan; bg: Blue;
     text: #196#196'   '#250#250),
    (x: 35; y: 20; fg: Cyan; bg: Blue;
     text: '~   ~  ~~'),
    (x: 4; y: 23; fg: LightBlue; bg: Blue;
     text: #247#247'   '#247),
    (x: 42; y: 24; fg: LightBlue; bg: Blue;
     text: #247#247#247),
    (x: 10; y: 25; fg: White; bg: Blue;
     text: '~  ~'),
    (x: 42; y: 26; fg: White; bg: Blue;
     text: '~'),
    { Ship }
    (x: 42; y: 17; fg: DarkGray; bg: Black;
     text: #220#219#220),
    (x: 40; y: 18; fg: DarkGray; bg: Blue;
     text: #223#219#219#219#219#219#223)
  );
  labels: array[1..LabelCount] of caption = (
    (x: 3; y: 2; fg: White; bg: Blue;
     text:
       'LIGHTHOUSE'),
    (x: 56; y: 6; fg: White; bg: Blue;
     text:
       'INVENTORY'),
    (x: 56; y: 15; fg: White; bg: Blue;
     text:
       'CELLAR DOOR'),
    (x: 56; y: 21; fg: White; bg: Blue;
     text:
       'THE LAMP'),
    (x: 17; y: 2; fg: LightCyan; bg: Blue;
     text:
       'THE LAST WATCH'),
    (x: 56; y: 2; fg: LightCyan; bg: Blue;
     text:
       'Bring the ship home.'),
    (x: 3; y: 3; fg: Cyan; bg: Black;
     text:
       'A stormbound harbour. One light can make the difference.'),
    (x: 3; y: 30; fg: LightGray; bg: Black;
     text:
       'TAKE KEY / MATCHES in cottage; TAKE OIL / WICK in cellar.'),
    (x: 3; y: 31; fg: LightGray; bg: Black;
     text:
       'OPEN unlocks the cellar. LIGHT works in the lamp room.'),
    (x: 3; y: 37; fg: Cyan; bg: Black;
     text:
       'N S U D: move   LOOK: examine   INV: inventory   ?: help   Q: quit')
  );
  borders: array[1..5] of border = (
    (y: 1; left: #218; right: #191),
    (y: 4; left: #195; right: #180),
    (y: 27; left: #195; right: #180),
    (y: 36; left: #195; right: #180),
    (y: 39; left: #192; right: #217)
  );
  CommandCount = 17;
  commands: array[1..CommandCount] of commandentry = (
    (name: '?'; code: HelpCommand),
    (name: 'HELP'; code: HelpCommand),
    (name: ''; code: LookCommand),
    (name: 'LOOK'; code: LookCommand),
    (name: 'INV'; code: InventoryCommand),
    (name: 'TAKE KEY'; code: KeyCommand),
    (name: 'TAKE OIL'; code: OilCommand),
    (name: 'TAKE WICK'; code: WickCommand),
    (name: 'TAKE MATCHES'; code: MatchesCommand),
    (name: 'OPEN'; code: OpenCommand),
    (name: 'LIGHT'; code: LightCommand),
    (name: 'N'; code: NorthCommand),
    (name: 'S'; code: SouthCommand),
    (name: 'D'; code: DownCommand),
    (name: 'U'; code: UpCommand),
    (name: 'Q'; code: QuitCommand),
    (name: 'QUIT'; code: QuitCommand)
  );

var
  place, shownroom: room;
  choice: action;
  bag, shownbag: belongings;
  opened, won, quit, painted, showndoor: boolean;
  command: string[40];
  raw: string[127];
  terminator: char;
  message, shownmessage: screenline;
  messagecolor, showncolor: byte;
  shownlamp, i: integer;

